#include "PartitionCreationPage.h"
#include "..\CoreFunctions\PartitionManager.h"

/*

 WinParted 1.2.0m12
========================

   Partitioning Disk 0. (AMD-RAID Array)

   Creating new partition.

   Please select one of the free spaces of the disk below, and specify the
   size of the partition.

   ╔════════════════════════════════════════════════════════════════════════╗
   ║  Location                        Start        End          Available   ║
   ║ *At the start of the disk        2048                      500MB       ║
   ║  Before partition 1                           1026047      500MB       ║
   ║  After partition 1               2052096                   400GB       ║
   ║  At the end of the disk                       1232896000   400GB       ║
   ║                                                                        ║
   ║                                                                        ║
   ╚════════════════════════════════════════════════════════════════════════╝

   Size: 250_
   [S] [KB] [MB] [GB] [TB]

████████████████████████████████████████████████████████████████████████████████
*/

void PartitionCreationPage::InitPage()
{
	// TODO: Implement sector alignment

	SetStatusText(L"");
	sizeString[0] = L'\x0';

	// Convert partition table into spans
	SectorSpan* partitionSpans = (SectorSpan*)malloc(sizeof(SectorSpan) * PartitionManager::CurrentDiskPartitionCount);
	for (int i = 0; i < PartitionManager::CurrentDiskPartitionCount; i++)
	{
		partitionSpans[i].startSector = PartitionManager::CurrentDiskPartitions[i].StartLBA.ULL;
		partitionSpans[i].endSector = PartitionManager::CurrentDiskPartitions[i].EndLBA.ULL;
		partitionSpans[i].partitionA = PartitionManager::CurrentDiskPartitions[i].PartitionNumber;
	}

	// Sort the partitions (important)
	for (int i = 0; i < PartitionManager::CurrentDiskPartitionCount; i++)
	{
		unsigned long long key = partitionSpans[i].startSector;
		for (int j = i - 1; j >= 0 && partitionSpans[j].startSector > key; j--)
		{
			SectorSpan temp = partitionSpans[j + 1];
			partitionSpans[j + 1] = partitionSpans[j];
			partitionSpans[j] = temp;
		}
	}

	// U A U A .... A U
	// As seen a above, there's always one more unallocated (U) space than allocated 
	// spaces (A).
	unallocatedSpans = (SectorSpan*)malloc(sizeof(SectorSpan) * (PartitionManager::CurrentDiskPartitionCount + 1));
	ZeroMemory(unallocatedSpans, sizeof(SectorSpan) * (PartitionManager::CurrentDiskPartitionCount + 1));

	// Start with one span accross the entire disk
	unallocatedSpans[0].startSector = PartitionManager::CurrentDiskGPT.FirstUsableLBA.ULL;
	unallocatedSpans[0].endSector = PartitionManager::CurrentDiskGPT.LastUsableLBA.ULL;
	// IF and only if the partitions are in order, we can iterate over the partitions and
	// splice the span into two parts using the partitions LBA's. The partitions need to
	// be sorted so that they will always be within the last unallocated space.
	// Let's say we have unallocated space (a b) and partition (c d) lies in between it.
	// The unallocated space can be split into (a c) and (d b).
	int unallocIndex = 0;
	for (int i = 0; i < PartitionManager::CurrentDiskPartitionCount; i++)
	{
		// Start aligns: only move start pointer of unallocated space
		if (unallocatedSpans[unallocIndex].startSector == partitionSpans[i].startSector)
		{
			unallocatedSpans[unallocIndex].startSector = partitionSpans[i].endSector + 1;
			unallocatedSpans[unallocIndex].partitionA = partitionSpans[i].partitionA;
		}

		// Ends align: only move end pointer of unallocated space
		else if (unallocatedSpans[unallocIndex].endSector == partitionSpans[i].endSector)
		{
			unallocatedSpans[unallocIndex].endSector = partitionSpans[i].startSector - 1;
			unallocatedSpans[unallocIndex].partitionB = partitionSpans[i].partitionA;
		}
		
		// If both align, the length of the span will become 0 automatically (end = start - 1)
		//  - Note that situation is actually impossible.
		
		// No alignment: split the span
		else 
		{
			unallocatedSpans[unallocIndex + 1].startSector = partitionSpans[i].endSector + 1;
			unallocatedSpans[unallocIndex + 1].endSector = unallocatedSpans[unallocIndex].endSector;
			unallocatedSpans[unallocIndex + 1].partitionA = partitionSpans[i].partitionA;
			unallocatedSpans[unallocIndex].endSector = partitionSpans[i].startSector - 1;
			unallocatedSpans[unallocIndex].partitionB = partitionSpans[i].partitionA;
			unallocIndex++;
		}
	}
	unallocatedSpanCount = unallocIndex + 1;
	free(partitionSpans);

	// Space between partitions
	// - Align span to nearest 1 MB boundaries
	// - Add 1 MB to start and remove 1 MB to end
	//   (1 MB seems arbitrary, but it is the value Windows uses)
	// - If start > end remove the space
	SectorSpan* newUnallocatedSpans = (SectorSpan*)malloc(sizeof(SectorSpan) * (PartitionManager::CurrentDiskPartitionCount + 1));
	ZeroMemory(newUnallocatedSpans, sizeof(SectorSpan) * (PartitionManager::CurrentDiskPartitionCount + 1));
	int newIndex = 0;
	const unsigned long long megabyte = 1024 * 1024 / PartitionManager::CurrentDisk.SectorSize;
	for (int i = 0; i < unallocatedSpanCount; i++) 
	{
		unsigned long long  remainder = unallocatedSpans[i].startSector % (megabyte);
		if (remainder == 0)
			unallocatedSpans[i].startSector += megabyte - remainder;
		unallocatedSpans[i].startSector += megabyte;

		remainder = unallocatedSpans[i].endSector % (megabyte);
		if (remainder == 0)
			unallocatedSpans[i].endSector -= remainder;
		unallocatedSpans[i].endSector -= megabyte;

		if ((long long)(unallocatedSpans[i].endSector - unallocatedSpans[i].startSector + 1) < 0)
			continue;

		newUnallocatedSpans[newIndex++] = unallocatedSpans[i];
	}
	unallocatedSpanCount = newIndex + 1;
	free(unallocatedSpans);
	unallocatedSpans = newUnallocatedSpans;

	// If there is no space on the disk (0 unallocated spans), display message box and pop
	if (unallocatedSpanCount == 0)
	{
		PartitionManager::ShowMessagePage(L"Cannot create a new partition because there is no space available on the disk.", MessagePageType::OK, MessagePageUI::Error);
		PartitionManager::PopPage();
		return;
	}
}

