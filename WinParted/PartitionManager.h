#pragma once

#include "PartitionTable.h"
#include <PantherConsole.h>
#include "Page.h"
#include "MessagePage.h"
#include <stack>

enum class PartitionTableType
{
	Unknown = 0,
	MBR = 1,
	GPT = 2,
	PMBR = 4,
	GPT_PMBR = GPT | PMBR,
	GPT_HMBR = GPT | MBR,
};

enum class OperatingMode
{
	Unknown,
	MBR,
	GPT,
};

struct DISK_INFORMATION
{
	unsigned int DiskNumber;
	wchar_t DiskPath[64];
	wchar_t DeviceName[256];
	unsigned int PartitionCount;
	MEDIA_TYPE MediaType;
	unsigned int SectorSize;
	unsigned long long SectorCount;
};

struct PartitionType
{
	short gDiskType;
	GUID guid;
	const wchar_t* display_name;
};

struct WP_PART_DESCRIPTION
{
	int PartitionNumber;
	short PartitionType;
	wchar_t PartitionSize[10];
	wchar_t FileSystem[10];
	const wchar_t* MountPoint;
};

struct WP_PART_LAYOUT
{
	int PartitionCount;
	WP_PART_DESCRIPTION Partitions[1];
};

#define PartitionTypeCount 224
#define PartitionTypeCommonCount 16

void PrintVdsData();

class PartitionManager
{
public:
	static const PartitionType GptTypes[PartitionTypeCount];
	
	//
	// UI management
	//

	static void SetConsole(Console* console);
	static int RunWinParted();
    static int RunWinParted(Console* console);
	static void PushPage(Page* page);
	static void PopPage();
	static MessagePageResult ShowMessagePage(const wchar_t* message, MessagePageType type = MessagePageType::OK, MessagePageUI uiStyle = MessagePageUI::Normal);
	static void Exit(int returnCode);
	static void Restart();

	//
	// Disk manipulation
	//

	static void PopulateDiskInformation();
	static PartitionTableType GetPartitionTableType(DISK_INFORMATION* diskInfo);
	static bool LoadDisk(DISK_INFORMATION* diskInfo);

	//
	// Partition table manipulation
	//

	static bool LoadPartitionTable();
	static bool SavePartitionTableToDisk();
	static bool DeletePartition(PartitionInformation* partInfo);
	static HRESULT ApplyPartitionLayoutGPT(WP_PART_LAYOUT* layout);
	static HRESULT ApplyPartitionLayoutMBR(WP_PART_LAYOUT* layout);

	static DISK_INFORMATION* DiskInformationTable;
	static long DiskInformationTableSize;
	static DISK_INFORMATION CurrentDisk;
	static PartitionTableType CurrentDiskType;
	static MBR_HEADER CurrentDiskMBR;
	static GPT_HEADER CurrentDiskGPT;
	static GPT_ENTRY* CurrentDiskGPTTable;
	static OperatingMode CurrentDiskOperatingMode;
	static PartitionInformation* CurrentDiskPartitions;
	static long CurrentDiskPartitionCount;
	static bool CurrentDiskPartitionsModified;
	static bool CurrentDiskPartitionTableDestroyed;
	static long CurrentDiskFirstAvailablePartition;

	//
	// Partition manipulation
	//

	static bool LoadPartition(PartitionInformation* partition);
	static bool SetCurrentPartitionType(short value);
	static bool SetCurrentPartitionGuid(GUID value);
	static HRESULT FormatPartition(PartitionInformation* partition, const wchar_t* fileSystem, const wchar_t* mountPoint);

	static PartitionInformation CurrentPartition;
	
	//
	// Miscellaneous conversion stuff
	//

	static const wchar_t* GetOperatingModeString();
	static const wchar_t* GetOperatingModeExtraString();
	static void GetSizeStringFromBytes(unsigned long long bytes, wchar_t buffer[10]);
	static void GetGuidStringFromStructure(GUID bytes, wchar_t buffer[37]);
	static void GetGuidStructureFromString(GUID* bytes, const wchar_t buffer[37]);
	static const wchar_t* GetStringFromPartitionTypeGUID(GUID guid);
	static const wchar_t* GetStringFromPartitionSystemID(char systemID);
	static const wchar_t* GetStringFromPartitionTypeCode(short typeCode);
	static const GUID* GetGUIDFromPartitionTypeCode(short typeCode);
	static long CalculateCRC32(char* data, unsigned long long length);

	static Page* CurrentPage;
	static bool ShowNoInfoDialogs;
private:
	static Console* currentConsole;
	static Page* nextPage;
	static std::stack<Page*> pageStack;
	static bool shouldExit;
	static int exitCode;
};
