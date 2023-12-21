#include "PartitionTypeSelectionPage.h"

void PartitionTypeSelectionPage::InitPage()
{
	SetStatusText(L"");

	scrollIndex = 0;
	selectionIndex = 0;

	LoadItems(false);

	for (int i = 0; i < 4; i++) enteredChars[i] = L'0';
	enteredChars[4] = L'\0';

	inputIndex = 0;
	inputSelected = false;
}

void PartitionTypeSelectionPage::DrawPage()
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
	console->Write(L"Select the new type of the partition.");

	console->SetForegroundColor(CONSOLE_COLOR_FG);
	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To select a type use the UP and DOWN keys.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To display all partition types, press F9.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  Alternatively, enter the type code below.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To save the new type, press ENTER.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To go back, press ESC.");

	console->SetPosition(3, console->GetPosition().y + 2);
	console->Write(L"Type ID: ");
	textLocation = console->GetPosition();

	boxY = console->GetPosition().y + 3;
	int boxHeight = consoleSize.cy - boxY - 2;
	console->DrawBox(3, boxY, consoleSize.cx - 6, boxHeight, true);
	maxItems = boxHeight - 3;

	console->SetPosition(6, boxY + 1);
	console->Write(PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT ? 
		L"Type GUID                             Name                    Code" : 
		L"Type Code  Name");

	free(buffer);
}

void PartitionTypeSelectionPage::UpdatePage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();
	int bufferSize = consoleSize.cx + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);
	wchar_t guidBuffer[37];

	int selbufferSize = consoleSize.cx - 12 + 1;
	wchar_t* selbuffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);
	for (int i = 0; i < min(totalItems, maxItems); i++)
	{
		int j = i + scrollIndex;
		if (i == selectionIndex && !inputSelected)
		{
			console->SetBackgroundColor(CONSOLE_COLOR_FG);
			console->SetForegroundColor(CONSOLE_COLOR_BG);
		}
		else
		{
			console->SetBackgroundColor(CONSOLE_COLOR_BG);
			console->SetForegroundColor(CONSOLE_COLOR_FG);
		}

		console->SetPosition(6, boxY + 2 + i);
		swprintf(selbuffer, selbufferSize, L"%-*s", selbufferSize - 1, L"");
		console->Write(selbuffer);
		console->SetPosition(6, boxY + 2 + i);
		if (PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT)
		{
			PartitionManager::GetGuidStringFromStructure(items[j].guid, guidBuffer);
			swprintf(selbuffer, selbufferSize, L"%s  %-22.22s  0x%04hX", guidBuffer,
				items[j].display_name, items[j].gDiskType);
		}
		else
			swprintf(selbuffer, selbufferSize, L"0x%4hX  %s", items[j].gDiskType, items[j].display_name);

		console->Write(selbuffer);
	}

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	buffer[1] = L'\x00';
	console->SetPosition(textLocation.x, textLocation.y);
	for (int i = 0; i < 4; i++)
	{
		buffer[0] = i < inputIndex ? enteredChars[i] : L' ';
		console->Write(buffer);
	}

	console->SetPosition(0, textLocation.y + 1);
	for (int i = 0; i < bufferSize - 1; i++)
		buffer[i] = L' ';
	buffer[bufferSize - 1] = L'\0';
	console->Write(buffer);

	if (inputIndex >= 2)
	{
		short value;
		if (swscanf_s(enteredChars, L"%hX", &value))
		{
			console->SetPosition(3, textLocation.y + 1);
			console->Write(L"Detected type: ");
			console->Write(PartitionManager::GetStringFromPartitionTypeCode(value));
		}
	}

	console->SetPosition(textLocation.x + inputIndex, textLocation.y);
	console->SetCursor(inputSelected, inputSelected);

	free(buffer);
}

void PartitionTypeSelectionPage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case VK_DOWN:
			if (inputSelected)
				inputSelected = false;
			else if (selectionIndex + 1 < min(maxItems, totalItems))
				selectionIndex++;
			else if (selectionIndex + scrollIndex + 1 < totalItems)
				scrollIndex++;
			Update();
			break;
		case VK_UP:
			if (selectionIndex + scrollIndex == 0)
				inputSelected = true;
			else if (selectionIndex - 1 >= 0)
				selectionIndex--;
			else if (selectionIndex + scrollIndex - 1 >= 0)
				scrollIndex--;
			Update();
			break;
		case L'0': case L'1': case L'2': case L'3':
		case L'4': case L'5': case L'6': case L'7':
		case L'8': case L'9': case L'A': case L'B':
		case L'C': case L'D': case L'E': case L'F':
			if (inputSelected)
			{
				if (inputIndex < (PartitionManager::CurrentDiskOperatingMode == OperatingMode::MBR ? 2 : 4))
				{
					enteredChars[inputIndex++] = key->wVirtualKeyCode;
					Update();
				}
			}
			break;
		case VK_BACK:
			if (inputSelected)
			{
				if (inputIndex > 0)
				{
					enteredChars[--inputIndex] = L'0';
					Update();
				}
			}
			break;
		case VK_RETURN:
			{
			if (inputSelected)
			{
				if (inputIndex < 2)
					break;

				short value;
				if (!swscanf_s(enteredChars, L"%hX", &value))
					break;

				const wchar_t* string = PartitionManager::GetStringFromPartitionTypeCode(value);
				if (!wcsncmp(string, L"Unknown type", 12))
					break;

				PartitionManager::SetCurrentPartitionType(value);
			}
			else
			{
				PartitionManager::SetCurrentPartitionType(PartitionManager::GptTypes[selectionIndex + scrollIndex + 1].gDiskType);
			}
			}
		case VK_ESCAPE:
			PartitionManager::PopPage();
			return;
		case VK_F3:
			PartitionManager::Exit(0);
			return;
		case VK_F9:
			LoadItems(!allItemsShown);
			Update();
			break;
		}
	}
}

void PartitionTypeSelectionPage::LoadItems(bool showAll)
{
	GUID zero = { 0 };
	int i = 1;
	int j = 0;
	while (i < (showAll ? PartitionTypeCount : PartitionTypeCommonCount))
	{
		PartitionType type = PartitionManager::GptTypes[i++];
		if (PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT && !memcmp(&zero, &type.guid, sizeof(GUID)))
			continue;
		items[j++] = type;
	}

	totalItems = j;
	allItemsShown = showAll;
	selectionIndex = 0;
	scrollIndex = 0;
}