void PartitionCreationPage::DrawPage()
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
	console->Write(L"Creating a new partition.");

	console->DrawTextLeft(L"Please select one of the free spaces of the disk below, and specify the size of the partition.", consoleSize.cx - 6, console->GetPosition().y + 2);

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To select the unallocated space, use the UP and DOWN keys.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To switch to entering the size, press the TAB key.");
	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To create the partition, press the ETNER key.");

	drawY = console->GetPosition().y + 2;
}

void PartitionCreationPage::UpdatePage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();

	unsigned long long bufferSize = consoleSize.cx + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	int boxHeight = consoleSize.cy - drawY - 5;
	console->DrawBox(3, drawY, consoleSize.cx - 6, boxHeight, true);

	console->SetPosition(6, drawY + 1);
	swprintf_s(buffer, bufferSize, L"Location%*sStart        End          Available", consoleSize.cx - 12 - 44, L"");
	console->Write(buffer);

	maxItems = boxHeight - 3;
	for (int i = 0; i < maxItems; i++)
	{
		if (i == selectionIndex && !enteringSize)
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
		if (j / 2 >= unallocatedSpanCount) break;

		if (j % 2)
			if (unallocatedSpans[j / 2].partitionB == 0)
				swprintf_s(buffer, bufferSize, L"At the end of the disk");
			else
				swprintf_s(buffer, bufferSize, L"Before partition %d", unallocatedSpans[j / 2].partitionB);
		else
			if (unallocatedSpans[j / 2].partitionA == 0)
				swprintf_s(buffer, bufferSize, L"At the start of the disk");
			else
				swprintf_s(buffer, bufferSize, L"After partition %d", unallocatedSpans[j / 2].partitionA);

		int length = lstrlenW(buffer);
		// TODO: Make the zero not show up because it is confusing
		swprintf_s(buffer + length, bufferSize - length, L"%*s%-11.0llu  %-11.0llu  %-9.0d", consoleSize.cx - 12 - length - 36,
			L"", j % 2 ? 0 : unallocatedSpans[j / 2].startSector, j % 2 ? unallocatedSpans[j / 2].endSector : 0,
			unallocatedSpans[j / 2].GetSize());

		console->SetPosition(6, drawY + 2 + i);
		console->Write(buffer);
	}

	console->SetBackgroundColor(CONSOLE_COLOR_BG);
	console->SetForegroundColor(CONSOLE_COLOR_FG);

	console->DrawTextLeft(L"Size: ", consoleSize.cx - 6, drawY + boxHeight + 1);
	POINT p = console->GetPosition();
	for (int i = 0; i < 21; i++)
		console->Write(L" ");
	console->SetPosition(p.x, p.y);
	console->Write(sizeString);
	if (enteringSize)
	{
		console->SetForegroundColor(CONSOLE_COLOR_LIGHTFG);
		console->Write(L"_");
		console->SetForegroundColor(CONSOLE_COLOR_FG);
	}
	console->SetPosition(3, drawY + boxHeight + 2);
	swprintf_s(buffer, bufferSize, L"Final size for partition: %-25llu", size);
	console->Write(buffer);
}

