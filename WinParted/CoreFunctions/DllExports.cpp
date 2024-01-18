#include "PartitionManager.h"
#include <Shlwapi.h>
#include <shlobj_core.h>

EXTERN_C BOOL WINAPI _CRT_INIT(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);
__declspec(dllexport) void InitializeCRT()
{
	HINSTANCE instance = GetModuleHandleA("WinParted.exe");
	_CRT_INIT(instance, DLL_PROCESS_ATTACH, NULL);
};

__declspec(dllexport) int RunWinParted(Console* console, LibPanther::Logger* logger)
{
	int result = PartitionManager::RunWinParted(console);
	return result;
};

// TODO: Cleanup this mess
__declspec(dllexport) HRESULT ApplyP2KLayoutToDiskGPT(Console* console, LibPanther::Logger* logger, int diskNumber, bool letters, wchar_t*** mountPath, wchar_t*** volumeList)
{
	HRESULT ret;
	PartitionManager::ShowNoInfoDialogs = true;
	goto start;
exit:
	PartitionManager::ShowNoInfoDialogs = false;
	return ret;
start:
	PartitionManager::SetConsole(console);
	PartitionManager::SetLogger(logger);

	// Show loading screen
	PartitionManager::CurrentPage = new Page();
	PartitionManager::CurrentPage->Initialize(console);
	PartitionManager::CurrentPage->Update();

	if (PartitionManager::ShowMessagePage(L"Warning: All data on the drive will be lost and a new partition table will be written. Would you like to continue?", MessagePageType::YesNo, MessagePageUI::Warning) != MessagePageResult::Yes)
	{
		ret = ERROR_CANCELLED;
		goto exit;
	}

	HRESULT hResult;

	PartitionManager::PopulateDiskInformation();
	PartitionManager::CurrentDisk.DiskNumber = -1;
	for (int i = 0; i < PartitionManager::DiskInformationTableSize; i++)
		if (PartitionManager::DiskInformationTable[i].DiskNumber == diskNumber)
			PartitionManager::CurrentDisk = PartitionManager::DiskInformationTable[diskNumber];
	if (PartitionManager::CurrentDisk.DiskNumber == -1)
	{
		ret = ERROR_FILE_NOT_FOUND;
		goto exit;
	}

	wchar_t rootCwdPath[MAX_PATH];
	if (_wgetcwd(rootCwdPath, MAX_PATH) == NULL)
	{
		ret = ERROR_PATH_NOT_FOUND;
		goto exit;
	}

	PathStripToRootW(rootCwdPath);

	for (int i = 0; i < 3; i++)
	{
		wcscpy_s((*mountPath)[i], MAX_PATH, rootCwdPath);
	}

	if (letters)
	{
		int mountIndex = 0;
		DWORD drives = GetLogicalDrives();
		const wchar_t* letters = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		for (int i = 25; i >= 0 && mountIndex < 3; i--)
		{
			if (drives ^ 0b10000000000000000000000000
				&& letters[i] != L'X'
				&& letters[i] != L'A'
				&& letters[i] != L'B')
			{
				(*mountPath)[mountIndex][0] = letters[i];
				(*mountPath)[mountIndex++][1] = L'\0';
			}
			drives <<= 1;
		}

		if (mountIndex != 3)
		{
			ret = ERROR_BUSY;
			goto exit;
		}
	}
	else
	{
		wcscat_s((*mountPath)[0], MAX_PATH, L"$Panther2K\\Sys\\");
		wcscat_s((*mountPath)[1], MAX_PATH, L"$Panther2K\\Win\\");
		wcscat_s((*mountPath)[2], MAX_PATH, L"$Panther2K\\Rec\\");

		for (int i = 0; i < 3; i++)
		{
			hResult = SHCreateDirectoryExW(NULL, (*mountPath)[i], NULL);
			if (hResult != ERROR_SUCCESS && hResult != ERROR_ALREADY_EXISTS)
			{
				ret = hResult;
				goto exit;
			}
		}
	}

	int totalPartitions = 4;
	long structSize = (sizeof(WP_PART_LAYOUT) + ((totalPartitions - 1) * sizeof(WP_PART_DESCRIPTION)));
	WP_PART_LAYOUT* layout = (WP_PART_LAYOUT*)safeMalloc(logger, structSize);
	ZeroMemory(layout, structSize);
	layout->PartitionCount = totalPartitions;

	layout->Partitions[0].PartitionNumber = 1;
	layout->Partitions[0].PartitionType = 0x0700;
	if (PartitionManager::CurrentDisk.SectorSize == 4096)
		wcscpy_s(layout->Partitions[0].PartitionSize, L"300M");
	else
		wcscpy_s(layout->Partitions[0].PartitionSize, L"150M");
	wcscpy_s(layout->Partitions[0].FileSystem, L"FAT32");
	layout->Partitions[0].MountPoint = (*mountPath)[0];

	layout->Partitions[1].PartitionNumber = 2;
	layout->Partitions[1].PartitionType = 0x0C01;
	wcscpy_s(layout->Partitions[1].PartitionSize, L"16M");
	wcscpy_s(layout->Partitions[1].FileSystem, L"RAW");

	layout->Partitions[2].PartitionNumber = 1;
	layout->Partitions[2].PartitionType = 0x0700;
	wcscpy_s(layout->Partitions[2].PartitionSize, L"100%");
	wcscpy_s(layout->Partitions[2].FileSystem, L"NTFS");
	layout->Partitions[2].MountPoint = (*mountPath)[1];

	layout->Partitions[3].PartitionNumber = 1;
	layout->Partitions[3].PartitionType = 0x0700;
	wcscpy_s(layout->Partitions[3].PartitionSize, L"500M");
	wcscpy_s(layout->Partitions[3].FileSystem, L"NTFS");
	layout->Partitions[3].MountPoint = (*mountPath)[2];

	ret = PartitionManager::ApplyPartitionLayoutGPT(layout);
	int volIndex = 0;
	for (int i = 0; i < 4; i++)
	{
		if (i == 1) continue;
		PartitionManager::LoadPartition(&PartitionManager::CurrentDiskPartitions[i]);
		lstrcpyW((*volumeList)[volIndex++], PartitionManager::CurrentPartition.VolumeInformation.VolumeFile);
	}
	free(layout);
	goto exit;
}

