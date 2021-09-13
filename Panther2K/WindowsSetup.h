#pragma once

#include "Console.h"
#include "Page.h"
#include "WelcomePage.h"
#include <wimgapi.h>

void DoEvents();
wchar_t** SplitStringToLines(const wchar_t* string, int maxWidth, int* lineCount);

struct ImageInfo
{
	unsigned int Architecture;
	wchar_t* DisplayName;
	SYSTEMTIME CreationTime;
};

static class WindowsSetup
{
public:
	static wchar_t GetFirstFreeDrive();
	static bool LoadConfig();

	static int RunSetup();
	static void LoadPhase(int phase);
	static void KeyHandler(WPARAM wParam);
	static void LoadPage(Page* page);

	static bool LoadWimFile();
	static void GetWimImageCount();
	static void EnumerateImageInfo();

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
	static COLOR BackgroundColor;
	static COLOR ForegroundColor;
	static COLOR ProgressBarColor;
	static COLOR ErrorColor;
	static COLOR LightForegroundColor;
	static COLOR DarkForegroundColor;
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

	static Console* console;
	static Page* currentPage;
};

