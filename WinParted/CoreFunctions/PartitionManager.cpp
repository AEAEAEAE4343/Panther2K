#include "PartitionManager.h"
#include "..\DiskSelectionPage.h"
#include <iostream>

#include "PantherLogger.h"

DISK_INFORMATION* PartitionManager::DiskInformationTable = 0;
long PartitionManager::DiskInformationTableSize = 0;

DISK_INFORMATION PartitionManager::CurrentDisk = { 0 };
PartitionTableType PartitionManager::CurrentDiskType = PartitionTableType::Unknown;
MBR_HEADER PartitionManager::CurrentDiskMBR = { 0 };
GPT_HEADER PartitionManager::CurrentDiskGPT = { 0 };
GPT_ENTRY* PartitionManager::CurrentDiskGPTTable = 0;
OperatingMode PartitionManager::CurrentDiskOperatingMode = OperatingMode::Unknown;
PartitionInformation* PartitionManager::CurrentDiskPartitions = 0;
long PartitionManager::CurrentDiskPartitionCount = 0;
bool PartitionManager::CurrentDiskPartitionsModified = 0;
bool PartitionManager::CurrentDiskPartitionTableDestroyed = 0;
long PartitionManager::CurrentDiskFirstAvailablePartition = 0;
PartitionInformation PartitionManager::CurrentPartition = { 0 };

bool PartitionManager::ShowNoInfoDialogs = false;

Console* PartitionManager::currentConsole = 0;
LibPanther::Logger* PartitionManager::logger = 0;
Page* PartitionManager::CurrentPage = 0;
std::stack<Page*> PartitionManager::pageStack = std::stack<Page*>();
bool PartitionManager::shouldExit = false;
int PartitionManager::exitCode = 0;

void PartitionManager::SetConsole(Console* console)
{
	currentConsole = console;
}

void PartitionManager::SetLogger(LibPanther::Logger* logger)
{
	PartitionManager::logger = logger;
}

int PartitionManager::RunWinParted(Console* console)
{
    // Prepare the console
    if (console == NULL)
    {
		bool customConsole = true;
		if (customConsole)
		{
			currentConsole = new CustomConsole();
			currentConsole->Init();
			//currentConsole->SetSize(40, 80);
			ShowWindow(((CustomConsole*)currentConsole)->WindowHandle, SW_SHOW);
			((CustomConsole*)currentConsole)->SetPixelScale(1);
		}
		else 
		{
			printf("Creating WinParted console (Win32)...");
			currentConsole = new Win32Console();
			currentConsole->Init();
		}
    }
    else
        currentConsole = console;

	// Reset values
	printf("Initializing values...");
	exitCode = 0;
	shouldExit = false;

    // Show loading screen
	printf("Creating loading screen...");
    CurrentPage = new Page();
    CurrentPage->Initialize(currentConsole);
	printf("Started.");
	CurrentPage->Draw();
    CurrentPage->Run();

	Sleep(100);

    PushPage(new DiskSelectionPage());

    // Run page loop
    while (!shouldExit || !pageStack.top())
    {
		CurrentPage = pageStack.top();
		CurrentPage->Draw();
        CurrentPage->Run();
    }

    return exitCode;
}

void PartitionManager::PushPage(Page* page)
{
	pageStack.push(page);
	page->Initialize(currentConsole);
}

void PartitionManager::PopPage()
{
	delete pageStack.top();
	pageStack.pop();
}

MessagePageResult PartitionManager::ShowMessagePage(const wchar_t* message, MessagePageType type, MessagePageUI uiStyle)
{
	if (ShowNoInfoDialogs && uiStyle == MessagePageUI::Normal)
		return MessagePageResult::OK;

	if (!wcsstr(message, L"%s"))
	{
		MessagePage* msgp = new MessagePage(currentConsole, message, type, uiStyle);
		MessagePageResult res = msgp->ShowDialog();
		delete msgp;
		if (CurrentPage) CurrentPage->Draw();
		return res;
	}
	else
	{
		wchar_t buffer[512];
		wchar_t errorBuffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuffer, 256, NULL);
		wchar_t* formattedError = CleanString(errorBuffer);
		swprintf(buffer, 512, message, formattedError);
		free(formattedError);

		MessagePage* msgp = new MessagePage(currentConsole, buffer, type, uiStyle);
		MessagePageResult res = msgp->ShowDialog();
		delete msgp;
		if (CurrentPage) CurrentPage->Draw();
		return res;
	}
}

void PartitionManager::Exit(int returnCode)
{
	if (returnCode == 0 && CurrentDiskPartitionsModified)
	{
		switch (PartitionManager::ShowMessagePage(L"The partition table was modified, but it was not saved to disk. Would you like to save the modified partition table before exiting WinParted?", MessagePageType::YesNoCancel))
		{
		case MessagePageResult::Yes:
			// Save and continue
			PartitionManager::SavePartitionTableToDisk();
			break;
		case MessagePageResult::No:
			CurrentDiskPartitionsModified = false;
			// Discard and continue
			break;
		case MessagePageResult::Cancel:
			// Just return
			return;
		}
	}

	exitCode = returnCode;
	shouldExit = true;
}

void PartitionManager::Restart() 
{
	while (pageStack.size() > 1)
		PopPage();
};

