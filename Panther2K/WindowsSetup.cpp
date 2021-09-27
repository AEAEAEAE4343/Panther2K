#include "WindowsSetup.h"
#include "..\rapidxml.1.13.0\build\native\include\rapidxml\rapidxml.hpp"

#include "WelcomePage.h"
#include "ImageSelectionPage.h"
#include "BootMethodSelectionPage.h"
#include "WimApplyPage.h"
#include "BootPreparationPage.h"
#include "FinalPage.h"
#include "MessageBoxPage.h"
#include "PartitionSelectionPage.h"

#include "Logger.h"

#include "shlwapi.h"
#include "shlobj.h"
#include "Gdiplus.h"
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

// Config
bool WindowsSetup::UseCp437 = true;
bool WindowsSetup::CanGoBack = true;
bool WindowsSetup::SkipPhase1 = false;
bool WindowsSetup::SkipPhase2Wim = false;
bool WindowsSetup::SkipPhase2Image = false;
bool WindowsSetup::SkipPhase3 = false;
bool WindowsSetup::SkipPhase4_1 = false;
bool WindowsSetup::SkipPhase4_2 = false;
bool WindowsSetup::SkipPhase4_3 = false;

bool WindowsSetup::IsWinPE = true;
COLOR WindowsSetup::BackgroundColor = COLOR{ 0, 0, 170};
COLOR WindowsSetup::ForegroundColor = COLOR{ 170, 170, 170 };
COLOR WindowsSetup::ErrorColor = COLOR{ 170, 0, 0 };
COLOR WindowsSetup::ProgressBarColor = COLOR{ 255, 255, 0 };
COLOR WindowsSetup::LightForegroundColor = COLOR{ 255, 255, 255 };
COLOR WindowsSetup::DarkForegroundColor = COLOR{ 0, 0, 0 };
COLOR WindowsSetup::ConfigBackgroundColor = COLOR{ 0, 0, 170 };
COLOR WindowsSetup::ConfigForegroundColor = COLOR{ 170, 170, 170 };
COLOR WindowsSetup::ConfigErrorColor = COLOR{ 170, 0, 0 };
COLOR WindowsSetup::ConfigProgressBarColor = COLOR{ 255, 255, 0 };
COLOR WindowsSetup::ConfigLightForegroundColor = COLOR{ 255, 255, 255 };
COLOR WindowsSetup::ConfigDarkForegroundColor = COLOR{ 0, 0, 0 };

const wchar_t* WindowsSetup::WimFile = L"C:\\SCT\\install.wim";
HANDLE WindowsSetup::WimHandle = 0;
int WindowsSetup::WimImageCount = -1;
ImageInfo* WindowsSetup::WimImageInfos = 0;
int WindowsSetup::WimImageIndex = 0;

bool WindowsSetup::UseLegacy = false;

const wchar_t* WindowsSetup::Partition1Volume = L"";
const wchar_t* WindowsSetup::Partition2Volume = L"";
const wchar_t* WindowsSetup::Partition3Volume = L"";
const wchar_t* WindowsSetup::Partition1Mount = L"";
const wchar_t* WindowsSetup::Partition2Mount = L"";
const wchar_t* WindowsSetup::Partition3Mount = L"";
bool WindowsSetup::AllowOtherFileSystems = true;

bool WindowsSetup::ShowFileNames = true;
int WindowsSetup::FileNameLength = 12;

bool WindowsSetup::ContinueWithoutRecovery = true;

int WindowsSetup::RebootTimer = 10000;

