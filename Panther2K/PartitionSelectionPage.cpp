#include "PartitionSelectionPage.h"
#include "WindowsSetup.h"
#include "QuitingPage.h"
#include "wdkpartial.h"
#include "MessageBoxPage.h"
#include "WinPartedDll.h"

bool libLoaded = false;
NtQueryVolumeInformationFileFunction NtQueryVolumeInformationFile;

const wchar_t* const part1Strings[] = 
{ 
	L"Select the System partition.", 
	L"Select the EFI System Partition partition.", 
	L"Select the System Reserved partition.", 
	L"Select the Recovery partition." 
};
const wchar_t* const part2Strings[] = 
{
	L"Panther2K will use this partition to store the system files for the Microsoft Windows operating system. This partition will be available as the C: drive from within Windows.", 
	L"Panther2K will use this partition to store the files required for booting Microsoft Windows on your computer. This is a system reserved partition that will not be available for use inside Windows after installation.",
	L"Panther2K will use this partition to store the files required for booting Microsoft Windows on your computer. This is a system reserved partition that will not be available for use inside Windows after installation.", 
	L"Panther2K will use this partition to store the files required for booting into the Windows Recovery Environment (WinRE). WinRE can be used whenever Windows fails to load, for example due to an incompatible driver update."
};

PartitionSelectionPage::PartitionSelectionPage(const wchar_t* fileSystem, long long minimumSize, long long minimumBytesAvailable, int stringIndex, int displayIndex)
{
	requirements.fileSystem = fileSystem;
	requirements.partitionSize = minimumSize;
	requirements.partitionFree = minimumBytesAvailable;
	stringTableIndex = stringIndex;
	dispIndex = displayIndex;

	if (!libLoaded)
	{
		HINSTANCE hinstKrnl = LoadLibraryW(L"ntdll.dll");
		if (hinstKrnl == 0)
		{
			//ERROR OUT
		}
		NtQueryVolumeInformationFile = (NtQueryVolumeInformationFileFunction) GetProcAddress(hinstKrnl, "NtQueryVolumeInformationFile");
		if (NtQueryVolumeInformationFile == 0)
		{
			//ERROR OUT
		}
		libLoaded = true;
	}
}

