#define INITGUID
#include "PartitionManager.h"

#define AssertLogged(result, action) { if (result != S_OK) { action; }};
#define AssertPrinted(result, action) { if (result != S_OK) { wprintf(L"Failed (0x%08X)\n", result); action; } };

#ifdef _DEBUG
#define DebugAction(string) {string};
#else
#define DebugAction(string) {};
#endif

#ifdef _DEBUG
#define Assert(result, action) AssertPrinted(result, action)
#else
#define Assert(result, action) AssertLogged(result, action)
#endif

#define ObjectNameInformation (OBJECT_INFORMATION_CLASS)1
#define SafeRelease(x) {if (NULL != x) { x->Release(); x = NULL; } }
#define SafeCoFree(x) {if (NULL != x) { CoTaskMemFree(x); x = NULL; } }

#include <winternl.h>
#include <vds.h>

HRESULT PartitionManager::FormatAndOrMountPartition(PartitionInformation* partition, const wchar_t* fileSystem, const wchar_t* mountPoint)
{
	if (!fileSystem && !mountPoint)
		return ERROR_INVALID_PARAMETER;

	DebugAction(wprintf(L"Starting %s%s%s operation for partition %d on disk %d.\n", fileSystem ? L"format" : L"", fileSystem && mountPoint ? L" and " : L"", mountPoint ? L"mount" : L"", partition->PartitionNumber, partition->DiskNumber));
	if (fileSystem) DebugAction(wprintf(L"Target file system: %s.\n", fileSystem));
	if (mountPoint) DebugAction(wprintf(L"Target mount point: %s.\n", mountPoint));

	wchar_t requestedDisk[MAX_PATH];
	swprintf_s(requestedDisk, L"\\\\.\\GLOBALROOT\\Device\\Harddisk%d\\Partition0", partition->DiskNumber);
	HANDLE hFile = CreateFileW(requestedDisk, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	Assert(GetLastError(), return GetLastError());

	char infoBuffer[512];
	DWORD bytesReceived;
	NTSTATUS res = NtQueryObject(hFile, ObjectNameInformation, &infoBuffer, 512, &bytesReceived);
	CloseHandle(hFile);
	Assert(res, return res);
	wcscpy_s(requestedDisk, ((UNICODE_STRING*)infoBuffer)->Buffer);

	DebugAction(wprintf(L"Target disk path: %s.\n", requestedDisk));

	HRESULT hResult;
	HRESULT asyncRes;
	ULONG ulFetchCount;

	bool formatComplete = false;
	bool mountComplete = false;

	VDS_DISK_PROP diskProperties;
	VDS_DISK_EXTENT* diskExtents = NULL;
	wchar_t** accessPaths = NULL;
	VDS_ASYNC_OUTPUT asyncOut;

	IVdsServiceLoader* pLoader = NULL;
	IVdsService* pService = NULL;
	IVdsSwProvider* pProvider = NULL;
	IVdsPack* pPack = NULL;
	IVdsDisk* pDisk = NULL;
	IVdsAdvancedDisk* pAdvDisk = NULL;
	IVdsDiskPartitionMF* pDiskMF = NULL;
	IVdsVolumeMF* pVolumeMF = NULL;
	IVdsVolumeMF2* pVolumeMF2 = NULL;
	IVdsAsync* pAsync = NULL;

	IUnknown* pUnknown = NULL;
	IEnumVdsObject* pEnumVdsSwProviders = NULL;
	IEnumVdsObject* pEnumVdsPacks = NULL;
	IEnumVdsObject* pEnumVdsDisks = NULL;

	// Initilize COM and IVdsLoader
	DebugAction(wprintf(L"Connecting to COM..."));
	hResult = CoInitialize(NULL);
	Assert(hResult, return hResult);
	hResult = CoCreateInstance(CLSID_VdsLoader, NULL, CLSCTX_LOCAL_SERVER, IID_IVdsServiceLoader, (void**)&pLoader);
	Assert(hResult, goto releaseCOM);
	DebugAction(wprintf(L"Success.\n"));

	// Connect to IVdsService
	DebugAction(wprintf(L"Connecting to VDS service..."));
	hResult = pLoader->LoadService(NULL, &pService);
	SafeRelease(pLoader);
	Assert(hResult, goto releaseCOM);
	hResult = pService->WaitForServiceReady();
	Assert(hResult, goto releaseCOM);
	DebugAction(wprintf(L"Success.\n"));

	// Refresh VDS data
	DebugAction(wprintf(L"Refreshing data..."));
	hResult = pService->Reenumerate();
	Assert(hResult, goto releaseCOM);
	hResult = pService->Refresh();
	Assert(hResult, goto releaseCOM);
	DebugAction(wprintf(L"Success.\n"));

	// Query through all software providers
	DebugAction(wprintf(L"Querying software providers..."));
	hResult = pService->QueryProviders(VDS_QUERY_SOFTWARE_PROVIDERS, &pEnumVdsSwProviders);
	Assert(hResult, goto releaseCOM);
	DebugAction(wprintf(L"Success.\n\n"));

	for (int swpIndex = 0; (hResult = pEnumVdsSwProviders->Next(1, &pUnknown, &ulFetchCount)) == S_OK; swpIndex++)
	{
		hResult = pUnknown->QueryInterface(&pProvider);
		SafeRelease(pUnknown);
		Assert(hResult, continue);

		DebugAction(wprintf(L"Querying packs for Software Provider #%d...", swpIndex));

		// Query through all packs
		hResult = pProvider->QueryPacks(&pEnumVdsPacks);
		SafeRelease(pProvider);
		Assert(hResult, continue);
		DebugAction(wprintf(L"Success.\n"));

		for (int packIndex = 0; (hResult = pEnumVdsPacks->Next(1, &pUnknown, &ulFetchCount)) == S_OK; packIndex++)
		{
			hResult = pUnknown->QueryInterface(&pPack);
			SafeRelease(pUnknown);
			Assert(hResult, continue);

			DebugAction(wprintf(L"  Querying disks for Pack #%d...", packIndex));

			// Query through all disks
			hResult = pPack->QueryDisks(&pEnumVdsDisks);
			SafeRelease(pPack);
			Assert(hResult, continue);
			DebugAction(wprintf(L"Success.\n"));

			for (int diskIndex = 0; (hResult = pEnumVdsDisks->Next(1, &pUnknown, &ulFetchCount)) == S_OK; diskIndex++)
			{
				hResult = pUnknown->QueryInterface(&pDisk);
				SafeRelease(pUnknown);
				Assert(hResult, goto releaseDisk);

				DebugAction(wprintf(L"    Getting properties for Disk #%d...", diskIndex));

				// Determine if the disk contains the target
				hResult = pDisk->GetProperties(&diskProperties);
				Assert(hResult, goto releaseDisk);
				DebugAction(wprintf(L"Success.\n"));

				hFile = CreateFileW(diskProperties.pwszName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
				hResult = GetLastError();
				Assert(hResult, goto releaseDisk);
				res = NtQueryObject(hFile, ObjectNameInformation, &infoBuffer, 512, &bytesReceived);
				CloseHandle(hFile);
				Assert(res, goto releaseDisk);

				DebugAction(wprintf(L"    The path of the disk is %s.\n", ((UNICODE_STRING*)infoBuffer)->Buffer));

				if (lstrcmpW(requestedDisk, ((UNICODE_STRING*)infoBuffer)->Buffer))
					goto releaseDisk;

				// From this point, no branch should continue iterating over disks
				DebugAction(wprintf(L"\nFound the target disk.\n"));

				// Query through extents
				hResult = pDisk->QueryExtents(&diskExtents, (PLONG)&ulFetchCount);
				Assert(hResult, goto releaseCOM);

				for (int i = 0; i < ulFetchCount; i++)
				{
					// Test if extent matches the partition offset
					if (diskExtents[i].ullOffset != partition->StartLBA.ULL * CurrentDisk.SectorSize)
						continue;

					DebugAction(wprintf(L"Found the target partition.\n"));
					wprintf(L"         Extent #%d:\n", i);
					wprintf(L"            Type: %d\n", diskExtents[i].type);
					wprintf(L"            Offset: %llu\n", diskExtents[i].ullOffset);
					wprintf(L"            Size: %llu\n", diskExtents[i].ullSize);
					wprintf(L"            Volume GUID: {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
						diskExtents[i].volumeId.Data1, diskExtents[i].volumeId.Data2, diskExtents[i].volumeId.Data3,
						diskExtents[i].volumeId.Data4[0], diskExtents[i].volumeId.Data4[1], diskExtents[i].volumeId.Data4[2], diskExtents[i].volumeId.Data4[3],
						diskExtents[i].volumeId.Data4[4], diskExtents[i].volumeId.Data4[5], diskExtents[i].volumeId.Data4[6], diskExtents[i].volumeId.Data4[7]);

					// Test if volumeId is not null
					const GUID GUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
					if (memcmp(&GUID_NULL, &diskExtents[i].volumeId, sizeof(GUID)) == 0)
					{
						// If it is, check if it is an ESP/OEM volume
						if (diskExtents[i].type == VDS_DET_ESP ||
							diskExtents[i].type == VDS_DET_OEM)
						{
							// OEM volumes can only be assigned to a drive letter
							if (mountPoint && lstrlenW(mountPoint) > 1)
							{
								hResult = ERROR_NOT_SUPPORTED;
								goto releaseCOM;
							}

							goto oemOrESP;
						}
					}

					// Get IVdsVolumeMF2 object
					DebugAction(wprintf(L"Found the target volume.\n"));
					hResult = pService->GetObjectW(diskExtents[i].volumeId, VDS_OT_VOLUME, &pUnknown);
					Assert(hResult, goto releaseCOM);
					hResult = pUnknown->QueryInterface(&pVolumeMF2);
					SafeRelease(pUnknown);
					Assert(hResult, goto releaseCOM);

					// Start the formatting
					if (fileSystem)
					{
						DebugAction(wprintf(L"Formatting the target volume..."));
						wchar_t fsNameBuffer[10];
						wcscpy_s(fsNameBuffer, fileSystem);
						wchar_t fsLabelBuffer[10];
						wcscpy_s(fsLabelBuffer, L"");
						hResult = pVolumeMF2->FormatEx(fsNameBuffer, 0, 0, fsLabelBuffer, true, true, false, &pAsync);

						Assert(hResult, goto releaseCOM);
						asyncRes = pAsync->Wait(&hResult, &asyncOut);
						Assert(asyncRes, hResult = asyncRes; goto releaseCOM);
						Assert(hResult, goto releaseCOM);
						formatComplete = true;
						DebugAction(wprintf(L"Success.\n"));
						if (!mountPoint) goto releaseCOM;
					}

					// Start mounting
					hResult = pVolumeMF2->QueryInterface(&pVolumeMF);
					SafeRelease(pVolumeMF2);
					Assert(hResult, goto releaseCOM);

					// For some reason on MBR disks it takes longer for QueryAccessPaths to become
					// available after formatting. So we just retry a couple of times.

					DebugAction(wprintf(L"Querying and unmounting existing volume mount points..."));
					for (int i = 0; i < 10; i++)
					{
						hResult = pVolumeMF->QueryAccessPaths(&accessPaths, (PLONG)&ulFetchCount);
						if (hResult == S_OK) break;
						Sleep(1000);
					}
					Assert(hResult, goto releaseCOM);
					for (int i = 0; i < ulFetchCount; i++)
						if (lstrlenW(accessPaths[i]) == 3)
						{
							hResult = pVolumeMF->DeleteAccessPath(accessPaths[i], true);
							Assert(hResult, goto releaseCOM);
						}
					hResult = pVolumeMF->Dismount(TRUE, TRUE);
					Assert(hResult, goto releaseCOM);
					DebugAction(wprintf(L"Success.\n"));

					wchar_t mountBuffer[MAX_PATH];
					wcscpy_s(mountBuffer, mountPoint);
					if (lstrlenW(mountBuffer) == 1)
						wcscat_s(mountBuffer, L":\\");
					DebugAction(wprintf(L"Mounting the target volume to %s...", mountBuffer));
					hResult = pVolumeMF->AddAccessPath(mountBuffer);
					if (hResult == S_FALSE) hResult = S_OK;
					Assert(hResult, goto releaseCOM);
					mountComplete = true;
					DebugAction(wprintf(L"Success.\n"));
					goto releaseCOM;
				}

				// The partition does not exist
				hResult = VDS_E_OBJECT_NOT_FOUND;
				goto releaseCOM;

			oemOrESP:
				// The partition is an OEM or ESP partition
				DebugAction(wprintf(L"The partition is an OEM partition or is the EFI System Partition.\n"));

				hResult = pDisk->QueryInterface(&pDiskMF);
				SafeRelease(pDisk);
				Assert(hResult, goto releaseCOM);

				if (fileSystem) {
					DebugAction(wprintf(L"Formatting the target partition..."));
					wchar_t fsNameBuffer[10];
					wcscpy_s(fsNameBuffer, fileSystem);
					wchar_t fsLabelBuffer[10];
					wcscpy_s(fsLabelBuffer, L"");
					pDiskMF->FormatPartitionEx(partition->StartLBA.ULL * CurrentDisk.SectorSize, fsNameBuffer, 0, 0, fsLabelBuffer, true, true, false, &pAsync);

					Assert(hResult, goto releaseCOM);
					asyncRes = pAsync->Wait(&hResult, &asyncOut);
					Assert(asyncRes, hResult = asyncRes; goto releaseCOM);
					Assert(hResult, goto releaseCOM);
					formatComplete = true;
					DebugAction(wprintf(L"Success.\n"));
				}
				if (!mountPoint) goto releaseCOM;

				hResult = pDiskMF->QueryInterface(&pAdvDisk);
				SafeRelease(pDiskMF);
				Assert(hResult, goto releaseCOM);

				{
					wchar_t letter = 0;
					DebugAction(wprintf(L"Querying existing partition drive letter..."));
					hResult = pAdvDisk->GetDriveLetter(partition->StartLBA.ULL * CurrentDisk.SectorSize, &letter);
					Assert(hResult, goto releaseCOM);
					if (letter != 0)
					{
						DebugAction(wprintf(L"Success.\nUmmounting partition from drive letter %c:\\...", letter));
						hResult = pAdvDisk->DeleteDriveLetter(partition->StartLBA.ULL * CurrentDisk.SectorSize, letter);
					}
					else DebugAction(wprintf(L"Success.\nThe partition has no existing drive letter.\n"));
				}

				DebugAction(wprintf(L"Mounting the target partition to %s...", mountPoint));
				hResult = pAdvDisk->AssignDriveLetter(partition->StartLBA.ULL * CurrentDisk.SectorSize, *mountPoint);
				Assert(hResult, goto releaseCOM);
				mountComplete = true;
				DebugAction(wprintf(L"Success.\n"));
				goto releaseCOM;

			releaseDisk:
				SafeRelease(pDisk);
			}
		}
	}

	// The disk was not found
	hResult = VDS_S_DISK_IS_MISSING;

releaseCOM:
	SafeCoFree(diskExtents);
	SafeCoFree(accessPaths);
	SafeRelease(pEnumVdsSwProviders);
	SafeRelease(pEnumVdsPacks);
	SafeRelease(pEnumVdsDisks);
	SafeRelease(pAsync);
	SafeRelease(pVolumeMF2);
	SafeRelease(pVolumeMF);
	SafeRelease(pDiskMF);
	SafeRelease(pAdvDisk);
	SafeRelease(pDisk);
	SafeRelease(pPack);
	SafeRelease(pProvider);
	SafeRelease(pService);
	SafeRelease(pUnknown);

	DebugAction
	({
		if (formatComplete) wprintf(L"The format operation was completed succesfully.\n");
		else wprintf(L"The format operation has failed.\n");

		if (mountPoint)
			if (mountComplete) wprintf(L"The mount operation was completed succesfully.\n");
			else wprintf(L"The mount operation has failed.\n");
		});

	CoUninitialize();

	if (!formatComplete || mountPoint && !mountComplete)
	{
		DebugAction(if (hResult == S_OK) wprintf(L"Warning: operation failed, but result is S_OK!"));
		return hResult;
	}
	else
		return ERROR_SUCCESS;
}