Console* WindowsSetup::console = 0;
Page* WindowsSetup::currentPage = 0;
void DoEvents()
{
	MSG msg;
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

bool lstreqW(const wchar_t* string, const wchar_t* comparator)
{
	int len = lstrlenW(comparator);
	if (lstrlenW(string) != len)
		return false;
	for (int i = 0; i < len; i++)
		if (string[i] != comparator[i])
			return false;
	return true;
}

bool IsSet(DWORD bitfield, int index)
{
	DWORD flag = 1;
	flag <<= index;
	return (bitfield & flag) != FALSE;
}

wchar_t* ConCatW(const wchar_t* s1, const wchar_t* s2)
{
	const size_t len1 = lstrlenW(s1);
	const size_t len2 = lstrlenW(s2);
	wchar_t* result = (wchar_t*)malloc((len1 + len2 + 1) * sizeof(wchar_t)); // +1 for the null-terminator
	if (!result)
		return 0;
	memcpy(result, s1, len1 * sizeof(wchar_t));
	memcpy(result + len1, s2, (len2 + 1) * sizeof(wchar_t)); // +1 to copy the null-terminator
	return result;
}

wchar_t WindowsSetup::GetFirstFreeDrive()
{
	const wchar_t* chars = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	DWORD letters = GetLogicalDrives();
	for (int i = 2; i < 28; i++)
		if (!IsSet(letters, i % 26))
			return chars[i % 26];
	return L'0';
}

bool WindowsSetup::LoadPartitionFromMount(const wchar_t* buffer, const wchar_t** destVolume, const wchar_t** destMount)
{
	int c = lstrlenW(buffer);
	wchar_t* partition = (wchar_t*)malloc(sizeof(wchar_t) * c);
	if (!partition)
	{
		MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to allocate memory through malloc(). Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}
	memcpy(partition, buffer + 1, c * sizeof(wchar_t));
	*destMount = partition;
	wchar_t* tempVolume = (wchar_t*)malloc(sizeof(wchar_t) * 50);
	if (!tempVolume)
	{
		MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to allocate memory through malloc(). Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}
	GetVolumeNameForVolumeMountPointW(WindowsSetup::Partition1Mount, tempVolume, 50);
	*destVolume = tempVolume;
}

bool WindowsSetup::LoadPartitionFromVolume(wchar_t* buffer, const wchar_t* rootPath, const wchar_t* mountPath, const wchar_t** destVolume, const wchar_t** destMount)
{
	int c = lstrlenW(buffer);
	wchar_t* partition = (wchar_t*)malloc(sizeof(wchar_t) * c);
	if (!partition)
	{
		MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to allocate memory through malloc(). Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}
	memcpy(partition, buffer + 1, c * sizeof(wchar_t));
	*destVolume = partition;
	DWORD length = 0;
	GetVolumePathNamesForVolumeNameW(*destVolume, buffer, length, &length);
	if (length != 0)
	{
		wchar_t* pathBuffer = (wchar_t*)malloc(sizeof(wchar_t) * length);
		if (!pathBuffer)
		{
			MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to allocate memory through malloc(). Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		GetVolumePathNamesForVolumeNameW(*destVolume, pathBuffer, length, &length);
		int bufferPtr = 0;
		for (int i = 0; i < length; i++)
		{
			buffer[bufferPtr] = pathBuffer[i];
			if (pathBuffer[i] == L'\0')
			{
				if (bufferPtr)
					DeleteVolumeMountPointW(buffer);
				bufferPtr = 0;
				if (pathBuffer[i + 1] == L'\0')
					break;
			}
			else
				bufferPtr++;
		}
	}
	if (WindowsSetup::IsWinPE)
	{
		wchar_t* mountPoint = (wchar_t*)malloc(sizeof(wchar_t) * 4);
		if (!mountPoint)
		{
			MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to allocate memory through malloc(). Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		mountPoint[0] = WindowsSetup::GetFirstFreeDrive();
		if (mountPoint[0] == L'0')
		{
			MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to mount partition. There are no drive letters available. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		memcpy(mountPoint + 1, L":\\", 3 * sizeof(wchar_t));
		*destMount = mountPoint;
		if (!SetVolumeMountPointW(*destMount, *destVolume))
		{
			swprintf(buffer, MAX_PATH, L"Mounting partition to mount point %s failed (0x%08x). Panther2K will exit.", *destMount, GetLastError());
			MessageBoxPage* msgBox = new MessageBoxPage(buffer, true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
	}
	else
	{
		*destMount = ConCatW(rootPath, mountPath);
		c = SHCreateDirectoryExW(NULL, *destMount, NULL);
		if (c != 0 &&
			c != ERROR_FILE_EXISTS &&
			c != ERROR_ALREADY_EXISTS)
		{
			swprintf(buffer, MAX_PATH, L"Creating mount directory for partition failed (0x%08x). Panther2K will exit.", c);
			MessageBoxPage* msgBox = new MessageBoxPage(buffer, true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		if (!SetVolumeMountPointW(*destMount, *destVolume))
		{
			swprintf(buffer, MAX_PATH, L"Mounting partition to mount point %s failed (0x%08x). Panther2K will exit.", *destMount, GetLastError());
			MessageBoxPage* msgBox = new MessageBoxPage(buffer, true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
	}
}

bool FileExists(const wchar_t* szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool WindowsSetup::LocateWimFile(wchar_t* buffer)
{
	wchar_t pathBuffer[MAX_PATH + 1];
	const wchar_t* chars = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	DWORD letters = GetLogicalDrives();
	for (int i = 2; i < 28; i++)
		if (IsSet(letters, i % 26))
		{
			memcpy(pathBuffer, L"0:\\sources\\install.wim", sizeof(wchar_t) * 23);
			pathBuffer[0] = chars[i % 26];
			if (FileExists(pathBuffer))
			{
				memcpy(buffer, pathBuffer, sizeof(wchar_t) * 23);
				return true;
			}
			memcpy(pathBuffer, L"0:\\sources\\install.esd", sizeof(wchar_t) * 23);
			pathBuffer[0] = chars[i % 26];
			if (FileExists(pathBuffer))
			{
				memcpy(buffer, pathBuffer, sizeof(wchar_t) * 23);
				return true;
			}
		}
	return false;
}

COLOR ParseColor(const wchar_t* text, bool* success)
{
	unsigned char rgb[3] = { 0 };
	*success = swscanf_s(text, L"%hhu,%hhu,%hhu", &rgb[0], &rgb[1], &rgb[2]) == 3;
	return COLOR{ rgb[0], rgb[1], rgb[2] };
}

bool WindowsSetup::LoadConfig()
{
	wchar_t buffer[MAX_PATH] = L"";
	const wchar_t* INIFileRelative = L".\\panther.ini";
	wchar_t INIFile[MAX_PATH] = L"";
	wchar_t rootCwdPath[MAX_PATH] = L"";
	wchar_t rootFilePath[MAX_PATH] = L"";
	DWORD length = 0; 
	MessageBoxPage* msgBox = 0;

	_wgetcwd(rootCwdPath, MAX_PATH);
	PathStripToRootW(rootCwdPath);
	GetModuleFileNameW(NULL, rootFilePath, MAX_PATH);
	PathStripToRootW(rootFilePath);

	GetFullPathNameW(L"panther.ini", MAX_PATH, INIFile, NULL);

	// Generic 
	GetPrivateProfileStringW(L"Generic", L"CanGoBack", L"Yes", buffer, 4, INIFile);
	CanGoBack = lstreqW(buffer, L"Yes");

	// Phase 1
	GetPrivateProfileStringW(L"Phase1", L"SkipWelcome", L"No", buffer, 4, INIFile);
	SkipPhase1 = lstreqW(buffer, L"Yes");

	// Phase 2
	GetPrivateProfileStringW(L"Phase2", L"WimFile", L"Auto", buffer, MAX_PATH, INIFile);
	
	if (lstreqW(buffer, L"Auto"))
	{
		if (!LocateWimFile(buffer))
		{
			msgBox = new MessageBoxPage(L"The WIM file could not be found automatically. Please specify the location of the WIM file top use in the config. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}

		wchar_t* wimPath = (wchar_t*)malloc(sizeof(wchar_t) * (lstrlenW(buffer) + 1));
		memcpy(wimPath, buffer, sizeof(wchar_t) * (lstrlenW(buffer) + 1));
		WimFile = wimPath;
		if (!LoadWimFile())
		{
			msgBox = new MessageBoxPage(L"The WIM file specified in the config could not be loaded. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		GetWimImageCount();
		EnumerateImageInfo();
	}
	else if (!lstreqW(buffer, L"Ask"))
	{
		SkipPhase2Wim = true;
		wchar_t* wimPath;
		if (buffer[0] == L'\\')
			wimPath = (wchar_t*)malloc(sizeof(wchar_t) * (lstrlenW(buffer) + lstrlenW(rootFilePath)));
		else
			wimPath = (wchar_t*)malloc(sizeof(wchar_t) * (lstrlenW(buffer) + 1));
		if (!wimPath)
		{
			msgBox = new MessageBoxPage(L"Failed to allocate memory through malloc(). Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		if (buffer[0] == L'\\')
		{
			memcpy(wimPath, rootFilePath, sizeof(wchar_t) * (lstrlenW(rootFilePath)));
			memcpy(wimPath + lstrlenW(rootFilePath) - 1, buffer, sizeof(wchar_t) * (lstrlenW(buffer) + 1));
		}
		else
			memcpy(wimPath, buffer, sizeof(wchar_t) * (lstrlenW(buffer) + 1));
		WimFile = wimPath;
		if (!LoadWimFile())
		{
			msgBox = new MessageBoxPage(L"The WIM file specified in the config could not be loaded. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
		GetWimImageCount();
		EnumerateImageInfo();
	}
	else
	{
		msgBox = new MessageBoxPage(L"The WIM file specified in the config could not be loaded. Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}
	if (SkipPhase2Wim)
	{
		int index = GetPrivateProfileIntW(L"Phase2", L"WimImageIndex", -1, INIFile);
		if (index != -1)
		{
			SkipPhase2Image = true;
			WimImageIndex = index;
			HANDLE hImage = WIMLoadImage(WimHandle, WimImageIndex);
			if (!hImage)
			{
				msgBox = new MessageBoxPage(L"Failed to load the image index specified in the config. Panther2K will exit.", true, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				return false;
			}
		}
	}

	// Phase 3
	GetPrivateProfileStringW(L"Phase3", L"BootMethod", L"Ask", buffer, MAX_PATH, INIFile);
	if (!lstreqW(buffer, L"Ask"))
	{
		SkipPhase3 = true;
		if (lstrcmpW(L"UEFI", buffer) == 0)
			UseLegacy = false;
		else if (lstrcmpW(L"Legacy", buffer) == 0)
			UseLegacy = true;
		else
		{
			msgBox = new MessageBoxPage(L"The value Phase3\\BootMethod specified in the config file is invalid. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
	}

	// Phase 4
	if (SkipPhase3)
	{
		GetPrivateProfileStringW(L"Phase4", L"Partition1", L"Ask", buffer, MAX_PATH, INIFile);
		if (!lstreqW(buffer, L"Ask"))
		{
			SkipPhase4_1 = true;
			switch (buffer[0])
			{
			case L'N':
				msgBox = new MessageBoxPage(L"Panther2K does not support Disk and Partition number yet. Panther2K will exit.", true, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				return false;
			case L'V':
				if (!LoadPartitionFromVolume(buffer, rootCwdPath, L"$Panther2K\\Sys\\", &Partition1Volume, &Partition1Mount))
					return false;
				break;
			case L'M':
				if (!LoadPartitionFromMount(buffer, &Partition1Volume, &Partition1Mount))
					return false;
				break;
			}
		}

		GetPrivateProfileStringW(L"Phase4", L"Partition3", L"Ask", buffer, MAX_PATH, INIFile);
		if (!lstreqW(buffer, L"Ask"))
		{
			SkipPhase4_3 = true;
			switch (buffer[0])
			{
			case L'N':
				msgBox = new MessageBoxPage(L"Panther2K does not support Disk and Partition number yet. Panther2K will exit.", true, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				return false;
			case L'V':
				if (!LoadPartitionFromVolume(buffer, rootCwdPath, L"$Panther2K\\Win\\", &Partition3Volume, &Partition3Mount))
					return false;
				break;
			case L'M':
				if (!LoadPartitionFromMount(buffer, &Partition3Volume, &Partition3Mount))
					return false;
				break;
			}
		}

		if (!UseLegacy)
		{
			GetPrivateProfileStringW(L"Phase4", L"Partition2", L"Ask", buffer, MAX_PATH, INIFile);
			if (!lstreqW(buffer, L"Ask"))
			{
				SkipPhase4_2 = true;
				switch (buffer[0])
				{
				case L'N':
					msgBox = new MessageBoxPage(L"Panther2K does not support Disk and Partition number yet. Panther2K will exit.", true, currentPage);
					msgBox->ShowDialog();
					delete msgBox;
					return false;
				case L'V':
					if (!LoadPartitionFromVolume(buffer, rootCwdPath, L"$Panther2K\\Rec\\", &Partition2Volume, &Partition2Mount))
						return false;
					break;
				case L'M':
					if (!LoadPartitionFromMount(buffer, &Partition2Volume, &Partition2Mount))
						return false;
					break;
				}
			}
		}
	}
	GetPrivateProfileStringW(L"Phase4", L"AllowOtherFileSystems", L"No", buffer, 4, INIFile);
	AllowOtherFileSystems = lstreqW(buffer, L"Yes");
	
	// Phase 5
	GetPrivateProfileStringW(L"Phase5", L"ShowFileNames", L"No", buffer, 4, INIFile);
	ShowFileNames = lstreqW(buffer, L"Yes");

	// Phase 6
	GetPrivateProfileStringW(L"Phase6", L"ContinueWithoutRecovery", L"Yes", buffer, 4, INIFile);
	ContinueWithoutRecovery = lstreqW(buffer, L"Yes");

	// Phase 7
	RebootTimer = GetPrivateProfileIntW(L"Phase7", L"RebootTimer", 10000, INIFile);

	// Console
	GetPrivateProfileStringW(L"Console", L"ColorScheme", L"Windows Setup", buffer, MAX_PATH, INIFile);
	if (!lstrcmpW(buffer, L"Windows Setup"))
	{
		BackgroundColor = COLOR{ 0, 0, 168 };
		ForegroundColor = COLOR{ 168, 168, 168 };
		ErrorColor = COLOR{ 168, 0, 0 };
		ProgressBarColor = COLOR{ 255, 255, 0 };
		LightForegroundColor = COLOR{ 255, 255, 255 };
		DarkForegroundColor = COLOR{ 0, 0, 0 };
	}
	else if (!lstrcmpW(buffer, L"BIOS (Blue)"))
	{
		BackgroundColor = COLOR{ 0, 0, 170 };
		ForegroundColor = COLOR{ 170, 170, 170 };
		ErrorColor = COLOR{ 170, 0, 0 };
		ProgressBarColor = COLOR{ 255, 255, 0 };
		LightForegroundColor = COLOR{ 255, 255, 255 };
		DarkForegroundColor = COLOR{ 0, 0, 0 };
	}
	else if (!lstrcmpW(buffer, L"BIOS (Black)"))
	{
		BackgroundColor = COLOR{ 0, 0, 0 };
		ForegroundColor = COLOR{ 170, 170, 170 };
		ErrorColor = COLOR{ 170, 0, 0 };
		ProgressBarColor = COLOR{ 255, 255, 0 };
		LightForegroundColor = COLOR{ 255, 255, 255 };
		DarkForegroundColor = COLOR{ 0, 0, 0 };
	}
	else if (!lstrcmpW(buffer, L"Custom"))
	{
		goto parse;
	fail:
		msgBox = new MessageBoxPage(L"The color scheme specified in the config file is invalid. Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	parse:
		bool success = false;
		GetPrivateProfileStringW(L"Console", L"BackgroundColor", L"Fail", buffer, MAX_PATH, INIFile);
		BackgroundColor = ParseColor(buffer, &success);
		if (!success) goto fail;
		GetPrivateProfileStringW(L"Console", L"ForegroundColor", L"Fail", buffer, MAX_PATH, INIFile);
		ForegroundColor = ParseColor(buffer, &success);
		if (!success) goto fail;
		GetPrivateProfileStringW(L"Console", L"ErrorColor", L"Fail", buffer, MAX_PATH, INIFile);
		ErrorColor = ParseColor(buffer, &success);
		if (!success) goto fail;
		GetPrivateProfileStringW(L"Console", L"ProgressBarColor", L"Fail", buffer, MAX_PATH, INIFile);
		ProgressBarColor = ParseColor(buffer, &success);
		if (!success) goto fail;
		GetPrivateProfileStringW(L"Console", L"LightForegroundColor", L"Fail", buffer, MAX_PATH, INIFile);
		LightForegroundColor = ParseColor(buffer, &success);
		if (!success) goto fail;
		GetPrivateProfileStringW(L"Console", L"DarkForegroundColor", L"Fail", buffer, MAX_PATH, INIFile);
		DarkForegroundColor = ParseColor(buffer, &success);
		if (!success) goto fail;
	}
	else
	{
		msgBox = new MessageBoxPage(L"The value Console\\ColorScheme specified in the config file is invalid. Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}

	int columns = GetPrivateProfileIntW(L"Console", L"Columns", 80, INIFile);
	int rows = GetPrivateProfileIntW(L"Console", L"Rows", 25, INIFile);
	int fontHeight = GetPrivateProfileIntW(L"Console", L"FontHeight", -1, INIFile);
	if (fontHeight == -1)
	{
		msgBox = new MessageBoxPage(L"The value Console\\FontHeight specified in the config file is invalid. Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}
	GetPrivateProfileStringW(L"Console", L"FontSmoothing", L"No", buffer, 4, INIFile);
	bool smooth = lstreqW(buffer, L"Yes");
	GetPrivateProfileStringW(L"Console", L"Font", L"Windows Setup", buffer, MAX_PATH, INIFile);
	HFONT font = CreateFontW(fontHeight, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, smooth ? DEFAULT_QUALITY : NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, buffer);
	LOGFONTW lf = { 0 };
	GetObjectW(font, sizeof(LOGFONTW), &lf);
	if (lstrcmpW(buffer, lf.lfFaceName))
	{
		msgBox = new MessageBoxPage(L"Loading the font specified in Console\\FontHeight in the config file failed. Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}
	GetPrivateProfileStringW(L"Console", L"UseCodePage437", L"Yes", buffer, 4, INIFile);
	UseCp437 = lstreqW(buffer, L"Yes");
	console->ReloadSettings(columns, rows, font);

	ConfigBackgroundColor = BackgroundColor;
	ConfigForegroundColor = ForegroundColor;
	ConfigErrorColor = ErrorColor;
	ConfigProgressBarColor = ProgressBarColor;
	ConfigLightForegroundColor = LightForegroundColor;
	ConfigDarkForegroundColor = DarkForegroundColor;

	return true;
}

int WindowsSetup::RunSetup()
{
	MSG msg;

	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Create console
	Console::Init();
	console = Console::CreateConsole();
	console->OnKeyPress = KeyHandler;
	ShowWindow(console->WindowHandle, 4);
	SendMessage(console->WindowHandle, WM_KEYDOWN, VK_F11, 0);

	// Load config
	Page* loadPage = new Page();
	LoadPage(loadPage);
	loadPage->text = L"Panther2K";
	loadPage->statusText = L"  Windows is starting Panther2K";
	loadPage->Draw();

	DoEvents();
	if (!LoadConfig())
		return (0b11 << 30) || (FACILITY_WIN32 << 16) || (ERROR_INVALID_PARAMETER); /* 0b11 (Error) + FACILITY_WIN32 + ERROR_INVALID_PARAMETER */

	// Start welcome phase
	LoadPhase(1);

	// Main message loop:
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	GdiplusShutdown(gdiplusToken);
	return (int)msg.wParam;
}

void WindowsSetup::LoadPhase(int phase)
{
	Page* page;
	switch (phase)
	{
	case 1:
		if (!SkipPhase1)
		{
			page = new WelcomePage();
			break;
		}
	case 2:
		// TODO: Add WIM file selection
		if (!SkipPhase2Image)
		{
			// If the image count is not -1 we have already loaded the image
			if (WimImageCount == -1)
			{
				GetWimImageCount();
				EnumerateImageInfo();
			}
			if (WimImageCount != 1)
			{
				page = new ImageSelectionPage();
				break;
			}
			else
				WimImageIndex = 1;
		}
		else if (WimImageCount == -1)
		{
			// If we skip phase 2, we still need to get the image count and info to display what we're installing
			GetWimImageCount();
			EnumerateImageInfo();
		}
	case 3:
		if (!SkipPhase3)
		{
			page = new BootMethodSelectionPage();
			break;
		}
	case 4:
		SelectNextPartition(-1, 1);
		return;
		// TODO: Find out what partitions the user wants to install to
		//if (!SkipPhase4_1)
		//{
	case 5:
		// Apply the image
		// 
		// Doing this is going to produce the following call stack:
		//   Console::WndProc (WM_KEYDOWN from Phase 4 (or earlier))
		//   LoadPhase (5)
		//   WimApplyPage::ApplyImage 
		//   WimApplyPage::WimMessageLoop
		// 
		// Only when the applying of the image finishes will the WM_KEYDOWN message return
		// If you are reading this, I need help figuring out what the fuck to do here.
		//  - Leet
		page = new WimApplyPage();
		LoadPage(page);
		((WimApplyPage*)page)->ApplyImage();
	case 6:
		// Generate boot manager files and BCD
		page = new BootPreparationPage();
		LoadPage(page);
		((BootPreparationPage*)page)->PrepareBootFiles();
	case 7:
		// Finalize installation
		page = new FinalPage();
		break;
	default:
		page = new Page();
		break;
	}

	LoadPage(page);
}

void WindowsSetup::SelectPartition(int stringIndex, VOLUME_INFO volume)
{
	int partitionIds[4] = { 3, 1, 1, 2 };
	wchar_t buffer[MAX_PATH + 1];
	wchar_t rootPath[MAX_PATH + 1];

	_wgetcwd(rootPath, MAX_PATH);
	PathStripToRootW(rootPath);

	const wchar_t** targetVolume;
	const wchar_t** targetMount;
	const wchar_t* mount;
	switch (partitionIds[stringIndex])
	{
	case 1:
		targetVolume = &Partition1Volume;
		targetMount = &Partition1Mount;
		mount = L"$Panther2K\\Sys\\";
		break;
	case 2:
		targetVolume = &Partition2Volume;
		targetMount = &Partition2Mount;
		mount = L"$Panther2K\\Rec\\";
		break;
	case 3:
		targetVolume = &Partition3Volume;
		targetMount = &Partition3Mount;
		mount = L"$Panther2K\\Win\\";
		break;
	default:
		return;
	}
	(*targetVolume) = (wchar_t*)malloc(sizeof(wchar_t) * (MAX_PATH + 1));
	(*targetMount) = (wchar_t*)malloc(sizeof(wchar_t) * (MAX_PATH + 1));
	if (!(*targetVolume) || !(*targetMount))
		return;

	lstrcpyW((LPWSTR)*targetVolume, volume.guid);
	if (lstrcmpW(volume.mountPoint, L"") == 0)
	{
		buffer[0] = L'V';
		lstrcpyW(buffer + 1, volume.guid);
		LoadPartitionFromVolume(buffer, rootPath, mount, targetVolume, targetMount);
	}
	else
		lstrcpyW((LPWSTR)*targetMount, volume.mountPoint);
}

void WindowsSetup::SelectNextPartition(int stringIndex, int direction)
{
	Page* page;
	int partitionIds[4] = { 3, 1, 1, 2 };

	// Legacy / UEFI overrides
	int nextPage = stringIndex + direction;
	if (nextPage == 1 && UseLegacy)
		nextPage += direction;
	else if (nextPage == 2 && !UseLegacy)
		nextPage += direction;
	else if (nextPage == 3 && UseLegacy)
		nextPage += direction;
	
	// Config overrides
	if (partitionIds[nextPage] == 1 && SkipPhase4_1)
		nextPage += direction;
	else if (partitionIds[nextPage] == 2 && SkipPhase4_2)
		nextPage += direction;
	else if (partitionIds[nextPage] == 3 && SkipPhase4_3)
		nextPage += direction;

	// Navigate out of Phase 4
	if (nextPage == -1)
	{
		LoadPhase(3);
		return;
	}
	else if (nextPage == 4)
	{
		LoadPhase(5);
		return;
	}

	// If there were overrides, re-evaluate everything
	if (nextPage != stringIndex + direction)
	{
		nextPage -= direction;
		SelectNextPartition(nextPage, direction);
		return;
	}

	switch (nextPage)
	{
	case 0:
		page = new PartitionSelectionPage(L"NTFS", 0, 0, 0); 
		break;
	case 1:
		page = new PartitionSelectionPage(L"FAT32", 0, 40000000, 1);
		break;
	case 2:
		page = new PartitionSelectionPage(L"NTFS", 500000000, 0, 2);
		break;
	case 3:
		page = new PartitionSelectionPage(L"NTFS", 500000000, 0, 3);
		break;
	default:
		return;
	}

	LoadPage(page);
}

bool waitingForKey = false;
const wchar_t* oldStatus = 0;
Page* oldPage = 0;
void WindowsSetup::KeyHandler(WPARAM wParam)
{
	if (waitingForKey)
	{
		switch (wParam)
		{
		case VK_NUMPAD0:
			BackgroundColor = ConfigBackgroundColor;
			ForegroundColor = ConfigForegroundColor;
			ErrorColor = ConfigErrorColor;
			ProgressBarColor = ConfigProgressBarColor;
			LightForegroundColor = ConfigLightForegroundColor;
			DarkForegroundColor = ConfigDarkForegroundColor;
			break;
		case VK_NUMPAD1:
			BackgroundColor = COLOR{ 0, 0, 170 };
			ForegroundColor = COLOR{ 170, 170, 170 };
			ErrorColor = COLOR{ 170, 0, 0 };
			ProgressBarColor = COLOR{ 255, 255, 0 };
			LightForegroundColor = COLOR{ 255, 255, 255 };
			DarkForegroundColor = COLOR{ 0, 0, 0 };
			break;
		case VK_NUMPAD2:
			BackgroundColor = COLOR{ 0, 0, 168 };
			ForegroundColor = COLOR{ 168, 168, 168 };
			ErrorColor = COLOR{ 168, 0, 0 };
			ProgressBarColor = COLOR{ 255, 255, 0 };
			LightForegroundColor = COLOR{ 255, 255, 255 };
			DarkForegroundColor = COLOR{ 0, 0, 0 };
			break;
		case VK_NUMPAD3:
			BackgroundColor = COLOR{ 0, 0, 0 };
			ForegroundColor = COLOR{ 170, 170, 170 };
			ErrorColor = COLOR{ 170, 0, 0 };
			ProgressBarColor = COLOR{ 255, 255, 0 };
			LightForegroundColor = COLOR{ 255, 255, 255 };
			DarkForegroundColor = COLOR{ 0, 0, 0 };
			break;
		}
		if (oldPage == currentPage)
		{
			currentPage->statusText = oldStatus;
			currentPage->Draw();
		}
		waitingForKey = false;
	}
	else if (wParam == VK_F7)
	{
		oldPage = currentPage;
		oldStatus = currentPage->statusText;
		currentPage->statusText = L"  NUM0=Default  NUM1=BIOS (Blue)  NUM2=Windows Setup  NUM3=BIOS (Dark)";
		currentPage->Draw();
		waitingForKey = true;
	}
	else
		currentPage->HandleKey(wParam);
}

void WindowsSetup::LoadPage(Page* page)
{
	if (currentPage)
	{
		delete currentPage;
	}

	page->Initialize(console);
	currentPage = page;
}

bool WindowsSetup::LoadWimFile()
{
	WimHandle = WIMCreateFile(WimFile, WIM_GENERIC_READ, WIM_OPEN_EXISTING, 0, WIM_COMPRESS_NONE, NULL);
	if (!WimHandle)
	{
		return false;
	}
	else
	{
		wchar_t path[MAX_PATH];
		GetTempPathW(MAX_PATH, path);
		WIMSetTemporaryPath(WimHandle, path);
	}
	return true;
}

void WindowsSetup::GetWimImageCount()
{
	if (WimHandle) WimImageCount = WIMGetImageCount(WimHandle);
}

char* getCharPointer(wchar_t* string, int count = -1)
{
	if (count == -1)
		count = lstrlenW(string);
	char* oldtext = (char*)string;
	char* text = (char*)malloc(sizeof(char) * count);
	if (text == 0)
		return NULL;
	for (int i = 0; i < count; i++)
	{
		int j = i * 2;
		text[i] = oldtext[j];
	}
	return text;
}

wchar_t* getWcharPointer(char* string)
{
	int count = strlen(string);
	wchar_t* text = (wchar_t*)malloc(sizeof(wchar_t) * (count + 1));
	if (text == 0)
		return NULL;
	for (int i = 0; i < count; i++)
		text[i] = string[i];
	text[count] = 0;
	return text;
}

void WindowsSetup::EnumerateImageInfo()
{
	wchar_t* str_ptr;
	long str_len;
	FILETIME fTime;
	SYSTEMTIME time;

	rapidxml::xml_document<>* doc = new rapidxml::xml_document<>();
	ImageInfo* pointer = (ImageInfo*)malloc(sizeof(ImageInfo) * WimImageCount);
	if (pointer == NULL)
		return;

	WIMGetImageInformation(WimHandle, (PVOID*)&str_ptr, (PDWORD)&str_len);
	str_ptr++;

	char* xml = getCharPointer(str_ptr, str_len / 2);
	int count = strlen(xml);
	doc->parse<0>(xml);

	// Parse XML
	rapidxml::xml_node<>* wimNode = doc->first_node();
	rapidxml::xml_node<>* imageNode = wimNode->first_node("IMAGE");
	rapidxml::xml_node<>* creationTimeNode;
	rapidxml::xml_node<>* windowsNode;
	rapidxml::xml_node<>* node;

	for (int i = 1; i <= WimImageCount; i++, imageNode = imageNode->next_sibling())
	{
		creationTimeNode = imageNode->first_node("CREATIONTIME");
		windowsNode = imageNode->first_node("WINDOWS");

		node = windowsNode->first_node("ARCH");
		(pointer + i - 1)->Architecture = node->value()[0] - '0';
		node = imageNode->first_node("DISPLAYNAME");
		if (node == NULL)
			node = imageNode->first_node("NAME");
		(pointer + i - 1)->DisplayName = getWcharPointer(node->value());

		wchar_t* end_str;
		node = creationTimeNode->first_node("HIGHPART");
		wchar_t* creationHigh = getWcharPointer(node->value());
		fTime.dwHighDateTime = wcstoul(creationHigh, &end_str, 0);
		node = creationTimeNode->first_node("LOWPART");
		wchar_t* creationLow = getWcharPointer(node->value());
		fTime.dwLowDateTime = wcstoul(creationLow, &end_str, 0);
		FileTimeToSystemTime(&fTime, &time);
		(pointer + i - 1)->CreationTime = time;
	}

	delete doc;
	WimImageInfos = pointer;
}
