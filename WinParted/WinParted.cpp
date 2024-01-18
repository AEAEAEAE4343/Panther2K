// WinParted.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "CoreFunctions\PartitionManager.h"

int main()
{
    //PrintVdsData();
    PartitionManager::RunWinParted();

    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file






/*

Hiërachy of WinParted:


Welcome/Disk selection (DiskSelectionPage)
 │
 └──Disk partitioning
     │
     ├──Partition creation
     │
     ├──Partition deletion
     │
     ├──Partition table creation
     │
     └──Partition operations
         │
         ├──Volume creation
         │
         ├──Partition type selection
         │
         ├──Partition attribute selection
         │
         ├──Partition GUID selection
         │
         ├──Partition name selection
         │
         └──Volume name selection


----------------------------------------------------------------------------------------------------------------
Welcome Screen
----------------------------------------------------------------------------------------------------------------

 WinParted 
===========

   Welcome to WinParted.

   To begin partitioning, you need to select a disk first.

      •  To select a disk, use the UP and DOWN keys

      •  To start partitioning the selected disk, press ENTER

      •  To quit WinParted (and return to Panther2K) press F3

   ╔════════════════════════════════════════════════════════════════════════╗
   ║ Disk  Device Name             Part count  Type       Sectorsize/count  ║
   ║    0  AMD-RAID Array 1                 3  Standard   512 / 466575360   ║
   ║    1  WDC WD10EADS-11M                 1  Standard   512 / 1953520065  ║
   ║    2  ST1000DM010-2EP1                 1  Standard   512 / 1953520065  ║
   ║    5  USB  SanDisk 3.2Gen1             2  Removable  512 / 60083100    ║      
   ║                                                                        ║
   ╚════════════════════════════════════════════════════════════════════════╝


████████████████████████████████████████████████████████████████████████████████
----------------------------------------------------------------------------------------------------------------
Disk operation screen (After selecting disk)
----------------------------------------------------------------------------------------------------------------

 WinParted
===========

   Partitioning Disk 0 (AMD-RAID Array 1)
   The disk uses GPT partitioning (protective MBR).

      •  To select a partition, use the UP and DOWN keys
      •  To modify a partition, press ENTER

      •  To create a new partition, press N
      •  To delete a partition, press D
      •  To create an empty (new) partition table, press E
      •  To write the currently modified partition table, press W

   ╔════════════════════════════════════════════════════════════════════════╗
   ║ Part #  Partition type          Start        End          Size         ║
   ║      1  EFI system partition    2048         1026047      1024000      ║
   ║      2  Microsoft basic data    1026048      368267230    367241181    ║
   ║      3  Linux filesystem        368269312    466575326    98306015     ║
   ║                                                                        ║
   ║                                                                        ║
   ╚════════════════════════════════════════════════════════════════════════╝
   Sector size=512  Alignment=2048

██ESC=Back██F3=Quit█████████████████████████████████████████████████████████████
----------------------------------------------------------------------------------------------------------------
Partition operation sceen (After selecting partition)
----------------------------------------------------------------------------------------------------------------

 WinParted
===========

   Partitioning Disk 0 (AMD-RAID Array 1)
   
   Modifying Partition 1

   Partition type: EFI System Partition
   Partition size: 500MiB (524288000 bytes)
   Partition name: EFI System Partition
   Partition GUID: 8B83AABE-28FC-4B1F-9D23-5A101CB17C0A
   Attribute flags: 0000000000000000
   Volume name: EFI         | Cannot determine or modify volume information:
   Volume filesystem: FAT32 | The partition table was modified, but not saved.

      •  To change the partition type, press T
      •  To change the partition GUID, press G
      •  To change the attributes flags, press A
      •  To change the name of the partition or volume, press N
      •  To format this partition, press F
      •  To go back to the disk partitioning menu, press ESC
   

████████████████████████████████████████████████████████████████████████████████
-----
New partition screen
-----
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
   
   Size: 250M (512000 sectors)                                                                 
   0B [████████████████████████████████                                ] 500M

████████████████████████████████████████████████████████████████████████████████
*/