void PartitionManager::PopulateDiskInformation()
{
	wchar_t nameBuffer[256]; 

    if (DiskInformationTable != NULL)
        free(DiskInformationTable);

	wchar_t* dosdevs = (wchar_t*)malloc(sizeof(wchar_t*) * 65536);
	if (dosdevs == NULL)
	{
		return;
	}

	QueryDosDeviceW(NULL, dosdevs, 65536);
	int diskCount = 0;
	for (wchar_t* pos = dosdevs; *pos; pos += lstrlenW(pos) + 1)
		if (wcsncmp(pos, L"PhysicalDrive", 13) == 0)
			diskCount++;

	DISK_INFORMATION* diskInfos = (DISK_INFORMATION*)malloc(sizeof(DISK_INFORMATION) * diskCount);
	if (diskInfos == NULL)
	{
		return;
	}

	int i = 0;
	for (wchar_t* pos = dosdevs; *pos; pos += lstrlenW(pos) + 1)
	{
		if (wcsncmp(pos, L"PhysicalDrive", 13) == 0)
		{
			// Pointers
			HANDLE diskFileHandle = 0;
			DRIVE_LAYOUT_INFORMATION_EX* dli = 0;
			STORAGE_DEVICE_DESCRIPTOR* psdd = 0;

			// Structures
			DISK_INFORMATION info;
			STORAGE_PROPERTY_QUERY spq;
			STORAGE_DEVICE_DESCRIPTOR sdd;
			DISK_GEOMETRY dg;

			// Integer variables
			DWORD byteCount;
			int partitionCount = 1;
			int dliSize;

			// Stuff for constructing the device name
			size_t bytesWritten = 0;
			size_t sizeOfSrcString;
			size_t sizeOfDestString;
			size_t count;
			char* srcPtr;
			wchar_t* destPtr;

			// Get disk number and path
			info.DiskNumber = wcstol(pos + 13, NULL, 10);
			swprintf(info.DiskPath, 64, L"\\\\.\\\PHYSICALDRIVE%d", info.DiskNumber);
			diskFileHandle = CreateFileW(info.DiskPath, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
			if (diskFileHandle == INVALID_HANDLE_VALUE)
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

			//memset(nameBuffer, 0, sizeof(wchar_t*) * 256);

			destPtr = nameBuffer;
			sizeOfSrcString = strlen((char*)psdd + psdd->VendorIdOffset);
			sizeOfDestString = 256;
			srcPtr = (char*)psdd + psdd->VendorIdOffset;
			count = _TRUNCATE;

			if (sizeOfSrcString > 0)
			{
				mbstowcs_s(&bytesWritten, destPtr, sizeOfDestString, srcPtr, count);
				nameBuffer[bytesWritten - 1] = L' ';
			}

			destPtr = nameBuffer + bytesWritten;
			sizeOfSrcString = strlen((char*)psdd + psdd->ProductIdOffset);
			sizeOfDestString = 256 - bytesWritten;
			srcPtr = (char*)psdd + psdd->ProductIdOffset;
			mbstowcs_s(&bytesWritten, destPtr, sizeOfDestString, srcPtr, count);

			{
				wchar_t* temp = CleanString(nameBuffer);
				if (temp)
				{
					lstrcpyW(info.DeviceName, temp);
					free(temp);
				}
			}

			// IOCTL_DISK_GET_DRIVE_GEOMETRY
			// Gets the absolute size of the disk (CHS)
			// Gets the media type (Standard/Removable)
			if (!DeviceIoControl(diskFileHandle, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &dg, sizeof(dg), &byteCount, NULL))
			{
				diskCount--;
				goto clean;
			}

			info.SectorSize = dg.BytesPerSector;
			info.SectorCount = dg.Cylinders.QuadPart * dg.TracksPerCylinder * dg.SectorsPerTrack;
			info.MediaType = dg.MediaType;

			// DRIVE_LAYOUT_INFORMATION_EX 
			// Gets the partition count according to what Windows perceives
			dliSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * (partitionCount++ - 1);
			dli = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(dliSize);
			if (!dli)
			{
				diskCount--;
				goto clean;
			}
			while (true)
			{
				if (DeviceIoControl(diskFileHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, dli, dliSize, &byteCount, NULL))
				{
					break;
				}

				if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				{
					diskCount--;
					goto clean;
				}

				free(dli);
				dliSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + sizeof(PARTITION_INFORMATION_EX) * (partitionCount++ - 1);
				dli = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(dliSize);
				if (!dli)
				{
					diskCount--;
					goto clean;
				}
			}

			info.PartitionCount = dli->PartitionCount;

			diskInfos[i] = info;
			i++;

		clean:

			if (diskFileHandle != 0) CloseHandle(diskFileHandle);
			if (psdd != 0) free(psdd);
			if (dli != 0) free(dli);
		}
	}

	// MBR/GPT
	// Get the parttable determined sector size and disk size (cross check with IOCTL_DISK_GET_DRIVE_GEOMETRY)
	// Get the partition count
	// Note: before selecting the disk, the partition table should not be accessed
	
	DiskInformationTable = diskInfos;
	DiskInformationTableSize = diskCount;
	free(dosdevs);
}

PartitionTableType PartitionManager::GetPartitionTableType(DISK_INFORMATION* diskInfo)
{
	// Open the disk for reading
	HANDLE hDisk = CreateFileW(diskInfo->DiskPath, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (hDisk == NULL)
		return PartitionTableType::Unknown;

	// Read in the first two sectors
	char* header = (char*)malloc(diskInfo->SectorSize * 2);
	ReadFile(hDisk, header, diskInfo->SectorSize * 2, NULL, NULL);

	// Check if a GPT is present
	GPT_HEADER* gpt = (GPT_HEADER*)(header + diskInfo->SectorSize);
	bool gptPresent = gpt->Signature == 0x5452415020494645ULL;

	// Check if there's an MBR present
	MBR_HEADER* mbr = (MBR_HEADER*)header;
	bool mbrPresent = mbr->BootSignature == 0xAA55;

	// Check if there's a protective MBR
	// My definition of a protective MBR:
	// A partition with one or more 0xEE partitions but no other partition types
	bool pmbrPresent;
	if (mbrPresent)
	{
		int efPartCount = 0;
		int normPartCount = 0;
		for (int i = 0; i < 4; i++)
		{
			if (mbr->PartitionTable[i].SystemID == '\xEE')
				efPartCount++;
			else if (mbr->PartitionTable[i].SystemID != 0)
				normPartCount++;
		}
		pmbrPresent = efPartCount >= 1 && normPartCount == 0;
	}
	else
		pmbrPresent = false;

	// Free all opened handles
	free(header);
	CloseHandle(hDisk);

	// Combine return value
	int type = (int)PartitionTableType::Unknown;
	if (gptPresent) type |= (int)PartitionTableType::GPT;
	if (pmbrPresent) type |= (int)PartitionTableType::PMBR;
	else if (mbrPresent) type |= (int)PartitionTableType::MBR;

	return (PartitionTableType)type;
}

bool PartitionManager::LoadDisk(DISK_INFORMATION* diskInfo, bool forceOperatingMode)
{
	PartitionTableType table = GetPartitionTableType(diskInfo);

	// GPT with no MBR is an invalid GPT
	if (table == PartitionTableType::GPT)
	{
		ShowMessagePage(L"The partition table on the disk could not be loaded: A GPT disk must contain a valid protective or hybrid MBR structure.", MessagePageType::OK, MessagePageUI::Error);
		return false;
	}

	// GPT with hybrid MBR is not recommended
	// and should only be modified with care
	if (!forceOperatingMode && table == PartitionTableType::GPT_HMBR)
	{
		MessagePageResult r = ShowMessagePage(L"The partition table on the disk is a GPT with a hybrid MBR structure. A hybrid MBR could cause significant damage to your data when used incorrectly. Would you like to modify the GPT? Selecting No will load the hybrid MBR instead.", MessagePageType::YesNoCancel, MessagePageUI::Warning);
		switch (r)
		{
		case MessagePageResult::Yes:
			CurrentDiskOperatingMode = OperatingMode::GPT;
			break;
		case MessagePageResult::No:
			CurrentDiskOperatingMode = OperatingMode::MBR;
			break;
		case MessagePageResult::Fail:
		case MessagePageResult::Cancel:
		default:
			return false;
		}
	}
	else if (!forceOperatingMode)
		CurrentDiskOperatingMode = (int)table & (int)PartitionTableType::GPT ? OperatingMode::GPT : OperatingMode::MBR;

	CurrentDisk = *diskInfo;
	CurrentDiskType = table;
	CurrentDiskPartitionsModified = false;
	CurrentDiskPartitionTableDestroyed = false;

	// Open the disk for reading
	HANDLE hDisk = CreateFileW(CurrentDisk.DiskPath, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (hDisk == NULL)
	{
		ShowMessagePage(L"The information on the disk could not be loaded because the disk could not be opened for reading: %s", MessagePageType::OK, MessagePageUI::Error);
		return false;
	}

	// Read in the first two sectors
	char* header = (char*)malloc(CurrentDisk.SectorSize * 2);
	if (header == NULL)
		return false;
	if (!ReadFile(hDisk, header, CurrentDisk.SectorSize * 2, NULL, NULL))
	{
		ShowMessagePage(L"The partition table on the disk could not be loaded because the data on the disk could not be accessed: %s", MessagePageType::OK, MessagePageUI::Error);
		return false;
	}

	// If there's a GPT, load it
	if ((int)table & (int)PartitionTableType::GPT)
		CurrentDiskGPT = *((GPT_HEADER*)(header + CurrentDisk.SectorSize));
	else
		CurrentDiskGPT = { 0 };

	// Load the normal/protective/hybrid MBR
	if ((int)table & (int)PartitionTableType::MBR || (int)table & (int)PartitionTableType::PMBR)
		CurrentDiskMBR = *((MBR_HEADER*)header);
	else
		CurrentDiskMBR = { 0 };

	if (CurrentDiskOperatingMode == OperatingMode::GPT)
	{
		int count = CurrentDiskGPT.TableEntryCount;
		unsigned long long PartitionTableSize = CurrentDiskGPT.TableEntrySize * count;
		CurrentDiskGPTTable = (GPT_ENTRY*)malloc(PartitionTableSize);

		unsigned long long newPtr = CurrentDiskGPT.TableLBA.ULL * CurrentDisk.SectorSize;
		//SetFilePointer(hDisk, CurrentDiskGPT->StartLBA.UL[0], (PLONG)CurrentDiskGPTTable->StartLBA.UL + 1, FILE_BEGIN);
		SetFilePointer(hDisk, static_cast<long>(newPtr), (PLONG)&newPtr + 1, FILE_BEGIN);
		if (!ReadFile(hDisk, CurrentDiskGPTTable, PartitionTableSize, NULL, NULL))
		{
			ShowMessagePage(L"The partition table on the disk could not be loaded because the data on the disk could not be accessed: %s", MessagePageType::OK, MessagePageUI::Error);
			return false;
		}
	}

	CloseHandle(hDisk);
	return LoadPartitionTable();
}

bool PartitionManager::LoadPartitionTable()
{
	// Start loading the partition table
	switch (CurrentDiskOperatingMode)
	{
	case OperatingMode::GPT:
	{
		PartitionInformation pi;
		int count = CurrentDiskGPT.TableEntryCount;

		CurrentDiskPartitionCount = 128;
		CurrentDiskPartitions = (PartitionInformation*)malloc(sizeof(PartitionInformation) * count);
		CurrentDiskFirstAvailablePartition = -1;

		GUID empty = { 0,0,0,0 }; int j = 0;
		for (int i = 0; i < 128; i++)
		{
			GPT_ENTRY* PartitionTableEntry = reinterpret_cast<GPT_ENTRY*>(reinterpret_cast<unsigned char*>(CurrentDiskGPTTable) + (CurrentDiskGPT.TableEntrySize * i));
			pi = { 0 };
			pi.DiskNumber = CurrentDisk.DiskNumber;
			pi.PartitionNumber = i + 1;
			pi.Type.TypeGUID = PartitionTableEntry->TypeGUID;
			if (!memcmp(&pi.Type.TypeGUID, &empty, sizeof(GUID)))
			{
				if (CurrentDiskFirstAvailablePartition == -1)
					CurrentDiskFirstAvailablePartition = pi.PartitionNumber;
				CurrentDiskPartitionCount--;
				continue;
			}
			pi.StartLBA = PartitionTableEntry->StartLBA;
			pi.EndLBA = PartitionTableEntry->EndLBA;
			pi.SectorCount = pi.EndLBA.ULL - pi.StartLBA.ULL + 1;
			memcpy_s(pi.Name, sizeof(wchar_t) * 36, PartitionTableEntry->Name, sizeof(wchar_t) * 36);
			memcpy_s(CurrentDiskPartitions + j, sizeof(PartitionInformation), &pi, sizeof(PartitionInformation));
			j++;
		}
		break;
	}
	case OperatingMode::MBR:
	{
		PartitionInformation pi;
		CurrentDiskPartitionCount = 4;
		CurrentDiskPartitions = (PartitionInformation*)malloc(sizeof(PartitionInformation) * CurrentDiskPartitionCount);

		for (int i = 0; i < 4; i++)
		{
			if (CurrentDiskMBR.PartitionTable[i].SystemID == 0)
			{
				CurrentDiskPartitionCount--;
				continue;
			}
			pi = { 0 };
			pi.DiskNumber = CurrentDisk.DiskNumber;
			pi.PartitionNumber = i + 1;
			pi.Type.SystemID = CurrentDiskMBR.PartitionTable[i].SystemID;
			pi.StartLBA = LBA{ static_cast<unsigned long long>(CurrentDiskMBR.PartitionTable[i].StartLBA) };
			pi.SectorCount = CurrentDiskMBR.PartitionTable[i].SectorCount;
			pi.EndLBA = LBA{ pi.StartLBA.ULL + pi.SectorCount - 1 };
			memcpy_s(CurrentDiskPartitions + i, sizeof(PartitionInformation), &pi, sizeof(PartitionInformation));
		}
	}
	}
	return true;
}

bool PartitionManager::SavePartitionTableToDisk()
{
	const wchar_t* oldStatus = CurrentPage->statusText;
	CurrentPage->SetStatusText(L"WinParted is writing the partition table to the disk...");
	CurrentPage->Update();

	// Open the disk for reading
	DWORD bytes;
	HANDLE hDisk = CreateFileW(CurrentDisk.DiskPath, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, 0);
	if (hDisk == NULL)
	{
		ShowMessagePage(L"The partition table could not be saved because the disk could not be opened for writing: %s Any changes made to the partition table will be discarded.", MessagePageType::OK, MessagePageUI::Error);
		return false;
	}

	switch (CurrentDiskOperatingMode)
	{
	case OperatingMode::GPT:
	{
		unsigned long long ptrBuffer = 0;
		unsigned long long PartitionTableSize = CurrentDiskGPT.TableEntrySize * CurrentDiskGPT.TableEntryCount;
		LBA backupGPTTable = { 0 };

		GPT_HEADER gptHeader = CurrentDiskGPT;
		char* gptBuffer = reinterpret_cast<char*>(malloc(CurrentDisk.SectorSize));

		// Determine where the backup GPT table lies
		if (CurrentDiskPartitionTableDestroyed)
		{
			// If a new GPT is being made, the table lies in the last sectors of the disk
			backupGPTTable.ULL = CurrentDisk.SectorCount
				- (PartitionTableSize / CurrentDisk.SectorSize)
				- 1;
		}
		else
		{
			// If an existing GPT is used, read the location of the table from the existing backup header
			ptrBuffer = gptHeader.BackupHeaderLBA.ULL * CurrentDisk.SectorSize;
			if (SetFilePointer(hDisk, static_cast<long>(ptrBuffer), (PLONG)&ptrBuffer + 1, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			{
				ShowMessagePage(L"The backup partition table could not be read because the disk could not seek to it: %s The main GPT table was saved succesfully, no information was lost. The partition table still needs to be repaired with other tools in order to create a valid backup GPT table.", MessagePageType::OK, MessagePageUI::Error);
				goto fail;
			}
			if (!ReadFile(hDisk, gptBuffer, CurrentDisk.SectorSize, &bytes, NULL))
			{
				ShowMessagePage(L"The backup GPT header could not be read: %s The main GPT header was saved succesfully, no information was lost. The partition table still needs to be repaired with other tools in order to create a valid backup GPT header.", MessagePageType::OK, MessagePageUI::Error);
				goto fail;
			}

			backupGPTTable = (*(GPT_HEADER*)gptBuffer).TableLBA;
		}

		// Calculate CRC's for main header
		gptHeader.TableCRC = CalculateCRC32((char*)CurrentDiskGPTTable, PartitionTableSize);
		gptHeader.HeaderCRC = 0;
		gptHeader.HeaderCRC = CalculateCRC32((char*)&gptHeader, CurrentDiskGPT.HeaderSize);

		// Write main GPT header
		if (SetFilePointer(hDisk, CurrentDisk.SectorSize, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			ShowMessagePage(L"The GPT header could not be saved because the disk could not seek to it: %s The partition table may have been corrupted. The backup GPT remains intact and can be used to restore your disk.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}
		*((GPT_HEADER*)gptBuffer) = gptHeader;
		if (!WriteFile(hDisk, gptBuffer, CurrentDisk.SectorSize, &bytes, NULL))
		{
			ShowMessagePage(L"The GPT header could not be saved: %s The partition table may have been corrupted. The backup GPT remains intact and can be used to restore your disk.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}

		// Write main GPT table
		ptrBuffer = gptHeader.TableLBA.ULL * CurrentDisk.SectorSize;
		if (SetFilePointer(hDisk, static_cast<long>(ptrBuffer), (PLONG)&ptrBuffer + 1, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			ShowMessagePage(L"The partition table could not be saved because the disk could not seek to it: %s The partition table will not be written and any unsaved changes will be lost.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}
		if (!WriteFile(hDisk, CurrentDiskGPTTable, PartitionTableSize, &bytes, NULL))
		{
			ShowMessagePage(L"The partition table could not be saved: %s The partition table will not be written and any unsaved changes will be lost.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}

		// Determine backup header location
		ptrBuffer = gptHeader.BackupHeaderLBA.ULL * CurrentDisk.SectorSize;

		// Calculate CRC for backup header and set LBA's
		gptHeader.HeaderCRC = 0;
		gptHeader.TableLBA = backupGPTTable;
		gptHeader.BackupHeaderLBA = gptHeader.CurrentHeaderLBA;
		gptHeader.CurrentHeaderLBA.ULL = ptrBuffer / CurrentDisk.SectorSize;
		gptHeader.HeaderCRC = CalculateCRC32((char*)&gptHeader, CurrentDiskGPT.HeaderSize);

		// Write backup GPT header
		if (SetFilePointer(hDisk, static_cast<long>(ptrBuffer), (PLONG)&ptrBuffer + 1, FILE_BEGIN) == INVALID_SET_FILE_POINTER) 
		{
			ShowMessagePage(L"The backup GPT header could not be saved because the disk could not seek to it: %s The main GPT table was saved succesfully, no information was lost. The partition table still needs to be repaired with other tools in order to create a valid backup GPT table.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}
		*((GPT_HEADER*)gptBuffer) = gptHeader;
		if (!WriteFile(hDisk, gptBuffer, CurrentDisk.SectorSize, &bytes, NULL))
		{
			ShowMessagePage(L"The backup GPT header could not be saved: %s The main GPT header was saved succesfully, no information was lost. The partition table still needs to be repaired with other tools in order to create a valid backup GPT header.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}

		// Write backup GPT table
		ptrBuffer = gptHeader.TableLBA.ULL * CurrentDisk.SectorSize;
		if (SetFilePointer(hDisk, static_cast<long>(ptrBuffer), (PLONG)&ptrBuffer + 1, FILE_BEGIN) == INVALID_SET_FILE_POINTER) 
		{
			ShowMessagePage(L"The backup partition table could not be saved because the disk could not seek to it: %s The main GPT table was saved succesfully, no information was lost. The partition table still needs to be repaired with other tools in order to create a valid backup GPT table.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}
		if (!WriteFile(hDisk, CurrentDiskGPTTable, PartitionTableSize, &bytes, NULL))
		{
			ShowMessagePage(L"The backup partition table could not be saved: %s The main GPT table was saved succesfully, no information was lost. The partition table still needs to be repaired with other tools in order to create a valid backup GPT table.", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}

		// Fall through to MBR because a GPT must always have either a protective or hybrid MBR
	}
	case OperatingMode::MBR:
		// Move to the start of the disk
		if (SetFilePointer(hDisk, 0, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			ShowMessagePage(L"The Master Boot Record could not be saved because the disk could not seek to it: %s", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}

		// Write the new MBR
		if (!WriteFile(hDisk, &CurrentDiskMBR, CurrentDisk.SectorSize, &bytes, NULL))
		{
			ShowMessagePage(L"The Master Boot Record could not be saved: %s", MessagePageType::OK, MessagePageUI::Error);
			goto fail;
		}
		break;
	}

	ShowMessagePage(L"The partition table was succesfully written to the disk.");
	if (!DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytes, NULL))
	{
		ShowMessagePage(L"WinParted was not able to synchronize the partition table. Your computer might use the old partition table until you reboot or reconnect the disk!", MessagePageType::OK, MessagePageUI::Warning);
	}

	for (int i = 0; i < DiskInformationTableSize; i++)
		if (DiskInformationTable[i].DiskNumber == CurrentDisk.DiskNumber)
			LoadDisk(&DiskInformationTable[i], true);

	CloseHandle(hDisk);
	CurrentPage->SetStatusText(oldStatus);
	CurrentPage->Update();
	return true;
fail:
	CloseHandle(hDisk);
	CurrentPage->SetStatusText(oldStatus);
	CurrentPage->Update();
	return false;
}

bool PartitionManager::DeletePartition(PartitionInformation* partInfo)
{
	int partIndex = partInfo->PartitionNumber - 1;
	switch (CurrentDiskOperatingMode)
	{
	case OperatingMode::GPT:
	{
		GPT_ENTRY* PartitionTableEntry = reinterpret_cast<GPT_ENTRY*>(reinterpret_cast<unsigned char*>(CurrentDiskGPTTable) + (CurrentDiskGPT.TableEntrySize * partIndex));
		ZeroMemory(PartitionTableEntry, CurrentDiskGPT.TableEntrySize);
		CurrentDiskPartitionCount--;
	}
		break;
	case OperatingMode::MBR:
		ZeroMemory(CurrentDiskMBR.PartitionTable + partIndex, sizeof(MBR_ENTRY));
		break;
	}

	CurrentDiskPartitionsModified = true;
	if (!LoadPartitionTable())
	{
		ShowMessagePage(L"Deleting the partition failed as the partition table could not be reloaded. The partition table may have been corrupted. WinParted will now return to the disk selection page. Any unsaved changes to the current disk will be discarded.", MessagePageType::OK, MessagePageUI::Error);
		Restart();
		return false;
	}
}

HRESULT PartitionManager::ApplyPartitionLayoutGPT(WP_PART_LAYOUT* layout)
{
	DWORD dwSize;
	HANDLE hDisk;

	wprintf_s(L"Applying the following partition layout to disk %d:\n", CurrentDisk.DiskNumber);
	for (int i = 0; i < layout->PartitionCount; i++)
		if (layout->Partitions[i].MountPoint == NULL)
			wprintf_s(L"%3d | %4hX | %5s | %s\n", layout->Partitions[i].PartitionNumber, layout->Partitions[i].PartitionType, layout->Partitions[i].FileSystem, layout->Partitions[i].PartitionSize);
		else
			wprintf_s(L"%3d | %4hX | %5s | %s | %s\n", layout->Partitions[i].PartitionNumber, layout->Partitions[i].PartitionType, layout->Partitions[i].FileSystem, layout->Partitions[i].PartitionSize, layout->Partitions[i].MountPoint);
	wprintf_s(L"\n");

	auto MakeCHS = [](unsigned short C, unsigned short H, unsigned short S)
	{
		char b1 = static_cast<char>(H);
		char b2 = static_cast<char>(((C & 0b1100000000) >> 8) + (S & 0b111111));
		char b3 = static_cast<char>(C * 0b11111111);

		return CHS{ b1, b2, b3 };
	};

	// Create empty partition tables
	ZeroMemory(&CurrentDiskMBR, sizeof(MBR_HEADER));
	CurrentDiskMBR.BootSignature = 0xAA55;
	CurrentDiskMBR.PartitionTable[0].BootIndicator = 0;
	CurrentDiskMBR.PartitionTable[0].SystemID = 0xEE;
	CurrentDiskMBR.PartitionTable[0].StartCHS = MakeCHS(0, 0, 2);
	CurrentDiskMBR.PartitionTable[0].EndCHS = MakeCHS(1023, 254, 63);
	CurrentDiskMBR.PartitionTable[0].StartLBA = 1UL;
	CurrentDiskMBR.PartitionTable[0].SectorCount = CurrentDisk.SectorCount - 1;

	ZeroMemory(&CurrentDiskGPT, sizeof(GPT_HEADER));
	CurrentDiskGPT.Signature = 0x5452415020494645ULL;
	CurrentDiskGPT.Revision = 0x00010000;
	CurrentDiskGPT.CurrentHeaderLBA.ULL = 1;
	CurrentDiskGPT.BackupHeaderLBA.ULL = CurrentDisk.SectorCount - 1;
	CurrentDiskGPT.HeaderSize = sizeof(GPT_HEADER);
	CurrentDiskGPT.TableEntryCount = 128;
	CurrentDiskGPT.TableEntrySize = sizeof(GPT_ENTRY);
	CurrentDiskGPT.TableLBA.ULL = 2;
	int tableSize = CurrentDiskGPT.TableEntryCount * CurrentDiskGPT.TableEntrySize / CurrentDisk.SectorSize;
	CoCreateGuid(&CurrentDiskGPT.DiskGUID);
	CurrentDiskGPT.FirstUsableLBA.ULL = tableSize + 2;
	CurrentDiskGPT.LastUsableLBA.ULL = CurrentDisk.SectorCount - tableSize - 2;

	const unsigned long blockSize = 2048;
	unsigned long totalCapacity = CurrentDisk.SectorCount / blockSize;
	unsigned long availableCapacity = totalCapacity - 2;

	wprintf_s(L"Partitions will be aligned on %d sector blocks.\n\n", blockSize);
	wprintf_s(L"Virtual sector size: %d bytes\n", CurrentDisk.SectorSize);
	wprintf_s(L"Total capacity:      %d blocks\n", totalCapacity);
	wprintf_s(L"Available space:     %d blocks\n\n", availableCapacity);
	wprintf_s(L"Partition sizes of value-based-partitions in blocks:\n");

	int partitionsParsed = 0;
	PartitionInformation* partitions = reinterpret_cast<PartitionInformation*>(malloc(sizeof(PartitionInformation) * layout->PartitionCount));
	ZeroMemory(partitions, sizeof(PartitionInformation) * layout->PartitionCount);
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		// Loop through value-based partitions (without %)
		if (wcsstr(layout->Partitions[i].PartitionSize, L"%") != 0)
			continue;

		unsigned long long partitionSize;
		wchar_t partitionSizeModifier;
		swscanf_s(layout->Partitions[i].PartitionSize, L"%4lld%c", &partitionSize, &partitionSizeModifier);

		const wchar_t* sizeModifiers = L"BKMGTPEZY";
		int index = ((unsigned long long)wcschr(sizeModifiers, partitionSizeModifier) - (unsigned long long)sizeModifiers) / sizeof(wchar_t);
		for (int j = 0; j < index; j++)
			partitionSize *= 1024;
		partitionSize /= CurrentDisk.SectorSize;
		partitionSize /= blockSize;
		availableCapacity -= partitionSize;

		partitions[i].PartitionNumber = layout->Partitions[i].PartitionNumber;
		partitions[i].SectorCount = partitionSize * blockSize;
		memcpy(&partitions[i].Type.TypeGUID, GetGUIDFromPartitionTypeCode(layout->Partitions[i].PartitionType), sizeof(GUID));

		wprintf_s(L"%3d: %d blocks\n", partitions[i].PartitionNumber, partitionSize);
		partitionsParsed++;
	}

	unsigned long long capacityRemaining = availableCapacity;
	wprintf_s(L"\nBlocks available after subtracting value-based partitions: %d blocks\n\n", availableCapacity);
	wprintf_s(L"Partition sizes of percentage-based partitions in blocks:\n");
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		// Loop through percentage-based partitions (with %)
		if (wcsstr(layout->Partitions[i].PartitionSize, L"%") == 0)
			continue;

		unsigned long long partitionSize;
		swscanf_s(layout->Partitions[i].PartitionSize, L"%4lld%%", &partitionSize);

		if (partitionsParsed < layout->PartitionCount - 1)
			partitionSize = partitionSize * availableCapacity / 100;
		else
			partitionSize = capacityRemaining;
		capacityRemaining -= partitionSize;

		partitions[i].PartitionNumber = layout->Partitions[i].PartitionNumber;
		partitions[i].SectorCount = partitionSize * blockSize;
		memcpy(&partitions[i].Type.TypeGUID, GetGUIDFromPartitionTypeCode(layout->Partitions[i].PartitionType), sizeof(GUID));

		wprintf_s(L"%3d: %d blocks\n", partitions[i].PartitionNumber, partitionSize);
		partitionsParsed++;
	}

	wprintf_s(L"\nProposed partition table:\n");
	wprintf_s(L"Part #  Partition type         Start        End          Size\n");

	unsigned long long currentSector = 2048;
	wchar_t sizeBuffer[10];
	if (!CurrentDiskGPTTable)
		CurrentDiskGPTTable = reinterpret_cast<GPT_ENTRY*>(malloc(sizeof(GPT_ENTRY) * CurrentDiskGPT.TableEntryCount));
	ZeroMemory(CurrentDiskGPTTable, sizeof(GPT_ENTRY) * CurrentDiskGPT.TableEntryCount);
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		partitions[i].StartLBA.ULL = currentSector;
		currentSector += partitions[i].SectorCount;
		partitions[i].EndLBA.ULL = currentSector - 1;
		partitions[i].DiskNumber = CurrentDisk.DiskNumber;
		const wchar_t* name = GetStringFromPartitionTypeGUID(partitions[i].Type.TypeGUID);
		memcpy(partitions[i].Name, name, 36 * sizeof(wchar_t));
		partitions[i].Name[35] = 0;

		GetSizeStringFromBytes(partitions[i].SectorCount * CurrentDisk.SectorSize, sizeBuffer);
		wprintf(L"%6d  %-21s  %-11lld  %-11lld  %s\n", partitions[i].PartitionNumber,
			partitions[i].Name,
			partitions[i].StartLBA.ULL,
			partitions[i].EndLBA.ULL,
			sizeBuffer);

		memcpy(CurrentDiskGPTTable[i].Name, partitions[i].Name, 36 * sizeof(wchar_t));
		CurrentDiskGPTTable[i].StartLBA = partitions[i].StartLBA;
		CurrentDiskGPTTable[i].EndLBA = partitions[i].EndLBA;
		// Set the partition types to Basic Data in order to format
		CurrentDiskGPTTable[i].TypeGUID = partitions[i].Type.TypeGUID;
		/*if (lstrcmpW(layout->Partitions[i].FileSystem, L"RAW"))
			memcpy(&CurrentDiskGPTTable[i].TypeGUID, GetGUIDFromPartitionTypeCode(0x0700), sizeof(GUID));
		else
			CurrentDiskGPTTable[i].TypeGUID = partitions[i].Type.TypeGUID;*/
		
		CoCreateGuid(&CurrentDiskGPTTable[i].UniqueGUID);
	}

	// Finalize partition table and write it to the disk
	CurrentDiskType = PartitionTableType::GPT_PMBR;
	CurrentDiskOperatingMode = OperatingMode::GPT;
	CurrentDiskPartitionTableDestroyed = true;
	SavePartitionTableToDisk();

	// Wait for VDS
	CurrentPage->SetStatusText(L"Waiting while VDS refreshes the partition table...");
	CurrentPage->Update();
	Sleep(10000);

	// Format partitions
	CurrentPage->SetStatusText(L"WinParted is formatting the partitions on the disk...");
	CurrentPage->Update();
	HRESULT result = ERROR_SUCCESS;
	for (int i = 0; i < layout->PartitionCount; i++) 
	{
		if (lstrcmpW(layout->Partitions[i].FileSystem, L"RAW"))
		{
			HRESULT res = FormatAndOrMountPartition(&CurrentDiskPartitions[i], layout->Partitions[i].FileSystem, layout->Partitions[i].MountPoint);
			if (res != S_OK)
			{
				result = res;
				break;
			}
		}
	}
	free(partitions);
	return result;
}

HRESULT PartitionManager::ApplyPartitionLayoutMBR(WP_PART_LAYOUT* layout)
{
	DWORD dwSize;
	HANDLE hDisk;

	wprintf_s(L"Applying the following partition layout to disk %d:\n", CurrentDisk.DiskNumber);
	for (int i = 0; i < layout->PartitionCount; i++)
		if (layout->Partitions[i].MountPoint == NULL)
			wprintf_s(L"%3d | %4hX | %5s | %s\n", layout->Partitions[i].PartitionNumber, layout->Partitions[i].PartitionType, layout->Partitions[i].FileSystem, layout->Partitions[i].PartitionSize);
		else
			wprintf_s(L"%3d | %4hX | %5s | %s | %s\n", layout->Partitions[i].PartitionNumber, layout->Partitions[i].PartitionType, layout->Partitions[i].FileSystem, layout->Partitions[i].PartitionSize, layout->Partitions[i].MountPoint);
	wprintf_s(L"\n");

	if (layout->PartitionCount > 4) 
	{
		wprintf_s(L"The supplied partition layout has too many partitions.");
		return ERROR_INVALID_PARAMETER;
	}

	auto MakeCHS = [](unsigned short C, unsigned short H, unsigned short S)
	{
		char b1 = static_cast<char>(H);
		char b2 = static_cast<char>(((C & 0b1100000000) >> 8) + (S & 0b111111));
		char b3 = static_cast<char>(C * 0b11111111);

		return CHS{ b1, b2, b3 };
	};

	// Create partition table
	ZeroMemory(&CurrentDiskMBR, sizeof(MBR_HEADER));
	CurrentDiskMBR.BootSignature = 0xAA55;
	CurrentDiskMBR.PartitionTable[0].BootIndicator = 0;

	const unsigned long blockSize = 2048;
	unsigned long totalCapacity = CurrentDisk.SectorCount / blockSize;
	unsigned long availableCapacity = totalCapacity - 1;

	wprintf_s(L"Partitions will be aligned on %d sector blocks.\n\n", blockSize);
	wprintf_s(L"Virtual sector size: %d bytes\n", CurrentDisk.SectorSize);
	wprintf_s(L"Total capacity:      %d blocks\n", totalCapacity);
	wprintf_s(L"Available space:     %d blocks\n\n", availableCapacity);
	wprintf_s(L"Partition sizes of value-based-partitions in blocks:\n");

	int partitionsParsed = 0;
	PartitionInformation* partitions = reinterpret_cast<PartitionInformation*>(malloc(sizeof(PartitionInformation) * layout->PartitionCount));
	ZeroMemory(partitions, sizeof(PartitionInformation) * layout->PartitionCount);
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		// Loop through value-based partitions (without %)
		if (wcsstr(layout->Partitions[i].PartitionSize, L"%") != 0)
			continue;

		unsigned long long partitionSize;
		wchar_t partitionSizeModifier;
		swscanf_s(layout->Partitions[i].PartitionSize, L"%4lld%c", &partitionSize, &partitionSizeModifier);

		const wchar_t* sizeModifiers = L"BKMGTPEZY";
		int index = ((unsigned long long)wcschr(sizeModifiers, partitionSizeModifier) - (unsigned long long)sizeModifiers) / sizeof(wchar_t);
		for (int j = 0; j < index; j++)
			partitionSize *= 1024;
		partitionSize /= CurrentDisk.SectorSize;
		partitionSize /= blockSize;
		availableCapacity -= partitionSize;

		partitions[i].PartitionNumber = layout->Partitions[i].PartitionNumber;
		partitions[i].SectorCount = partitionSize * blockSize;
		partitions[i].Type.SystemID = layout->Partitions[i].PartitionType >> 8;
		//memcpy(&partitions[i].Type.TypeGUID, GetGUIDFromPartitionTypeCode(layout->Partitions[i].PartitionType), sizeof(GUID));

		wprintf_s(L"%3d: %d blocks\n", partitions[i].PartitionNumber, partitionSize);
		partitionsParsed++;
	}

	unsigned long long capacityRemaining = availableCapacity;
	wprintf_s(L"\nBlocks available after subtracting value-based partitions: %d blocks\n\n", availableCapacity);
	wprintf_s(L"Partition sizes of percentage-based partitions in blocks:\n");
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		// Loop through percentage-based partitions (with %)
		if (wcsstr(layout->Partitions[i].PartitionSize, L"%") == 0)
			continue;

		unsigned long long partitionSize;
		swscanf_s(layout->Partitions[i].PartitionSize, L"%4lld%%", &partitionSize);

		if (partitionsParsed < layout->PartitionCount - 1)
			partitionSize = partitionSize * availableCapacity / 100;
		else
			partitionSize = capacityRemaining;
		capacityRemaining -= partitionSize;

		partitions[i].PartitionNumber = layout->Partitions[i].PartitionNumber;
		partitions[i].SectorCount = partitionSize * blockSize;
		partitions[i].Type.SystemID = layout->Partitions[i].PartitionType >> 8;
		//memcpy(&partitions[i].Type.TypeGUID, GetGUIDFromPartitionTypeCode(layout->Partitions[i].PartitionType), sizeof(GUID));

		wprintf_s(L"%3d: %d blocks\n", partitions[i].PartitionNumber, partitionSize);
		partitionsParsed++;
	}

	wprintf_s(L"\nProposed partition table:\n");
	wprintf_s(L"Part #  Partition type         Start        End          Size\n");

	unsigned long long currentSector = 2048;
	wchar_t sizeBuffer[10];
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		partitions[i].StartLBA.ULL = currentSector;
		currentSector += partitions[i].SectorCount;
		partitions[i].EndLBA.ULL = currentSector - 1;
		partitions[i].DiskNumber = CurrentDisk.DiskNumber;
		const wchar_t* name = GetStringFromPartitionTypeGUID(partitions[i].Type.TypeGUID);
		memcpy(partitions[i].Name, name, 36 * sizeof(wchar_t));
		partitions[i].Name[35] = 0;

		GetSizeStringFromBytes(partitions[i].SectorCount * CurrentDisk.SectorSize, sizeBuffer);
		wprintf(L"%6d  %-21s  %-11lld  %-11lld  %s\n", partitions[i].PartitionNumber,
			partitions[i].Name,
			partitions[i].StartLBA.ULL,
			partitions[i].EndLBA.ULL,
			sizeBuffer);

		CurrentDiskMBR.PartitionTable[i].StartLBA = static_cast<unsigned long>(partitions[i].StartLBA.ULL);
		CurrentDiskMBR.PartitionTable[i].SectorCount = partitions[i].SectorCount;
		CurrentDiskMBR.PartitionTable[i].SystemID = partitions[i].Type.SystemID;
		CurrentDiskMBR.PartitionTable[i].BootIndicator = layout->Partitions[i].PartitionType & 0xff;
	}

	// Finalize partition table and write it to the disk
	CurrentDiskType = PartitionTableType::MBR;
	CurrentDiskOperatingMode = OperatingMode::MBR;
	CurrentDiskPartitionTableDestroyed = true;
	SavePartitionTableToDisk();

	// Wait for VDS
	CurrentPage->SetStatusText(L"Waiting while VDS refreshes the partition table...");
	CurrentPage->Update();
	Sleep(10000);

	// Format partitions
	CurrentPage->SetStatusText(L"WinParted is formatting the partitions on the disk...");
	CurrentPage->Update();
	HRESULT result = ERROR_SUCCESS;
	for (int i = 0; i < layout->PartitionCount; i++)
	{
		if (lstrcmpW(layout->Partitions[i].FileSystem, L"RAW"))
		{
			HRESULT res = FormatAndOrMountPartition(&CurrentDiskPartitions[i], layout->Partitions[i].FileSystem, layout->Partitions[i].MountPoint);
			if (res != S_OK)
			{
				result = res;
				break;
			}
		}
	}
	free(partitions);
	return result;
}

bool PartitionManager::LoadPartition(PartitionInformation* partition)
{
	TCHAR szVolumeName[MAX_PATH];
	TCHAR szDiskName[MAX_PATH];
	HANDLE hVolume = 0;
	HANDLE hDisk = 0;
	HANDLE search = 0;
	BOOL itemFound;
	BOOL res;

	DWORD dliSize;
	DRIVE_LAYOUT_INFORMATION_EX* dli;

	int partitionCount = 0;
	bool success = true;

	CurrentPartition = *partition;
	if (CurrentPartition.VolumeLoaded)
		return true;

	// Find out which volume belongs to the partition

	// Get disk layout
	swprintf(szDiskName, MAX_PATH, L"\\\\.\\\PHYSICALDRIVE%d", CurrentPartition.DiskNumber);
	if ((hDisk = CreateFile(szDiskName, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE)
		goto fail;

	dliSize = offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry[++partitionCount]);
	dli = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(dliSize);
	res = DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, dli, dliSize, NULL, NULL);
	while (!res)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			dliSize = offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry[++partitionCount]);
			dli = (DRIVE_LAYOUT_INFORMATION_EX*)realloc(dli, dliSize);
			res = DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, dli, dliSize, NULL, NULL);
		}
		else goto fail;
	}
	
	// Enumerate
	// FindFirstVolume -> \\?\Volume{guid}\ 
	search = FindFirstVolume(szVolumeName, MAX_PATH);
	itemFound = search != INVALID_HANDLE_VALUE;
	while (itemFound)
	{
		// Remove backslash
		// \\?\Volume{guid}\ -> \\?\Volume{guid}
		szVolumeName[lstrlen(szVolumeName) - 1] = 0;

		// Open volume
		// \\?\Volume{guid} -> CreateFileW (FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE) (FILE_SHARE_READ | FILE_SHARE_WRITE)
		if ((hVolume = CreateFile(szVolumeName, FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL)) == INVALID_HANDLE_VALUE)
			goto fail;

		// Check if extents match
		// DeviceIoControl(IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS)
		DWORD vdeSize = sizeof(VOLUME_DISK_EXTENTS);
		VOLUME_DISK_EXTENTS* vde = (VOLUME_DISK_EXTENTS*)malloc(vdeSize);
		res = DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, vdeSize, NULL, NULL);
		while (!res)
		{
			if (GetLastError() == ERROR_MORE_DATA)
			{
				vde = (VOLUME_DISK_EXTENTS*)realloc(vde, vdeSize);
				res = DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, vdeSize, NULL, NULL);
			}
			else if (GetLastError() == ERROR_INVALID_FUNCTION) goto cleanup;
			else goto fail;
		}
		
		for (int i = 0; i < vde->NumberOfDiskExtents; i++)
		{
			if (vde->Extents[i].DiskNumber == CurrentPartition.DiskNumber)
			{
				// DeviceIoControl(IOCTL_DISK_GET_DRIVE_LAYOUT_EX)
				if (vde->Extents[i].StartingOffset.QuadPart >= dli->PartitionEntry[CurrentPartition.PartitionNumber - 1].StartingOffset.QuadPart &&
					vde->Extents[i].StartingOffset.QuadPart < dli->PartitionEntry[CurrentPartition.PartitionNumber - 1].StartingOffset.QuadPart + dli->PartitionEntry[CurrentPartition.PartitionNumber - 1].PartitionLength.QuadPart)
				{
					// Get volume name + filesystem type
					// GetVolumeInformationByHandle
					if (!GetVolumeInformationByHandleW(hVolume, CurrentPartition.VolumeInformation.VolumeName, 128, NULL, NULL, NULL, CurrentPartition.VolumeInformation.FileSystem, 16))
						goto fail;

					lstrcpyW(CurrentPartition.VolumeInformation.VolumeFile, szVolumeName);
					CurrentPartition.VolumeLoaded = true;
					goto cleanup;
				}
			}
		}

		CloseHandle(hVolume);
		itemFound = FindNextVolume(search, szVolumeName, MAX_PATH);
	}

	goto cleanup;
	fail:
	//DebugBreak();
	success = false;
	cleanup:
	FindVolumeClose(search);
	CloseHandle(hDisk);
	return success;
}

bool PartitionManager::SetCurrentPartitionType(short value)
{
	int partIndex = CurrentPartition.PartitionNumber - 1;
	switch (CurrentDiskOperatingMode)
	{
	case OperatingMode::GPT:
	{
		GPT_ENTRY* gptEntry = reinterpret_cast<GPT_ENTRY*>(reinterpret_cast<unsigned char*>(CurrentDiskGPTTable) + (CurrentDiskGPT.TableEntrySize * (partIndex)));

		GUID oldType;
		if (partIndex == -1) oldType = *GetGUIDFromPartitionTypeCode(0);
		else oldType = gptEntry->TypeGUID;

		gptEntry->TypeGUID = *GetGUIDFromPartitionTypeCode(value);
		if (value == 0)
		{
			memset(gptEntry->Name, 0, sizeof(wchar_t) * 36);
			break;
		}

		const wchar_t* oldName = GetStringFromPartitionTypeGUID(oldType);
		if (lstrcmpW(gptEntry->Name, oldName))
			break; 

		const wchar_t* name = GetStringFromPartitionTypeCode(value);
		memset(gptEntry->Name, 0, sizeof(wchar_t) * 36);
		memcpy(gptEntry->Name, name, (lstrlenW(name) + 1) * sizeof(wchar_t));

		break;
	}
	case OperatingMode::MBR:
		CurrentDiskMBR.PartitionTable[partIndex].SystemID = (char)(value >> 8);
		break;
	}

	CurrentDiskPartitionsModified = true;

	if (!LoadPartitionTable())
	{
		ShowMessagePage(L"Setting the partition type failed as the partition table could not be reloaded. WinParted will now return to the disk selection page. Any unsaved changes to the current disk will be discarded.", MessagePageType::OK, MessagePageUI::Error);
		Restart();
		return false;
	}
	if (!LoadPartition(&CurrentDiskPartitions[partIndex]))
	{
		ShowMessagePage(L"Setting the partition type failed as the partition information could not be reloaded. WinParted will now return to the disk selection page. Any unsaved changes to the current disk will be discarded.", MessagePageType::OK, MessagePageUI::Error);
		Restart();
		return false;
	}

	return true;
}

bool PartitionManager::SetCurrentPartitionGuid(GUID value)
{
	int partIndex = CurrentPartition.PartitionNumber - 1;
	GPT_ENTRY* gptEntry = reinterpret_cast<GPT_ENTRY*>(reinterpret_cast<unsigned char*>(CurrentDiskGPTTable) + (CurrentDiskGPT.TableEntrySize * (partIndex)));
	gptEntry->UniqueGUID = value;

	CurrentDiskPartitionsModified = true;

	if (!LoadPartitionTable())
	{
		ShowMessagePage(L"Setting the partition GUID failed as the partition table could not be reloaded. WinParted will now return to the disk selection page. Any unsaved changes to the current disk will be discarded.", MessagePageType::OK, MessagePageUI::Error);
		Restart();
		return false;
	}
	if (!LoadPartition(&CurrentDiskPartitions[partIndex]))
	{
		ShowMessagePage(L"Setting the partition GUID failed as the partition information could not be reloaded. WinParted will now return to the disk selection page. Any unsaved changes to the current disk will be discarded.", MessagePageType::OK, MessagePageUI::Error);
		Restart();
		return false;
	}
}

const wchar_t* PartitionManager::GetOperatingModeString()
{
	switch (CurrentDiskOperatingMode)
	{
	case OperatingMode::GPT:
		return L"GPT";
	case OperatingMode::MBR:
		return L"MBR";
	}
	return L"unknown";
}

const wchar_t* PartitionManager::GetOperatingModeExtraString()
{
	switch (CurrentDiskType)
	{
	case PartitionTableType::GPT_PMBR:
		return L" (protective MBR)";
	case PartitionTableType::GPT_HMBR:
		return L" (hybrid MBR, dangerous!)";
	}
	return L"";
}

void PartitionManager::GetSizeStringFromBytes(unsigned long long bytes, wchar_t buffer[10])
{
	// Overkill
	bool useSI = false;
	double factor = useSI ? 1000.0 : 1024.0;
	const wchar_t valueStringsSI[9][4] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB" };
	const wchar_t valueStringsBinary[9][4] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB", L"PiB", L"EiB", L"ZiB", L"YiB" };

	int index = 0;
	double bytesFp = bytes;
	while (bytesFp > factor)
	{
		bytesFp /= factor;
		index++;
	}

	swprintf(buffer, 10, L"%.1f%s", bytesFp, useSI ? valueStringsSI[index] : valueStringsBinary[index]);
}

void PartitionManager::GetGuidStringFromStructure(GUID guid, wchar_t buffer[37])
{
	swprintf(buffer, 37, L"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

void PartitionManager::GetGuidStructureFromString(GUID* guid, const wchar_t buffer[37])
{
	swscanf_s(buffer, L"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		&guid->Data1, &guid->Data2, &guid->Data3,
		&guid->Data4[0], &guid->Data4[1], &guid->Data4[2], &guid->Data4[3],
		&guid->Data4[4], &guid->Data4[5], &guid->Data4[6], &guid->Data4[7]);
}

long PartitionManager::CalculateCRC32(char* data, unsigned long long len)
{
	// Code ripped and modified from here:
	// https://lxp32.github.io/docs/a-simple-example-crc32-calculation/
	// Generate a CRC table with the CRC32 polynomial
	
	/*long crc = 0xFFFFFFFF;
	for (size_t i = 0; i < len; i++) {
		char ch = data[i];
		for (long j = 0; j < 8; j++) {
			long b = (ch ^ crc) & 1;
			crc >>= 1;
			if (b) crc = crc ^ 0xEDB88320;
			ch >>= 1;
		}
	}
	return crc;*/

	// Code ripped from here
	// gdisk
	
	unsigned long table[256];
    unsigned long polynomial = 0xEDB88320;
	for (int i = 0; i < 256; i++)
	{
		unsigned long c = i;
		for (int j = 0; j < 8; j++)
		{
			if (c & 1) {
				c = polynomial ^ (c >> 1);
			}
			else {
				c >>= 1;
			}
		}
		table[i] = c;
	}

	// Calculate the CRC
	register unsigned long c = 0xFFFFFFFF;
	for (unsigned long i = 0; i < len; i++)
	{
		c = ((c >> 8) & 0x00FFFFFF) ^ table[(c ^ *data++) & 0xFF];
	}

	return c ^ 0xFFFFFFFF;
}
