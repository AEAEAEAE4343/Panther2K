#include "DiskSelectionPage.h"
#include "PartitionManager.h"
#include "DiskPartitioningPage.h"

void DiskSelectionPage::InitPage()
{
	if (PartitionManager::DiskInformationTable == NULL)
		PartitionManager::PopulateDiskInformation();

	SetStatusText(L"");

	scrollIndex = 0;
	selectionIndex = 0;

	return;
}

void DiskSelectionPage::DrawPage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_LIGHTFG);
	console->SetPosition(3, 4);
	console->Write(L"Welcome to WinParted.");

	console->SetForegroundColor(CONSOLE_COLOR_FG);
	console->SetPosition(3, console->GetPosition().y + 2);
	console->Write(L"To begin partitioning, you need to select a disk first.");

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To select a disk, use the UP and DOWN keys");

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To start partitioning the selected disk, press ENTER");

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To quit WinParted (and return to Panther2K) press F3");

	console->SetPosition(3, console->GetPosition().y + 2);
	
	boxY = console->GetPosition().y;
	int consoleHeight = consoleSize.cy - boxY - 2;
	console->DrawBox(3, console->GetPosition().y, consoleSize.cx - 6, consoleHeight, true);
	maxItems = consoleHeight - 3;

	console->SetPosition(4, boxY + 1);
	console->WriteLine(L"  Disk  Device Name             Part count  Type       Sectorsize/count");
	
}

void DiskSelectionPage::UpdatePage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();

	int bufferSize = consoleSize.cx - 12;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);
	for (int i = 0; i < min(PartitionManager::DiskInformationTableSize, maxItems); i++)
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

		DISK_INFORMATION* diskInfo = PartitionManager::DiskInformationTable + j;
		console->SetPosition(6, boxY + 2 + i);
		swprintf(buffer, bufferSize, L"%-*s", bufferSize - 1, L"");
		console->Write(buffer);
		console->SetPosition(6, boxY + 2 + i);
		swprintf(buffer, bufferSize, L"%4d  %-*.*s  %10d  %-9s  %d/%lld", diskInfo->DiskNumber, bufferSize - 46, bufferSize - 46, diskInfo->DeviceName, diskInfo->PartitionCount, L"Standard", diskInfo->SectorSize, diskInfo->SectorCount);
		console->Write(buffer);
	}

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	int boxY = 14;
	int boxHeight = consoleSize.cy - 17;

	bool canScrollDown = (scrollIndex + maxItems) < PartitionManager::DiskInformationTableSize;
	bool canScrollUp = scrollIndex != 0;

	console->SetPosition(consoleSize.cx - 6, boxY + boxHeight - 2);
	if (canScrollDown) console->Write(L"\x2193");
	else console->Write(L" ");

	console->SetPosition(consoleSize.cx - 6, boxY + 2);
	if (canScrollUp) console->Write(L"\x2191");
	else console->Write(L" ");
}

void DiskSelectionPage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case VK_DOWN:
			if (selectionIndex + 1 < min(maxItems, PartitionManager::DiskInformationTableSize))
				selectionIndex++;
			else if (selectionIndex + scrollIndex + 1 < PartitionManager::DiskInformationTableSize)
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
			if (PartitionManager::LoadDisk(PartitionManager::DiskInformationTable + (scrollIndex + selectionIndex)))
			{
				DiskPartitioningPage* page = new DiskPartitioningPage();
				PartitionManager::PushPage(page);
				return;
			}
			break;
		case VK_F3:
			PartitionManager::Exit(0);
			return;
		}
	}
}