__declspec(dllexport) HRESULT ApplyP2KLayoutToDiskMBR(Console* console, LibPanther::Logger* logger, int diskNumber, bool letters, wchar_t*** mountPath, wchar_t*** volumeList)
{
	HRESULT ret;
	PartitionManager::ShowNoInfoDialogs = true;
	goto start;
exit:
	PartitionManager::ShowNoInfoDialogs = false;
	return ret;
start:
	PartitionManager::SetConsole(console);

	// Show loading screen
	PartitionManager::CurrentPage = new Page();
	PartitionManager::CurrentPage->Initialize(console);
	PartitionManager::CurrentPage->Update();

	if (PartitionManager::ShowMessagePage(L"Warning: All data on the drive will be lost and a new partition table will be written. Would you like to continue?", MessagePageType::YesNo, MessagePageUI::Warning) != MessagePageResult::Yes)
	{
		ret = ERROR_CANCELLED;
		goto exit;
	}

	HRESULT hResult;

	PartitionManager::PopulateDiskInformation();
	PartitionManager::CurrentDisk.DiskNumber = -1;
	for (int i = 0; i < PartitionManager::DiskInformationTableSize; i++)
		if (PartitionManager::DiskInformationTable[i].DiskNumber == diskNumber)
			PartitionManager::CurrentDisk = PartitionManager::DiskInformationTable[diskNumber];
	if (PartitionManager::CurrentDisk.DiskNumber == -1)
	{
		ret = ERROR_FILE_NOT_FOUND;
		goto exit;
	}

	wchar_t rootCwdPath[MAX_PATH];
	if (_wgetcwd(rootCwdPath, MAX_PATH) == NULL)
	{
		ret = ERROR_PATH_NOT_FOUND;
		goto exit;
	}

	PathStripToRootW(rootCwdPath);

	for (int i = 0; i < 3; i++)
	{
		wcscpy_s((*mountPath)[i], MAX_PATH, rootCwdPath);
	}

	if (letters)
	{
		int mountIndex = 0;
		DWORD drives = GetLogicalDrives();
		const wchar_t* letters = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		for (int i = 25; i >= 0 && mountIndex < 3; i--)
		{
			if (drives ^ 0b10000000000000000000000000
				&& letters[i] != L'X'
				&& letters[i] != L'A'
				&& letters[i] != L'B')
			{
				(*mountPath)[mountIndex][0] = letters[i];
				(*mountPath)[mountIndex++][1] = L'\0';
			}
			drives <<= 1;
		}

		if (mountIndex != 3)
		{
			ret = ERROR_BUSY;
			goto exit;
		}
	}
	else
	{
		wcscat_s((*mountPath)[0], MAX_PATH, L"$Panther2K\\Sys\\");
		wcscat_s((*mountPath)[1], MAX_PATH, L"$Panther2K\\Win\\");
		wcscat_s((*mountPath)[2], MAX_PATH, L"$Panther2K\\Rec\\");

		for (int i = 0; i < 3; i++)
		{
			hResult = SHCreateDirectoryExW(NULL, (*mountPath)[i], NULL);
			if (hResult != ERROR_SUCCESS && hResult != ERROR_ALREADY_EXISTS)
			{
				ret = hResult;
				goto exit;
			}
		}
	}

	int totalPartitions = 3;
	long structSize = (sizeof(WP_PART_LAYOUT) + ((totalPartitions - 1) * sizeof(WP_PART_DESCRIPTION)));
	WP_PART_LAYOUT* layout = (WP_PART_LAYOUT*)safeMalloc(logger, structSize);
	ZeroMemory(layout, structSize);
	layout->PartitionCount = totalPartitions;

	layout->Partitions[0].PartitionNumber = 1;
	layout->Partitions[0].PartitionType = 0x0780;
	if (PartitionManager::CurrentDisk.SectorSize == 4096)
		wcscpy_s(layout->Partitions[0].PartitionSize, L"500M");
	else
		wcscpy_s(layout->Partitions[0].PartitionSize, L"150M");
	wcscpy_s(layout->Partitions[0].FileSystem, L"NTFS");
	layout->Partitions[0].MountPoint = (*mountPath)[0];

	layout->Partitions[1].PartitionNumber = 2;
	layout->Partitions[1].PartitionType = 0x0700;
	wcscpy_s(layout->Partitions[1].PartitionSize, L"100%");
	wcscpy_s(layout->Partitions[1].FileSystem, L"NTFS");
	layout->Partitions[1].MountPoint = (*mountPath)[1];

	layout->Partitions[2].PartitionNumber = 3;
	layout->Partitions[2].PartitionType = 0x0700;
	wcscpy_s(layout->Partitions[2].PartitionSize, L"500M");
	wcscpy_s(layout->Partitions[2].FileSystem, L"NTFS");
	layout->Partitions[2].MountPoint = (*mountPath)[2];

	ret = PartitionManager::ApplyPartitionLayoutMBR(layout);
	int volIndex = 0;
	for (int i = 0; i < 3; i++)
	{
		PartitionManager::LoadPartition(&PartitionManager::CurrentDiskPartitions[i]);
		lstrcpyW((*volumeList)[volIndex++], PartitionManager::CurrentPartition.VolumeInformation.VolumeFile);
	}
	free(layout);
	goto exit;
}

__declspec(dllexport) bool SetPartType(Console* console, LibPanther::Logger* logger, int diskNumber, unsigned long long partOffset, short partType)
{
	PartitionManager::SetConsole(console);
	PartitionManager::ShowNoInfoDialogs = true;

	PartitionManager::PopulateDiskInformation();
	if (!PartitionManager::LoadDisk(&PartitionManager::DiskInformationTable[diskNumber])) goto retFalse;
	{
		bool loaded = false;
		for (int i = 0; i < PartitionManager::CurrentDiskPartitionCount; i++)
		{
			if (PartitionManager::CurrentDiskPartitions[i].StartLBA.ULL * PartitionManager::CurrentDisk.SectorSize == partOffset)
			{
				if (!PartitionManager::LoadPartition(&PartitionManager::CurrentDiskPartitions[i]))
					goto retFalse;
				loaded = true;
				break;
			}
		}
		if (!loaded) goto retFalse;
	}
	if (!PartitionManager::SetCurrentPartitionType(partType)) goto retFalse;
	if (!PartitionManager::SavePartitionTableToDisk()) goto retFalse;

	PartitionManager::ShowNoInfoDialogs = false;
	return true;
retFalse:
	PartitionManager::ShowNoInfoDialogs = false;
	return false;
}