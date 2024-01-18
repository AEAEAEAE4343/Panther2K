#include "DiskPartitioningPage.h"
#include "CoreFunctions\PartitionManager.h"
#include "Partition\PartitionInformationPage.h"
#include "Partition\PartitionCreationPage.h"
#include "Partition\PartitionTypeSelectionPage.h"

void DiskPartitioningPage::InitPage()
{
	if (PartitionManager::CurrentDiskPartitions == NULL)
		PartitionManager::LoadPartitionTable();

	SetStatusText(L"");

	scrollIndex = 0;
	selectionIndex = 0;

	return;
}

void DiskPartitioningPage::DrawPage()
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
	console->SetPosition(3, 5);
	swprintf(buffer, bufferSize, L"The disk uses %s partitioning.%s", PartitionManager::GetOperatingModeString(), PartitionManager::GetOperatingModeExtraString());
	console->Write(buffer);

	console->SetPosition(6, 7);
	console->Write(L"\x2022  To select a partition, use the UP and DOWN keys");
	console->SetPosition(6, 8);
	console->Write(L"\x2022  To modify a partition, press ENTER");

	console->SetPosition(6, 10);
	console->Write(L"\x2022  To create a new partition, press N");
	console->SetPosition(6, 11);
	console->Write(L"\x2022  To delete a partition, press D");
	console->SetPosition(6, 12);
	console->Write(L"\x2022  To create an empty (new) partition table, press E");
	

	console->DrawBox(3, 15, consoleSize.cx - 6, consoleSize.cy - 17, true);
	maxItems = consoleSize.cy - 17 - 3;

	console->SetPosition(4, 16);
	console->WriteLine(L"  Part #  Partition type        Start        End          Size");
}

void DiskPartitioningPage::UpdatePage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();

	int bufferSize = consoleSize.cx - 13 + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);
	wchar_t sizeBuffer[10];
	for (int i = 0; i < min(PartitionManager::CurrentDiskPartitionCount, maxItems); i++)
	{
		int j = i + scrollIndex;
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

		
		swprintf(buffer, bufferSize, L"%-*s", bufferSize - 1, L"");
		console->SetPosition(6, 17 + i);
		console->Write(buffer);

		PartitionInformation* partitionInfo = PartitionManager::CurrentDiskPartitions + j;
		PartitionManager::GetSizeStringFromBytes(partitionInfo->SectorCount * PartitionManager::CurrentDisk.SectorSize, sizeBuffer);
		swprintf(buffer, bufferSize, L"%6d  %-20.20s  %-11llu  %-11llu  %s", 
			partitionInfo->PartitionNumber, 
			PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT ?
				PartitionManager::GetStringFromPartitionTypeGUID(partitionInfo->Type.TypeGUID) :
				PartitionManager::GetStringFromPartitionSystemID(partitionInfo->Type.SystemID),
			partitionInfo->StartLBA.ULL, 
			partitionInfo->EndLBA.ULL, 
			sizeBuffer);
		console->SetPosition(6, 17 + i);
		console->Write(buffer);
	}

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	if (PartitionManager::CurrentDiskPartitionsModified)
	{
		console->SetPosition(6, 13);
		console->Write(L"\x2022  To write the currently modified partition table, press F10");
	}

	int boxY = 15;
	int boxHeight = consoleSize.cy - 17;

	bool canScrollDown = (scrollIndex + maxItems) < PartitionManager::CurrentDiskPartitionCount;
	bool canScrollUp = scrollIndex != 0;

	console->SetPosition(consoleSize.cx - 6, boxY + boxHeight - 2);
	if (canScrollDown) console->Write(L"\x2193");
	else console->Write(L" ");

	console->SetPosition(consoleSize.cx - 6, boxY + 2);
	if (canScrollUp) console->Write(L"\x2191");
	else console->Write(L" ");
}

void DiskPartitioningPage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case 'T':
			if (PartitionManager::LoadPartition(PartitionManager::CurrentDiskPartitions + (selectionIndex + scrollIndex)))
			{
				PartitionTypeSelectionPage* page = new PartitionTypeSelectionPage();
				PartitionManager::PushPage(page);
				return;
			}
			break;
		case 'D':
			if (PartitionManager::ShowMessagePage(L"Are you sure you would like to remove this partition?", MessagePageType::YesNo) == MessagePageResult::Yes)
			{
				PartitionManager::DeletePartition(PartitionManager::CurrentDiskPartitions + (selectionIndex + scrollIndex));
				selectionIndex = 0;
				scrollIndex = 0;
				Draw();
			}
			break;
		case 'N':
			PartitionManager::PushPage(new PartitionCreationPage());
			return;
		case VK_DOWN:
			if (selectionIndex + 1 < min(maxItems, PartitionManager::CurrentDiskPartitionCount))
				selectionIndex++;
			else if (selectionIndex + scrollIndex + 1 < PartitionManager::CurrentDiskPartitionCount)
				scrollIndex++;
			Update();
			break;
		case VK_UP:
			if (selectionIndex - 1 >= 0)
				selectionIndex--;
			else if (selectionIndex + scrollIndex - 1 >= 0)
				scrollIndex--;
			Update();
			break;
		case VK_RETURN:
			if (PartitionManager::LoadPartition(PartitionManager::CurrentDiskPartitions + (selectionIndex + scrollIndex)))
			{
				PartitionInformationPage* page = new PartitionInformationPage();
				PartitionManager::PushPage(page);
				return;
			}
			break;
		case VK_ESCAPE:
			if (PartitionManager::CurrentDiskPartitionsModified)
			{
				switch (PartitionManager::ShowMessagePage(L"The partition table was modified, but it was not saved to disk. Would you like to save the modified partition table before returning to the disk selection page?", MessagePageType::YesNoCancel))
				{
				case MessagePageResult::Yes:
					// Save and return
					PartitionManager::SavePartitionTableToDisk();
					PartitionManager::PopPage();
					return;
				case MessagePageResult::No:
					// Discard and return
					PartitionManager::CurrentDiskPartitionsModified = false;
					PartitionManager::PopPage();
					return;
				case MessagePageResult::Cancel:
					// Just break
					break;
				}
			}
			else 
			{
				PartitionManager::PopPage();
				return;
			}
			break;
		case VK_F3:
			PartitionManager::Exit(0);
			return;
		case VK_F10:
			if (PartitionManager::CurrentDiskPartitionsModified)
				PartitionManager::SavePartitionTableToDisk();
			break;
		}
	}
}
