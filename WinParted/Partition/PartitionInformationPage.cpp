#include "PartitionInformationPage.h"
#include "PartitionTypeSelectionPage.h"
#include "PartitionGuidSelectionPage.h"
#include "PartitionFormatPage.h"
#include "..\CoreFunctions\PartitionManager.h"

void PartitionInformationPage::InitPage()
{
	SetStatusText(L"");
}

void PartitionInformationPage::DrawPage()
{
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();
	int bufferSize = consoleSize.cx + 1;
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);

	wchar_t sizeBuffer[10];
	wchar_t guidBuffer[37];

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

	console->SetPosition(3, console->GetPosition().y + 2);
	swprintf(buffer, bufferSize, L"Partition type: %s", 
		PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT ?
		PartitionManager::GetStringFromPartitionTypeGUID(PartitionManager::CurrentPartition.Type.TypeGUID) :
		PartitionManager::GetStringFromPartitionSystemID(PartitionManager::CurrentPartition.Type.SystemID));
	console->Write(buffer);

	unsigned long long byteCount = PartitionManager::CurrentPartition.SectorCount * PartitionManager::CurrentDisk.SectorSize;
	PartitionManager::GetSizeStringFromBytes(byteCount, sizeBuffer);
	console->SetPosition(3, console->GetPosition().y + 1);
	swprintf(buffer, bufferSize, L"Partition size: %s (%llu bytes)", sizeBuffer, byteCount);
	console->Write(buffer);

	if (PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT)
	{
		GPT_ENTRY* CurrentPartitionGPT = reinterpret_cast<GPT_ENTRY*>(
			reinterpret_cast<unsigned char*>(PartitionManager::CurrentDiskGPTTable) + 
				(PartitionManager::CurrentDiskGPT.TableEntrySize * 
					(PartitionManager::CurrentPartition.PartitionNumber - 1)));
		PartitionManager::GetGuidStringFromStructure(CurrentPartitionGPT->UniqueGUID, guidBuffer);
		console->SetPosition(3, console->GetPosition().y + 1);
		swprintf(buffer, bufferSize, L"Partition GUID: %s", guidBuffer);
		console->Write(buffer);

		console->SetPosition(3, console->GetPosition().y + 1);
		swprintf(buffer, bufferSize, L"Partition attribute flags: %016llx", CurrentPartitionGPT->AttributeFlags);
		console->Write(buffer);
	}

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(L"\x2022  To change the partition type, press T");
	if (PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT)
	{
		console->SetPosition(6, console->GetPosition().y + 1);
		console->Write(L"\x2022  To change the partition GUID, press G");
		console->SetPosition(6, console->GetPosition().y + 1);
		console->Write(L"\x2022  To change the attributes flags, press A");
	}
	
	console->SetPosition(3, console->GetPosition().y + 1);

	if (!PartitionManager::CurrentDiskPartitionsModified)
	{
		if (PartitionManager::CurrentPartition.VolumeLoaded)
		{
			console->SetPosition(3, console->GetPosition().y + 1);
			if (lstrlenW(PartitionManager::CurrentPartition.VolumeInformation.VolumeName))
			{
				swprintf(buffer, bufferSize, L"Volume name: %s", PartitionManager::CurrentPartition.VolumeInformation.VolumeName);
				console->Write(buffer);
			}
			else console->Write(L"The volume has no name.");

			console->SetPosition(3, console->GetPosition().y + 1);
			swprintf(buffer, bufferSize, L"Volume filesystem: %s", PartitionManager::CurrentPartition.VolumeInformation.FileSystem);
			console->Write(buffer);

			console->SetPosition(6, console->GetPosition().y + 2);
			console->Write(L"\x2022  To change the name of the partition or volume, press N");
		}
		else
		{
			console->SetPosition(3, console->GetPosition().y + 1);
			console->Write(L"Cannot determine volume information:"); 
			console->SetPosition(3, console->GetPosition().y + 1);
			console->Write(L"The volume does not exist or cannot be read.");
			console->SetPosition(3, console->GetPosition().y + 1);
		}

		console->SetPosition(6, console->GetPosition().y + 1);
		console->Write(L"\x2022  To format this partition, press F");
	}
	else
	{
		console->SetPosition(3, console->GetPosition().y + 1);
		console->Write(L"Cannot determine or modify volume information:");
		console->SetPosition(3, console->GetPosition().y + 1);
		console->Write(L"The partition table was modified, but not saved.");
		console->SetPosition(3, console->GetPosition().y + 1);
	}

	console->SetPosition(6, console->GetPosition().y + 1);
	console->Write(L"\x2022  To go back to the disk partitioning menu, press ESC");
}

void PartitionInformationPage::UpdatePage()
{
	
}

void PartitionInformationPage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case 'T':
		{
			PartitionTypeSelectionPage* page = new PartitionTypeSelectionPage();
			PartitionManager::PushPage(page);
			return;
		}
		case 'G':
			if (PartitionManager::CurrentDiskOperatingMode == OperatingMode::GPT)
			{
				PartitionGuidSelectionPage* page = new PartitionGuidSelectionPage();
				PartitionManager::PushPage(page);
				return;
			}
			break;
		case 'F':
			if (PartitionManager::CurrentDiskPartitionsModified)
			{
				PartitionManager::ShowMessagePage(L"The partition can not be formatted because the partition table has not been saved. Please save the partition table before formatting the partition.", MessagePageType::OK, MessagePageUI::Error);
			}
			else 
			{
				PartitionManager::PushPage(new PartitionFormatPage());
				return;
			}
			break;
		case VK_RETURN:
			//DiskPartitioningPage* page = new DiskPartitioningPage();
			//PartitionManager::SetNextPage(page);
			break;
		case VK_ESCAPE:
			PartitionManager::PopPage();
			return;
		case VK_F3:
			PartitionManager::Exit(0);
			return;
		}
	}
}
