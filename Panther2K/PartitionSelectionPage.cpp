#include "PartitionSelectionPage.h"
#include <ntifs.h>

PartitionSelectionPage::PartitionSelectionPage(const wchar_t* fileSystem, long long minimumSize, long long minimumBytesAvailable)
{

}

void PartitionSelectionPage::EnumeratePartitions()
{
	DWORD bytesCopied;
	HANDLE volume;
	BOOL success;
	wchar_t szNextVolName[MAX_PATH + 1];
	wchar_t szNextVolNameNoBSlash[MAX_PATH + 1];
	wchar_t fileSystemName[MAX_PATH + 1];

	volume = FindFirstVolume(szNextVolName, MAX_PATH);
	success = (volume != INVALID_HANDLE_VALUE);
	while (success)
	{
		HANDLE volumeFileHandle;
		VOLUME_INFO vi = { 0 };
		IO_STATUS_BLOCK iosb = { 0 };
		FILE_FS_FULL_SIZE_INFORMATION fsi = { 0 };
		
		lstrcpyW(szNextVolNameNoBSlash, szNextVolName);
		szNextVolNameNoBSlash[lstrlenW(szNextVolNameNoBSlash) - 1] = L'\0';

		if (!GetVolumePathNamesForVolumeNameW(szNextVolName, vi.mountPoint, MAX_PATH + 1, &bytesCopied))
			goto cleanup;

		CreateFileW(szNextVolNameNoBSlash, FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE,FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (volumeFileHandle == INVALID_HANDLE_VALUE)
			goto cleanup;
		if (NtQueryVolumeInformationFile(volumeFileHandle, &iosb, &fsi, sizeof(FILE_FS_FULL_SIZE_INFORMATION), FileFsFullSizeInformation))
			goto cleanup; 
		vi.totalBytes = fsi.BytesPerSector * fsi.TotalAllocationUnits.QuadPart;
		vi.bytesFree = fsi.BytesPerSector * fsi.ActualAvailableAllocationUnits.QuadPart;

		if (!GetVolumeInformationByHandleW(volumeFileHandle, vi.name, MAX_PATH + 1, NULL, NULL, NULL, fileSystemName, MAX_PATH))
			goto cleanup;
		memcpy(vi.fileSystem, fileSystemName, sizeof(wchar_t) * (MAX_PATH + 1));

	cleanup:
		CloseHandle(volumeFileHandle);
		success = FindNextVolume(volume, szNextVolName, MAX_PATH) != 0;
	}
}
