#pragma once

#include <PantherConsole.h>
#include <PantherLogger.h>
#include "Page.h"
#include "WelcomePage.h"
#include <wimgapi.h>

void DoEvents();

struct ImageInfo
{
	unsigned int Architecture;
	wchar_t* DisplayName;
	SYSTEMTIME CreationTime;
};

typedef struct VOLUME_INFO;

static class WindowsSetup
{
public:
	static wchar_t GetFirstFreeDrive();
	static bool LoadConfig();

	// Program loop
	static int RunSetup();
	static void LoadPhase(int phase);
	static bool KeyHandler(WPARAM wParam);
	static void LoadPage(Page* page);

	// Partition selection (phase 4)
	static void SelectPartition(int stringIndex, VOLUME_INFO volume);
	static void SelectNextPartition(int index);

	// WinParted functions
	static int RunPartitionManager();
	static bool SelectPartitionsWithDisk(int diskNumber);
	static bool SetPartitionType(int disk, unsigned long long offset, short type);

	// Drivers
	static void LoadDrivers();
	static void InstallDrivers();

	// Wim file
	static bool LoadWimFile();
	static void GetWimImageCount();
	static void EnumerateImageInfo();

	static LibPanther::Logger* GetLogger();

	/*
	 * Configuration
	 */

	// Console
	static bool UseCp437;
	static long Columns;
	static long Rows;

	// Phase shit
	static bool CanGoBack;
	static bool SkipPhase1;
	static bool SkipPhase2Wim;
	static bool SkipPhase2Image;
	static bool SkipPhase3;
	static bool SkipPhase4_1;
	static bool SkipPhase4_2;
	static bool SkipPhase4_3;

	// Global
	static bool IsWinPE;
	static int BackgroundColor;
	static int ForegroundColor;
	static int ProgressBarColor;
	static int ErrorColor;
	static int LightForegroundColor;
	static int DarkForegroundColor;
	static COLOR ConfigBackgroundColor;
	static COLOR ConfigForegroundColor;
	static COLOR ConfigProgressBarColor;
	static COLOR ConfigErrorColor;
	static COLOR ConfigLightForegroundColor;
	static COLOR ConfigDarkForegroundColor;

	// Phase 2
	static const wchar_t* WimFile;
	static HANDLE WimHandle;
	static int WimImageCount;
	static ImageInfo* WimImageInfos;
	static int WimImageIndex;

	// Phase 3
	static bool UseLegacy;

	// Phase 4
	static const wchar_t* Partition1Volume;
	static const wchar_t* Partition2Volume;
	static const wchar_t* Partition3Volume; 
	static const wchar_t* Partition1Mount;
	static const wchar_t* Partition2Mount;
	static const wchar_t* Partition3Mount;
	static bool UseRecovery;
	static bool AllowOtherFileSystems;
	static bool AllowSmallVolumes;

	// Phase 5
	static bool ShowFileNames;
	static int FileNameLength;

	// Phase 6
	static bool ContinueWithoutRecovery;

	// Phase 7
	static int RebootTimer;
private:
	static bool LoadPartitionFromMount(const wchar_t* buffer, const wchar_t** destVolume, const wchar_t** destMount);
	static bool LoadPartitionFromVolume(wchar_t* buffer, const wchar_t* rootPath, const wchar_t* mountPath, const wchar_t** destVolume, const wchar_t** destMount);
	static bool LocateWimFile(wchar_t* buffer);

	static LibPanther::Logger* logger;
	static Console* console;
	static Page* currentPage;
};

