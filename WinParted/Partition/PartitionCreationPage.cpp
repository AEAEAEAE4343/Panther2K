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
	return;
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
		swprintf_s(buffer + length, bufferSize - length, L"%*s%-11llu  %-11llu  %-9d", consoleSize.cx - 12 - length - 36,
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
	console->Write(L"Final size for partition: ");
}

void PartitionCreationPage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
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
		{
			wchar_t lastChar = sizeString[lstrlenW(sizeString) - 2];
			if (lastChar < L'0' || lastChar > L'9')
				break;
			// TODO: Size input
			break;
		}
		case 'M':
		case 'G':
		case 'T':
		case VK_BACK:
			// TODO: Size input
			break;
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
