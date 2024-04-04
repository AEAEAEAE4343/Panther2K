#define INITGUID
#include "PartitionManager.h"

#define Assert(result, action) { if (result != S_OK) { wlogf(PartitionManager::GetLogger(), PANTHER_LL_BASIC, 25, L"Failed (0x%08X)", result); action; }};

#define ObjectNameInformation (OBJECT_INFORMATION_CLASS)1
#define SafeRelease(x) {if (NULL != x) { x->Release(); x = NULL; } }
#define SafeCoFree(x) {if (NULL != x) { CoTaskMemFree(x); x = NULL; } }

#include <winternl.h>
#include <vds.h>

HRESULT PerformVdsOperation(PartitionInformation* partition, const wchar_t* fileSystem, const wchar_t* mountPoint, const wchar_t* volumeName, wchar_t** query)
{
	if (!fileSystem && !mountPoint && !query)
		return ERROR_INVALID_PARAMETER;

	if (query && (fileSystem || mountPoint))
		return ERROR_INVALID_PARAMETER;

	if (volumeName && !fileSystem)
		return ERROR_INVALID_PARAMETER;

	wlogf(PartitionManager::GetLogger(), PANTHER_LL_DETAILED, MAX_PATH, L"Starting %s%s%s%s operation for partition %d on disk %d.", query ? L"supported filesystems query" : L"", fileSystem ? L"format" : L"", fileSystem && mountPoint ? L" and " : L"", mountPoint ? L"mount" : L"", partition->PartitionNumber, partition->DiskNumber);
	if (fileSystem) wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH, L"Target file system: %s.", fileSystem);
	if (mountPoint) wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH, L"Target mount point: %s.", mountPoint);
	if (volumeName) wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH, L"Target volume name: %s.", volumeName);

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

	wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH, L"Target disk path: %s.", requestedDisk);

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

	// Initialize COM and IVdsLoader
	PartitionManager::GetLogger()->Write(PANTHER_LL_VERBOSE, L"Connecting to COM...");
	hResult = CoInitialize(NULL);
	Assert(hResult, return hResult);
	hResult = CoCreateInstance(CLSID_VdsLoader, NULL, CLSCTX_LOCAL_SERVER, IID_IVdsServiceLoader, (void**)&pLoader);
	Assert(hResult, goto releaseCOM);

	// Connect to IVdsService
	PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Connecting to VDS service...");
	hResult = pLoader->LoadService(NULL, &pService);
	SafeRelease(pLoader);
	Assert(hResult, goto releaseCOM);
	hResult = pService->WaitForServiceReady();
	Assert(hResult, goto releaseCOM);

	// Refresh VDS data
	PartitionManager::GetLogger()->Write(PANTHER_LL_VERBOSE, L"Refreshing data...");
	hResult = pService->Reenumerate();
	Assert(hResult, goto releaseCOM);
	hResult = pService->Refresh();
	Assert(hResult, goto releaseCOM);

	// Query through all software providers
	PartitionManager::GetLogger()->Write(PANTHER_LL_VERBOSE, L"Querying software providers...");
	hResult = pService->QueryProviders(VDS_QUERY_SOFTWARE_PROVIDERS, &pEnumVdsSwProviders);
	Assert(hResult, goto releaseCOM);

	for (int swpIndex = 0; (hResult = pEnumVdsSwProviders->Next(1, &pUnknown, &ulFetchCount)) == S_OK; swpIndex++)
	{
		hResult = pUnknown->QueryInterface(&pProvider);
		SafeRelease(pUnknown);
		Assert(hResult, continue);
		wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 50, L"Querying packs for Software Provider #%d...", swpIndex);

		// Query through all packs
		hResult = pProvider->QueryPacks(&pEnumVdsPacks);
		SafeRelease(pProvider);
		Assert(hResult, continue);

		for (int packIndex = 0; (hResult = pEnumVdsPacks->Next(1, &pUnknown, &ulFetchCount)) == S_OK; packIndex++)
		{
			hResult = pUnknown->QueryInterface(&pPack);
			SafeRelease(pUnknown);
			Assert(hResult, continue);

			wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 40, L"  Querying disks for Pack #%d...", packIndex);

			// Query through all disks
			hResult = pPack->QueryDisks(&pEnumVdsDisks);
			SafeRelease(pPack);
			Assert(hResult, continue);

			for (int diskIndex = 0; (hResult = pEnumVdsDisks->Next(1, &pUnknown, &ulFetchCount)) == S_OK; diskIndex++)
			{
				hResult = pUnknown->QueryInterface(&pDisk);
				SafeRelease(pUnknown);
				Assert(hResult, goto releaseDisk);

				wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 45, L"    Getting properties for Disk #%d...", diskIndex);

				// Determine if the disk contains the target
				hResult = pDisk->GetProperties(&diskProperties);
				Assert(hResult, goto releaseDisk);

				hFile = CreateFileW(diskProperties.pwszName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
				hResult = GetLastError();
				Assert(hResult, goto releaseDisk);
				res = NtQueryObject(hFile, ObjectNameInformation, &infoBuffer, 512, &bytesReceived);
				CloseHandle(hFile);
				Assert(res, goto releaseDisk);

				wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH, L"    The path of the disk is %s.", ((UNICODE_STRING*)infoBuffer)->Buffer);

				if (lstrcmpW(requestedDisk, ((UNICODE_STRING*)infoBuffer)->Buffer))
					goto releaseDisk;

				// From this point, no branch should continue iterating over disks
				PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Found the target disk.");

				// Query through extents
				hResult = pDisk->QueryExtents(&diskExtents, (PLONG)&ulFetchCount);
				Assert(hResult, goto releaseCOM);

				for (int i = 0; i < ulFetchCount; i++)
				{
					// Test if extent matches the partition offset
					if (diskExtents[i].ullOffset != partition->StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize)
						continue;

					PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Found the target partition.");
					wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 50, L"         Extent #%d:", i);
					wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 50, L"            Type: %d", diskExtents[i].type);
					wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 50, L"            Offset: %llu", diskExtents[i].ullOffset);
					wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 50, L"            Size: %llu", diskExtents[i].ullSize);
					wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH, L"            Volume GUID: {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}",
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
					PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Found the target volume.");
					hResult = pService->GetObjectW(diskExtents[i].volumeId, VDS_OT_VOLUME, &pUnknown);
					Assert(hResult, goto releaseCOM);
					hResult = pUnknown->QueryInterface(&pVolumeMF2);
					SafeRelease(pUnknown);
					Assert(hResult, goto releaseCOM);

					// Query supported filesystems 
					if (query)
					{
						VDS_FILE_SYSTEM_FORMAT_SUPPORT_PROP* formatSupport;
						LONG fsCount;
						hResult = pVolumeMF2->QueryFileSystemFormatSupport(&formatSupport, &fsCount);
						Assert(hResult, goto releaseCOM);

						int strLen = 0;
						for (int i = 0; i < fsCount; i++)
							strLen += wcslen(formatSupport[i].wszName);

						wchar_t* supportedFilesystemNames = (wchar_t*)malloc(sizeof(wchar_t) * (strLen + fsCount));
						ZeroMemory(supportedFilesystemNames, sizeof(wchar_t) * (strLen + fsCount));
						for (int i = 0; i < fsCount; i++)
						{
							wcscat_s(supportedFilesystemNames, strLen + fsCount, formatSupport[i].wszName);
							if (i != fsCount - 1)
								wcscat_s(supportedFilesystemNames, strLen + fsCount, L"|");
						}
						*query = supportedFilesystemNames;

						goto releaseCOM;
					}

					// Start the formatting
					if (fileSystem)
					{
						PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Formatting the volume...");
						wchar_t fsNameBuffer[10];
						wcscpy_s(fsNameBuffer, fileSystem);
						wchar_t fsLabelBuffer[32];
						wcscpy_s(fsLabelBuffer, volumeName);
						hResult = pVolumeMF2->FormatEx(fsNameBuffer, 0, 0, fsLabelBuffer, true, true, false, &pAsync);

						Assert(hResult, goto releaseCOM);
						asyncRes = pAsync->Wait(&hResult, &asyncOut);
						Assert(asyncRes, hResult = asyncRes; goto releaseCOM);
						Assert(hResult, goto releaseCOM);
						formatComplete = true;
					}

					if (!mountPoint) goto releaseCOM;

					// Start mounting
					PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Mounting the volume...");
					hResult = pVolumeMF2->QueryInterface(&pVolumeMF);
					SafeRelease(pVolumeMF2);
					Assert(hResult, goto releaseCOM);

					// For some reason on MBR disks it takes longer for QueryAccessPaths to become
					// available after formatting. So we just retry a couple of times.

					PartitionManager::GetLogger()->Write(PANTHER_LL_VERBOSE, L"Querying and unmounting existing volume mount points...");
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

					wchar_t mountBuffer[MAX_PATH];
					wcscpy_s(mountBuffer, mountPoint);
					if (lstrlenW(mountBuffer) == 1)
						wcscat_s(mountBuffer, L":\\");
					wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH + 36, L"Mounting the target volume to %s...", mountBuffer);
					hResult = pVolumeMF->AddAccessPath(mountBuffer);
					if (hResult == S_FALSE) hResult = S_OK;
					Assert(hResult, goto releaseCOM);
					mountComplete = true;
					goto releaseCOM;
				}

				// The partition does not exist
				hResult = VDS_E_OBJECT_NOT_FOUND;
				goto releaseCOM;

			oemOrESP:
				// The partition is an OEM or ESP partition
				PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"The partition is an OEM partition or is the EFI System Partition.");

				hResult = pDisk->QueryInterface(&pDiskMF);
				SafeRelease(pDisk);
				Assert(hResult, goto releaseCOM);

				// Query supported filesystems 
				if (query)
				{
					VDS_FILE_SYSTEM_FORMAT_SUPPORT_PROP* formatSupport;
					LONG fsCount;
					hResult = pDiskMF->QueryPartitionFileSystemFormatSupport(partition->StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize , &formatSupport, &fsCount);
					Assert(hResult, goto releaseCOM);

					int strLen = 0;
					for (int i = 0; i < fsCount; i++)
						strLen += wcslen(formatSupport[i].wszName);

					wchar_t* supportedFilesystemNames = (wchar_t*)malloc(sizeof(wchar_t) * (strLen + fsCount));
					ZeroMemory(supportedFilesystemNames, sizeof(wchar_t) * (strLen + fsCount));
					for (int i = 0; i < fsCount; i++)
					{
						wcscat_s(supportedFilesystemNames, strLen + fsCount, formatSupport[i].wszName);
						if (i != fsCount - 1)
							wcscat_s(supportedFilesystemNames, strLen + fsCount, L"|");
					}
					*query = supportedFilesystemNames;

					goto releaseCOM;
				}

				if (fileSystem) 
				{
					PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Formatting the partition...");
					wchar_t fsNameBuffer[10];
					wcscpy_s(fsNameBuffer, fileSystem);
					wchar_t fsLabelBuffer[32];
					wcscpy_s(fsLabelBuffer, volumeName);
					pDiskMF->FormatPartitionEx(partition->StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize, fsNameBuffer, 0, 0, fsLabelBuffer, true, true, false, &pAsync);

					Assert(hResult, goto releaseCOM);
					asyncRes = pAsync->Wait(&hResult, &asyncOut);
					Assert(asyncRes, hResult = asyncRes; goto releaseCOM);
					Assert(hResult, goto releaseCOM);
					formatComplete = true;
				}
				if (!mountPoint) goto releaseCOM;

				PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"Mounting the partition...");
				hResult = pDiskMF->QueryInterface(&pAdvDisk);
				SafeRelease(pDiskMF);
				Assert(hResult, goto releaseCOM);

				{
					wchar_t letter = 0;
					PartitionManager::GetLogger()->Write(PANTHER_LL_VERBOSE, L"Querying existing partition drive letter...");
					hResult = pAdvDisk->GetDriveLetter(partition->StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize, &letter);
					Assert(hResult, goto releaseCOM);
					if (letter != 0)
					{
						wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, 47, L"Unmounting partition from drive letter %c:\\...", letter);
						hResult = pAdvDisk->DeleteDriveLetter(partition->StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize, letter);
					}
					else PartitionManager::GetLogger()->Write(PANTHER_LL_VERBOSE, L"The partition has no existing drive letter.");
				}

				wlogf(PartitionManager::GetLogger(), PANTHER_LL_VERBOSE, MAX_PATH + 39, L"Mounting the target partition to %s...", mountPoint);
				hResult = pAdvDisk->AssignDriveLetter(partition->StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize, *mountPoint);
				Assert(hResult, goto releaseCOM);
				mountComplete = true;
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

	if (fileSystem)
	{
		if (formatComplete) PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"The format operation was completed successfully.");
		else PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"The format operation has failed.");
	}

	if (mountPoint)
	{
		if (mountComplete) PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"The mount operation was completed successfully.");
		else PartitionManager::GetLogger()->Write(PANTHER_LL_DETAILED, L"The mount operation has failed.");
	}

	CoUninitialize();

	if (!formatComplete || mountPoint && !mountComplete)
	{
		if (hResult == S_OK) PartitionManager::GetLogger()->Write(PANTHER_LL_NORMAL, L"Warning: format/mount operation failed, but result is S_OK!");
		return hResult;
	}
	else
		return ERROR_SUCCESS;
}

HRESULT PartitionManager::FormatPartition(PartitionInformation* partition, const wchar_t* fileSystem, const wchar_t* volumeName)
{
	return PerformVdsOperation(partition, fileSystem, NULL, volumeName, NULL);
}

HRESULT PartitionManager::MountPartition(PartitionInformation* partition, const wchar_t* mountPoint)
{
	return PerformVdsOperation(partition, NULL, mountPoint, NULL, NULL);
}

HRESULT PartitionManager::FormatAndMountPartition(PartitionInformation* partition, const wchar_t* fileSystem, const wchar_t* mountPoint, const wchar_t* volumeName)
{
	return PerformVdsOperation(partition, fileSystem, mountPoint, volumeName, NULL);
}

HRESULT PartitionManager::QueryPartitionSupportedFilesystems(PartitionInformation* partition, wchar_t** query)
{
	return PerformVdsOperation(partition, NULL, NULL, NULL, query);
}