void PartitionSelectionPage::EnumeratePartitions()
{
	DWORD bytesCopied;
	HANDLE volume;
	BOOL success;
	wchar_t szNextVolName[MAX_PATH + 1];
	wchar_t szNextVolNameNoBSlash[MAX_PATH + 1];
	wchar_t fileSystemName[MAX_PATH + 1];
	wchar_t diskPath[MAX_PATH + 1];
	wchar_t fileBuffer[1024];

#ifdef _DEBUG
	HANDLE hFile = CreateFileW(L"partitions.txt", GENERIC_ALL, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else
	HANDLE hFile = 0;
#endif

	volume = FindFirstVolume(szNextVolName, MAX_PATH);
	success = (volume != INVALID_HANDLE_VALUE);
	while (success)
	{
		swprintf(fileBuffer, 1024,  L"========================================\n");
		WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);
		swprintf(fileBuffer, 1024, L"Volume name: %s\n", szNextVolName);
		WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

		HANDLE volumeFileHandle = 0;
		HANDLE diskFileHandle = 0;
		VOLUME_INFO vi = { 0 };
		IO_STATUS_BLOCK iosb = { 0 };
		FILE_FS_FULL_SIZE_INFORMATION fsi = { 0 };
		VOLUME_DISK_EXTENTS* vde = (VOLUME_DISK_EXTENTS*)safeMalloc(WindowsSetup::GetLogger(), sizeof(VOLUME_DISK_EXTENTS));
		DRIVE_LAYOUT_INFORMATION_EX* dli = (DRIVE_LAYOUT_INFORMATION_EX*)safeMalloc(WindowsSetup::GetLogger(), sizeof(DRIVE_LAYOUT_INFORMATION_EX));
		int partitionCount = 1;
		bool done = false;
		
		lstrcpyW(szNextVolNameNoBSlash, szNextVolName);
		szNextVolNameNoBSlash[lstrlenW(szNextVolNameNoBSlash) - 1] = L'\0';

		if (!GetVolumePathNamesForVolumeNameW(szNextVolName, vi.mountPoint, MAX_PATH + 1, &bytesCopied))
			goto cleanup;

		swprintf(fileBuffer, 1024, L"Volume path: %s\n", vi.mountPoint);
		WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

		volumeFileHandle = CreateFileW(szNextVolNameNoBSlash, FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (volumeFileHandle == INVALID_HANDLE_VALUE)
			goto cleanup;
		if (NtQueryVolumeInformationFile(volumeFileHandle, &iosb, &fsi, sizeof(FILE_FS_FULL_SIZE_INFORMATION), FileFsFullSizeInformation))
			goto cleanup; 
		vi.totalBytes = (long long)fsi.BytesPerSector * (long long)fsi.SectorsPerAllocationUnit * fsi.TotalAllocationUnits.QuadPart;
		vi.bytesFree = (long long)fsi.BytesPerSector * (long long)fsi.SectorsPerAllocationUnit * fsi.ActualAvailableAllocationUnits.QuadPart;
		
		swprintf(fileBuffer, 1024, L"Volume size (MiB): %I64i\n", vi.totalBytes / 1024 / 1024);
		WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

		swprintf(fileBuffer, 1024, L"Volume free (MiB): %I64i\n", vi.bytesFree / 1024 / 1024);
		WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

		if (!showAll && !WindowsSetup::AllowSmallVolumes && (vi.bytesFree < requirements.partitionFree || vi.totalBytes < requirements.partitionSize))
			goto cleanup;

		if (!GetVolumeInformationByHandleW(volumeFileHandle, vi.name, MAX_PATH + 1, NULL, NULL, NULL, fileSystemName, MAX_PATH))
			goto cleanup;

		swprintf(fileBuffer, 1024, L"Filesystem: %I64i\n", vi.bytesFree / 1024 / 1024);
		WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

		if (!showAll && !WindowsSetup::AllowOtherFileSystems && lstrcmpW(fileSystemName, requirements.fileSystem) != 0)
			goto cleanup;

		if (!DeviceIoControl(volumeFileHandle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, sizeof(VOLUME_DISK_EXTENTS), &bytesCopied, NULL))
			goto cleanup;

		swprintf(diskPath, MAX_PATH, L"\\\\.\\\PHYSICALDRIVE%d", vde->Extents->DiskNumber);
		diskFileHandle = CreateFileW(diskPath, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (diskFileHandle == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == 5)
			{
				MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to enumerate partitions: Access denied. Please re-run Panther2K as Administrator. Panther2K will exit.", true, this);
				msgBox->ShowDialog();
				delete msgBox;
				WindowsSetup::RequestExit();
				return;
			}
			else goto cleanup;
		}

		do
		{
			if (!DeviceIoControl(diskFileHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, dli, 48 + (partitionCount * 144), &bytesCopied, NULL))
			{
				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
					goto cleanup;

				size_t size = offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry[++partitionCount]);
				free(dli);
				dli = (DRIVE_LAYOUT_INFORMATION_EX*)safeMalloc(WindowsSetup::GetLogger(), size);
			}
			else done = true;
		} while (!done);

		for (int i = 0; i < dli->PartitionCount; i++)
		{
			if (vde->Extents->StartingOffset.QuadPart >= dli->PartitionEntry[i].StartingOffset.QuadPart &&
				vde->Extents->StartingOffset.QuadPart < dli->PartitionEntry[i].StartingOffset.QuadPart + dli->PartitionEntry[i].PartitionLength.QuadPart)
			{
				vi.diskNumber = vde->Extents->DiskNumber;
				vi.partitionNumber = dli->PartitionEntry[i].PartitionNumber;

				swprintf(fileBuffer, 1024, L"Disk number: %i\n", vi.diskNumber);
				WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);
				swprintf(fileBuffer, 1024, L"Partition number: %i\n", vi.partitionNumber);
				WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);
			}
		}

		free(vde);
		free(dli);
		memcpy(vi.fileSystem, fileSystemName, sizeof(wchar_t) * (MAX_PATH + 1));
		lstrcpyW(vi.guid, szNextVolNameNoBSlash);
		volumeInfo.push_back(vi);
	cleanup:
		CloseHandle(volumeFileHandle);
		success = FindNextVolume(volume, szNextVolName, MAX_PATH) != 0;
	}

	CloseHandle(hFile);
	Draw();
}

VOLUME_INFO PartitionSelectionPage::GetSelectedVolume()
{
	return volumeInfo[scrollIndex + selectionIndex];
}

