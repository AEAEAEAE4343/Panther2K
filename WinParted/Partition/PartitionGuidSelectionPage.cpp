#include "PartitionGuidSelectionPage.h"
#include "..\CoreFunctions\PartitionManager.h"

void PartitionGuidSelectionPage::InitPage()
{
	// This is what a GUID looks like.
	// 12345678-1234-1234-1234-123456789abc

	SetStatusText(L"");

	/*for (int i = 0; i < 36; i++) enteredChars[i] = L' ';
	enteredChars[8] = L'-';
	enteredChars[12] = L'-';
	enteredChars[16] = L'-';
	enteredChars[20] = L'-';
	enteredChars[36] = L'\0';*/

	GPT_ENTRY* CurrentPartitionGPT = reinterpret_cast<GPT_ENTRY*>(
		reinterpret_cast<unsigned char*>(PartitionManager::CurrentDiskGPTTable) +
		(PartitionManager::CurrentDiskGPT.TableEntrySize *
			(PartitionManager::CurrentPartition.PartitionNumber - 1)));
	PartitionManager::GetGuidStringFromStructure(CurrentPartitionGPT->UniqueGUID, enteredChars);

	inputIndex = 36;
}

void PartitionGuidSelectionPage::DrawPage()
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
	console->Write(L"Enter the new unique GUID of the partition.");

	console->SetForegroundColor(CONSOLE_COLOR_FG);
	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To set the new GUID, enter it below.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To save the new GUID, press ENTER.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To go back to the partition information screen, press ESC");

	console->SetPosition(3, console->GetPosition().y + 2);
	console->Write(L"Type ID: ");
	textLocation = console->GetPosition();

	free(buffer);
}

void PartitionGuidSelectionPage::UpdatePage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();
	int bufferSize = consoleSize.cx + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);
	wchar_t guidBuffer[37];

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	buffer[1] = L'\x00';
	console->SetPosition(textLocation.x, textLocation.y);
	for (int i = 0; i < 36; i++)
	{
		buffer[0] = i < inputIndex ? enteredChars[i] : L' ';
		console->Write(buffer);
	}

	console->SetPosition(0, textLocation.y + 1);
	for (int i = 0; i < bufferSize - 1; i++)
		buffer[i] = L' ';
	buffer[bufferSize - 1] = L'\0';
	console->Write(buffer);

	console->SetPosition(textLocation.x + inputIndex, textLocation.y);
	console->SetCursor(true, true);

	free(buffer);
}

void PartitionGuidSelectionPage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case L'0': case L'1': case L'2': case L'3':
		case L'4': case L'5': case L'6': case L'7': 
		case L'8': case L'9': case L'A': case L'B': 
		case L'C': case L'D': case L'E': case L'F':
			if (inputIndex < 36)
			{
				enteredChars[inputIndex++] = key->wVirtualKeyCode;
				if (inputIndex == 8 || inputIndex == 13 ||
					inputIndex == 18 || inputIndex == 23)
					inputIndex++;
				Update();
			}
			break;
		case VK_BACK:
			if (inputIndex > 0)
			{
				if (inputIndex == 9 || inputIndex == 14 ||
					inputIndex == 19 || inputIndex == 24)
					inputIndex--;
				enteredChars[--inputIndex] = L'0';
				Update();
			}
			break;
		case VK_RETURN:
		{
			if (inputIndex < 36)
				break;

			GUID newGuid;
			PartitionManager::GetGuidStructureFromString(&newGuid, enteredChars);
			PartitionManager::SetCurrentPartitionGuid(newGuid);
		}
		case VK_ESCAPE:
			PartitionManager::PopPage();
			return;
		case VK_F3:
			PartitionManager::Exit(0);
			return;
		}
	}
}