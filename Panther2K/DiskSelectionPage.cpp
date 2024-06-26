#include "DiskSelectionPage.h"
#include "WindowsSetup.h"
#include "QuitingPage.h"
#include "MessageBoxPage.h"

void GetSizeStringFromBytes(unsigned long long bytes, wchar_t buffer[10])
{
	// Overkill
	bool useSI = false;
	double factor = useSI ? 1000.0 : 1024.0;
	const wchar_t valueStringsSI[9][4] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB" };
	const wchar_t valueStringsBinary[9][4] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB", L"PiB", L"EiB", L"ZiB", L"YiB" };

	int index = 0;
	double bytesFp = bytes;
	while (bytesFp > factor && index < 9)
	{
		bytesFp /= factor;
		index++;
	}
	swprintf(buffer, 10, L"%.1f%s", bytesFp, useSI ? valueStringsSI[index] : valueStringsBinary[index]);
}

void DiskSelectionPage::Init()
{
	wchar_t* displayName = WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName;
	int length = lstrlenW(displayName);
	wchar_t* textBuffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), length * sizeof(wchar_t) + 14);
	memcpy(textBuffer, displayName, length * sizeof(wchar_t));
	memcpy(textBuffer + length, L" Setup", 14);
	text = textBuffer;
	statusText = L"  ENTER=Select  ESC=Back  F3=Quit";

	HANDLE diskFileHandle;
	DWORD byteCount;
	DISK_GEOMETRY dg;
	STORAGE_PROPERTY_QUERY spq;
	STORAGE_DEVICE_DESCRIPTOR sdd;
	PSTORAGE_DEVICE_DESCRIPTOR psdd;
	wchar_t buffer[256];

	if (diskInfo != NULL)
		free(diskInfo);

	wchar_t* dosdevs = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), sizeof(wchar_t*) * 65536);

	QueryDosDeviceW(NULL, dosdevs, 65536);
    diskCount = 0;
	for (wchar_t* pos = dosdevs; *pos; pos += lstrlenW(pos) + 1)
		if (wcsncmp(pos, L"PhysicalDrive", 13) == 0)
			diskCount++;

	diskInfo = (DISK_INFO*)safeMalloc(WindowsSetup::GetLogger(), sizeof(DISK_INFO) * diskCount);

	int i = 0;
	for (wchar_t* pos = dosdevs; *pos; pos += lstrlenW(pos) + 1)
	{
		if (wcsncmp(pos, L"PhysicalDrive", 13) == 0)
		{
			diskInfo[i].diskNumber = wcstol(pos + 13, NULL, 10);

			swprintf_s(buffer, L"\\\\.\\\PHYSICALDRIVE%d", diskInfo[i].diskNumber);
			diskFileHandle = CreateFileW(buffer, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
			if (diskFileHandle == INVALID_HANDLE_VALUE)
			{
				if (GetLastError() == 5)
				{
					MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to enumerate disks: Access denied. Please re-run Panther2K as Administrator. Panther2K will exit.", true, this);
					msgBox->ShowDialog();
					delete msgBox;
					WindowsSetup::RequestExit();
					return;
				}
				else
				{
					diskCount--;
					continue;
				}
			}

			if (!DeviceIoControl(diskFileHandle, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &dg, sizeof(dg), &byteCount, NULL))
			{
				diskCount--;
				goto clean;
			}

			// IOCTL_STORAGE_QUERY_PROPERTY 
			// Determines the name of the disk
			// See: https://docs.microsoft.com/en-us/windows/win32/api/winioctl/ne-winioctl-storage_property_id
			spq.PropertyId = StorageDeviceProperty;
			spq.QueryType = PropertyStandardQuery;
			if (!DeviceIoControl(diskFileHandle, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), &sdd, sizeof(sdd), &byteCount, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				diskCount--;
				goto clean;
			}
			psdd = (STORAGE_DEVICE_DESCRIPTOR*)malloc(sdd.Size);
			if (psdd == NULL || !DeviceIoControl(diskFileHandle, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), psdd, sdd.Size, &byteCount, NULL))
			{
				diskCount--;
				goto clean;
			}

			// Concatenate vendor id + product id
			// Convert the name into a wide string
			{
				wchar_t* destPtr = diskInfo[i].name;
				int sizeOfSrcString = strlen((char*)psdd + psdd->VendorIdOffset);
				int sizeOfDestString = 260;
				char* srcPtr = (char*)psdd + psdd->VendorIdOffset;
				size_t count = _TRUNCATE;
				size_t bytesWritten;

				if (sizeOfSrcString > 0)
				{
					mbstowcs_s(&bytesWritten, destPtr, sizeOfDestString, srcPtr, count);
					diskInfo[i].name[bytesWritten - 1] = L' ';
				}

				destPtr = diskInfo[i].name + bytesWritten;
				sizeOfSrcString = strlen((char*)psdd + psdd->ProductIdOffset);
				sizeOfDestString = 256 - bytesWritten;
				srcPtr = (char*)psdd + psdd->ProductIdOffset;
				mbstowcs_s(&bytesWritten, destPtr, sizeOfDestString, srcPtr, count);

				wchar_t* temp = CleanString(diskInfo[i].name);
				if (temp)
				{
					lstrcpyW(diskInfo[i].name, temp);
					free(temp);
				}
			}

			diskInfo[i].size = dg.Cylinders.QuadPart * dg.TracksPerCylinder * dg.SectorsPerTrack * dg.BytesPerSector;
			diskInfo[i].mediaType = dg.MediaType;

		clean:
			CloseHandle(diskFileHandle);
			i++;
		}
	}
}