void PartitionSelectionPage::Init()
{
	wchar_t* displayName = WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName;
	int length = lstrlenW(displayName);
	wchar_t* textBuffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), length * sizeof(wchar_t) + 14);
	memcpy(textBuffer, displayName, length * sizeof(wchar_t));
	memcpy(textBuffer + length, L" Setup", 14);
	text = textBuffer;
	statusText = L"  ENTER=Select  F8=DiskPart  F9=Display all  ESC=Back  F3=Quit";
}

void PartitionSelectionPage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);

	DrawTextLeft(part1Strings[stringTableIndex], console->GetSize().cx - 6, 4);

	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	DrawTextLeft(part2Strings[stringTableIndex], console->GetSize().cx - 6, console->GetPosition().y + 2);

	DrawTextLeft(L"Use the UP and DOWN ARROW keys to select the partition.", console->GetSize().cx - 6, console->GetPosition().y + 2);

	DrawTextLeft(L"Press F8 to modify your partition layout using DiskPart.", console->GetSize().cx - 6, console->GetPosition().y + 1);

	int boxX = 3;
	boxY = console->GetPosition().y + 2;
	int boxWidth = console->GetSize().cx - 6;
	int boxHeight = console->GetSize().cy - (boxY + 2);
	DrawBox(boxX, boxY, boxWidth, boxHeight, true);

	wchar_t* buffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), sizeof(wchar_t) * (boxWidth - 2));
	swprintf(buffer, boxWidth - 2, L"   Disk  Partition  Volume Name%*sSize (GB)  Mount Point  ", boxWidth - 58, L"");
	console->SetPosition(boxX + 1, boxY + 1);
	console->Write(buffer);
	free(buffer);
}

void PartitionSelectionPage::Redrawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	int boxX = 3;
	int boxWidth = console->GetSize().cx - 6;
	int boxHeight = console->GetSize().cy - (boxY + 2);
	int maxItems = boxHeight - 3;

	bool canScrollDown = (scrollIndex + maxItems) < WindowsSetup::WimImageCount;
	bool canScrollUp = scrollIndex != 0;

	wchar_t* buffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), sizeof(wchar_t) * (boxWidth - 2));
	for (int i = 0; i < min(volumeInfo.size(), maxItems); i++)
	{
		if (i == selectionIndex)
		{
			console->SetBackgroundColor(WindowsSetup::ForegroundColor);
			console->SetForegroundColor(WindowsSetup::BackgroundColor);
		}
		else
		{
			console->SetBackgroundColor(WindowsSetup::BackgroundColor);
			console->SetForegroundColor(WindowsSetup::ForegroundColor);
		}

		console->SetPosition(boxX + 4, boxY + i + 2);
		swprintf(buffer, boxWidth - 2, L"%4d  %-9d  %-*s%10.1F  %-11s", volumeInfo[i].diskNumber, volumeInfo[i].partitionNumber, boxWidth - 48, volumeInfo[i].name, static_cast<float>(volumeInfo[i].totalBytes / 1000) / 1000.0, volumeInfo[i].mountPoint);
		console->Write(buffer);
	}
	free(buffer);
}

bool PartitionSelectionPage::KeyHandler(WPARAM wParam)
{
	int boxX = 3;
	int boxWidth = console->GetSize().cx - 6;
	int boxHeight = console->GetSize().cy - (boxY + 2);
	int maxItems = boxHeight - 3;
	int totalItems = volumeInfo.size();
	switch (wParam)
	{
	case VK_DOWN:
		if (selectionIndex + 1 < min(maxItems, totalItems))
			selectionIndex++;
		else if (selectionIndex + scrollIndex + 1 < totalItems)
			scrollIndex++;
		Redraw();
		break;
	case VK_UP:
		if (selectionIndex - 1 >= 0)
			selectionIndex--;
		else if (selectionIndex + scrollIndex - 1 >= 0)
			scrollIndex--;
		Redraw();
		break;
	case VK_F3:
		AddPopup(new QuitingPage());
		break;
	case VK_F8:
		WinPartedDll::RunWinParted(console, WindowsSetup::GetLogger());
		break;
	case VK_F9:
		showAll = !showAll;
		EnumeratePartitions();
		break;
	case VK_ESCAPE:
		WindowsSetup::SelectNextPartition(dispIndex - 1);
		break;
	case VK_RETURN:
		WindowsSetup::SelectPartition(stringTableIndex, GetSelectedVolume());
		WindowsSetup::SelectNextPartition(dispIndex + 1);
		break;
	}
	return true;
}