void PartitionCreationPage::RunPage()
{
	Console* console = GetConsole();
	char keyChar = 0;

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case VK_NUMPAD0:
		case VK_NUMPAD1:
		case VK_NUMPAD2:
		case VK_NUMPAD3:
		case VK_NUMPAD4:
		case VK_NUMPAD5:
		case VK_NUMPAD6:
		case VK_NUMPAD7:
		case VK_NUMPAD8:
		case VK_NUMPAD9:
			keyChar = key->wVirtualKeyCode - 0x30;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'K':
		case 'M':
		case 'G':
		case 'T':
		{
			if (!enteringSize)
				break;
			wchar_t lastChar = sizeString[max(lstrlenW(sizeString) - 1, 0)];
			if (lastChar && (lastChar < L'0' || lastChar > L'9'))
				break;

			int index = lstrlenW(sizeString);
			sizeString[index] = keyChar ? keyChar : key->wVirtualKeyCode;
			sizeString[index + 1] = 0;

			lastChar = sizeString[max(lstrlenW(sizeString) - 1, 0)];
			size = wcstoull(sizeString, NULL, 10);
			if (lastChar < L'0' || lastChar > L'9')
			{
				const wchar_t* chars = L" KMGT";
				int index = wcschr(chars, lastChar) - chars;
				for (int i = 0; i < index; i++)
					size *= 1024;
			}
			size /= PartitionManager::CurrentDisk.SectorSize;

			Update();
			break;
		}
		case VK_BACK:
		{
			if (!enteringSize)
				break;
			int index = lstrlenW(sizeString);
			// This flips enteringSize when the string is already empty
			// because it accesses array element -1 and the memory before
			// the arrray is the enteringSize variable.
			// This is fine as this is nice behaviour to have.
			sizeString[index - 1] = 0;
			Update();
			break;
		}
		case VK_RETURN:
			// TODO: Create the partition
			break;
		case VK_DOWN:
			if (enteringSize)
				break;
			if (selectionIndex + 1 < min(maxItems, unallocatedSpanCount * 2))
				selectionIndex++;
			else if (selectionIndex + scrollIndex + 1 < unallocatedSpanCount * 2)
				scrollIndex++;
			Update();
			break;
		case VK_UP:
			if (enteringSize)
				break;
			if (selectionIndex - 1 >= 0)
				selectionIndex--;
			else if (selectionIndex + scrollIndex - 1 >= 0)
				scrollIndex--;
			Update();
			break;
		case VK_TAB:
			enteringSize = !enteringSize;
			Update();
			break;
		case VK_ESCAPE:
			PartitionManager::PopPage();
			return;
		}
	}
}