void DiskSelectionPage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);

	DrawTextLeft(L"Select the disk to install Windows to.", console->GetSize().cx - 6, 4);

	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	DrawTextLeft(L"Windows will be installed to the disk specified. All data on the disk will be destroyed and Panther2K will create a bootable Windows installation on the disk. To install Windows without wiping a disk or while using a custom partition layout, select \"Custom\"", console->GetSize().cx - 6, console->GetPosition().y + 2);

	DrawTextLeft(L"Use the UP and DOWN ARROW keys to select the disk.", console->GetSize().cx - 6, console->GetPosition().y + 2);

	int boxX = 3;
	boxY = console->GetPosition().y + 2;
	int boxWidth = console->GetSize().cx - 6;
	int boxHeight = console->GetSize().cy - (boxY + 2);
	DrawBox(boxX, boxY, boxWidth, boxHeight, true);

	wchar_t* buffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), sizeof(wchar_t) * (boxWidth - 2));
	swprintf(buffer, boxWidth - 2, L"   Disk  Device Name                        %*sType      Disk size   ", boxWidth - 68, L"");
	console->SetPosition(boxX + 1, boxY + 1);
	console->Write(buffer);
	free(buffer);
}

void DiskSelectionPage::Redrawer()
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
	wchar_t sizeBuffer[10];
	for (int i = 0; i < min(diskCount + 1, maxItems); i++)
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
		
		if (i == diskCount) {
			swprintf_s(buffer, boxWidth - 2, L"  Custom...%*s", boxWidth - 19, L"");
		}
		else {
			GetSizeStringFromBytes(diskInfo[i].size, sizeBuffer);
			swprintf_s(buffer, boxWidth - 2, L"%4d  %-*s  %-8s  %-9s", diskInfo[i].diskNumber, boxWidth - 35, diskInfo[i].name, L"Standard", sizeBuffer);
		}

		console->Write(buffer);
	}
	free(buffer);
}

bool DiskSelectionPage::KeyHandler(WPARAM wParam)
{
	int boxX = 3;
	int boxWidth = console->GetSize().cx - 6;
	int boxHeight = console->GetSize().cy - (boxY + 2);
	int maxItems = boxHeight - 3;
	switch (wParam)
	{
	case VK_DOWN:
		if (selectionIndex + 1 < min(maxItems, diskCount + 1))
			selectionIndex++;
		else if (selectionIndex + scrollIndex + 1 < diskCount + 1)
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
	case VK_ESCAPE:
		WindowsSetup::LoadPhase(3);
		break;
	case VK_RETURN:
		if (selectionIndex == diskCount)
			WindowsSetup::SelectNextPartition(0);
		else
		{
			if (WindowsSetup::SelectPartitionsWithDisk(diskInfo[selectionIndex].diskNumber))
				WindowsSetup::LoadPhase(5);
		}
		break;
	case VK_F3:
		AddPopup(new QuitingPage());
		break;
	}
	return true;
}
