#include "PartitionFormatPage.h"
#include "..\CoreFunctions\PartitionManager.h"

/*

 WinParted 1.2.0m12
========================

   Partitioning Disk 0. (AMD-RAID Array)
   Modifying partition 0. (Microsoft Basic Data)
   
   Select the new filesystem type and name of the partition.
   
   *  To select a filesystem type, use the UP and DOWN keys.
   *  To switch to entering the name, press the TAB key.
   *  To format the partition, press the ENTER key.

   ╔═════════╗
   ║  FAT32  ║
   ║  NTFS   ║
   ║  ExFAT  ║
   ╚═════════╝
   
   Name: FileSystemNameBlaBla

████████████████████████████████████████████████████████████████████████████████
*/

void PartitionFormatPage::InitPage()
{
	SetStatusText(L"Querying supported filesystems...");
	Draw();

	wchar_t* fileSystems;
	PartitionManager::QueryPartitionSupportedFilesystems(&PartitionManager::CurrentPartition, &fileSystems);
	wlogf(PartitionManager::GetLogger(), PANTHER_LL_BASIC, 128, L"Found filesystems: %s", fileSystems);
	
	wchar_t* context; 
	wchar_t* token = wcstok_s(fileSystems, L"|", &context);
	supportedFsCount = 0;
	while (token != NULL)
	{
		int tokenSize = lstrlenW(token);
		supportedFileSystems[supportedFsCount] = (wchar_t*)malloc(tokenSize + 1);
		if (!supportedFileSystems[supportedFsCount])
			continue;
		wcscpy_s(supportedFileSystems[supportedFsCount++], tokenSize + 1, token);
		biggestFsName = max(biggestFsName, tokenSize);

		token = wcstok_s(NULL, L"|", &context);
	}
	SetStatusText(L"");
}

void PartitionFormatPage::DrawPage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();
	int bufferSize = consoleSize.cx + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_LIGHTFG);
	
	console->SetPosition(3, 4);
	swprintf(buffer, bufferSize, L"Partitioning Disk %d. (%s)", PartitionManager::CurrentDisk.DiskNumber, PartitionManager::CurrentDisk.DeviceName);
	console->Write(buffer);
	if (PartitionManager::CurrentDiskPartitionsModified)
		console->Write(L" (partitions modified)");

	console->SetForegroundColor(CONSOLE_COLOR_FG);
	console->SetPosition(3, console->GetPosition().y + 1);
	if (PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT)
		swprintf(buffer, bufferSize, L"Modifying Partition %d. (%s)", PartitionManager::CurrentPartition.PartitionNumber, PartitionManager::CurrentPartition.Name);
	else
		swprintf(buffer, bufferSize, L"Modifying Partition %d.", PartitionManager::CurrentPartition.PartitionNumber);
	console->Write(buffer);

	console->SetForegroundColor(CONSOLE_COLOR_LIGHTFG);
	console->SetPosition(3, console->GetPosition().y + 2);
	console->Write(L"Select the new filesystem type and name of the partition.");
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To select the desired filesystem type, use the UP and DOWN keys.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To switch to entering the volume name, press the TAB key.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To format the partition, press the ENTER key.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To go back to the partition information screen, press ESC");

	drawY = console->GetPosition().y + 2;
}

void PartitionFormatPage::UpdatePage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();

	unsigned long long bufferSize = consoleSize.cx + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	int boxHeight = min(consoleSize.cy - drawY - 5, supportedFsCount + 2);
	console->DrawBox(3, drawY, biggestFsName + 6, boxHeight, true);

	maxItems = boxHeight - 2;
	for (int i = 0; i < maxItems; i++)
	{
		if (i == selectionIndex)
		{
			console->SetBackgroundColor(CONSOLE_COLOR_FG);
			console->SetForegroundColor(CONSOLE_COLOR_BG);
		}
		else
		{
			console->SetBackgroundColor(CONSOLE_COLOR_BG);
			console->SetForegroundColor(CONSOLE_COLOR_FG);
		}

		int j = i + scrollIndex;
		if (j > supportedFsCount) break;

		swprintf_s(buffer, bufferSize, L"%s", supportedFileSystems[j]);

		console->SetPosition(6, drawY + 1 + i);
		console->Write(buffer);
	}

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	console->DrawTextLeft(L"Name: ", consoleSize.cx - 6, drawY + boxHeight + 1);
	POINT p = console->GetPosition();
	for (int i = 0; i <= 63; i++)
		console->Write(L" ");
	console->SetPosition(p.x, p.y);
	console->Write(nameString);
	if (enteringName)
	{
		console->SetForegroundColor(CONSOLE_COLOR_LIGHTFG);
		console->Write(L"_");
	}
}

void PartitionFormatPage::RunPage()
{
	Console* console = GetConsole();
	bool holdingShift;

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		if (key->wVirtualKeyCode >= VK_NUMPAD0 && key->wVirtualKeyCode <= VK_NUMPAD9)
			key->wVirtualKeyCode -= 0x30;

		bool number = key->wVirtualKeyCode >= '0' && key->wVirtualKeyCode <= '9';
		bool letter = key->wVirtualKeyCode >= 'A' && key->wVirtualKeyCode <= 'Z';

		if (letter && !(key->dwControlKeyState & SHIFT_PRESSED))
			key->wVirtualKeyCode += 0x20;

		if (enteringName && (number || letter || key->wVirtualKeyCode == VK_SPACE))
		{
			int index = lstrlenW(nameString);
			if (index == 63)
				break;
			nameString[index] = key->wVirtualKeyCode;
			nameString[index + 1] = 0;

			Update();
		}
		else switch (key->wVirtualKeyCode) 
		{
		case VK_BACK:
		{
			if (!enteringName)
				break;
			int index = lstrlenW(nameString);
			// This flips enteringSize when the string is already empty
			// because it accesses array element -1 and the memory before
			// the array is the enteringSize variable.
			// This is fine as this is nice behaviour to have.
			nameString[index - 1] = 0;
			Update();
			break;
		}
		case VK_TAB:
			enteringName = !enteringName;
			Update();
			break;
		case VK_RETURN:
		{
			if (PartitionManager::ShowMessagePage(L"Warning: All data on the partition will be lost and a new file system will be created. Would you like to continue?", MessagePageType::YesNo, MessagePageUI::Warning) != MessagePageResult::Yes)
				break;

			SetStatusText(L"Formatting the partition...");
			Update();
			HRESULT hR = PartitionManager::FormatPartition(&PartitionManager::CurrentPartition, supportedFileSystems[selectionIndex + scrollIndex], nameString);
			if (hR != S_OK)
			{
				wchar_t buffer[MAX_PATH * 2];
				swprintf_s(buffer, L"Could not format the partition: 0x%08X", hR);
				PartitionManager::ShowMessagePage(buffer, MessagePageType::OK, MessagePageUI::Error);
			}
			PartitionManager::PopPage();
			return;
		}
		case VK_DOWN:
			if (enteringName)
				break;
			if (selectionIndex + 1 < min(maxItems, supportedFsCount))
				selectionIndex++;
			else if (selectionIndex + scrollIndex + 1 < supportedFsCount)
				scrollIndex++;
			Update();
			break;
		case VK_UP:
			if (enteringName)
				break;
			if (selectionIndex - 1 >= 0)
				selectionIndex--;
			else if (selectionIndex + scrollIndex - 1 >= 0)
				scrollIndex--;
			Update();
			break;
		case VK_ESCAPE:
			PartitionManager::PopPage();
			return;
		}
	}
}
