﻿#include "WindowsSetup.h"
#include "..\RapidXML\RapidXML\rapidxml.hpp"

#include "WelcomePage.h"
#include "ImageSelectionPage.h"
#include "BootMethodSelectionPage.h"
#include "WimApplyPage.h"
#include "BootPreparationPage.h"
#include "FinalPage.h"
#include "MessageBoxPage.h"
#include "PartitionSelectionPage.h"
#include "DiskSelectionPage.h"
#include "WinPartedDll.h"
#include "wdkpartial.h"

#include "Gdiplus.h"
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

#include "Resource.h"
#include <Shlwapi.h>
#include "pathcch.h"

// Undocumented WIMGAPI flag, loads the file with solid compression
#define WIM_FLAG_SOLIDCOMPRESSION 0x20000000

// Config
bool WindowsSetup::UseCp437 = false;
bool WindowsSetup::CanGoBack = true;
bool WindowsSetup::SkipPhase1 = false;
bool WindowsSetup::SkipPhase2Wim = false;
bool WindowsSetup::SkipPhase2Image = false;
bool WindowsSetup::SkipPhase3 = false;
bool WindowsSetup::SkipPhase4_1 = false;
bool WindowsSetup::SkipPhase4_2 = false;
bool WindowsSetup::SkipPhase4_3 = false;

bool WindowsSetup::IsWinPE = false;
int WindowsSetup::BackgroundColor = 0;
int WindowsSetup::ForegroundColor = 1;
int WindowsSetup::ErrorColor = 2;
int WindowsSetup::ProgressBarColor = 3;
int WindowsSetup::LightForegroundColor = 4;
int WindowsSetup::DarkForegroundColor = 5;
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
VOLUME_INFO WindowsSetup::SystemPartition = { 0 };
VOLUME_INFO WindowsSetup::BootPartition = { 0 };
VOLUME_INFO WindowsSetup::RecoveryPartition = { 0 };
bool WindowsSetup::UseRecovery = true;
bool WindowsSetup::AllowOtherFileSystems = true;
bool WindowsSetup::AllowSmallVolumes = true;
bool WindowsSetup::ShowFileNames = true;
int WindowsSetup::FileNameLength = 12;
bool WindowsSetup::ContinueWithoutRecovery = true;
int WindowsSetup::RebootTimer = 10000;

Console* WindowsSetup::console = 0;
LibPanther::Logger* WindowsSetup::logger = 0;
Page* WindowsSetup::currentPage = 0;
bool WindowsSetup::exitRequested = false;

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

NtQueryVolumeInformationFileFunction NtQueryVolumeInformationFile;
bool WindowsSetup::GetVolumeInfoFromName(const wchar_t* volumeName, PVOLUME_INFO pvi)
{
	if (!NtQueryVolumeInformationFile)
	{
		HINSTANCE hinstKrnl = LoadLibraryW(L"ntdll.dll");
		if (hinstKrnl == 0)
		{
			//ERROR OUT
			return false;
		}
		NtQueryVolumeInformationFile = (NtQueryVolumeInformationFileFunction)GetProcAddress(hinstKrnl, "NtQueryVolumeInformationFile");
		if (NtQueryVolumeInformationFile == 0)
		{
			//ERROR OUT
			return false;
		}
	}

	bool returnValue = false;
	DWORD bytesCopied;
	HANDLE volume;

	wchar_t szNextVolNameNoBSlash[MAX_PATH + 1];
	wchar_t fileSystemName[MAX_PATH + 1];
	wchar_t diskPath[MAX_PATH + 1];

	HANDLE volumeFileHandle = 0;
	HANDLE diskFileHandle = 0;
	VOLUME_INFO vi = { 0 };
	IO_STATUS_BLOCK iosb = { 0 };
	FILE_FS_FULL_SIZE_INFORMATION fsi = { 0 };
	VOLUME_DISK_EXTENTS* vde = (VOLUME_DISK_EXTENTS*)safeMalloc(WindowsSetup::GetLogger(), sizeof(VOLUME_DISK_EXTENTS));
	DRIVE_LAYOUT_INFORMATION_EX* dli = (DRIVE_LAYOUT_INFORMATION_EX*)safeMalloc(WindowsSetup::GetLogger(), sizeof(DRIVE_LAYOUT_INFORMATION_EX));
	int partitionCount = 1;
	bool done = false;

	lstrcpyW(szNextVolNameNoBSlash, volumeName);
	szNextVolNameNoBSlash[lstrlenW(szNextVolNameNoBSlash) - 1] = L'\0';

	if (!GetVolumePathNamesForVolumeNameW(volumeName, vi.mountPoint, MAX_PATH + 1, &bytesCopied))
		goto cleanup;

	volumeFileHandle = CreateFileW(szNextVolNameNoBSlash, FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (volumeFileHandle == INVALID_HANDLE_VALUE)
		goto cleanup;
	if (NtQueryVolumeInformationFile(volumeFileHandle, &iosb, &fsi, sizeof(FILE_FS_FULL_SIZE_INFORMATION), FileFsFullSizeInformation))
		goto cleanup;
	vi.totalBytes = (long long)fsi.BytesPerSector * (long long)fsi.SectorsPerAllocationUnit * fsi.TotalAllocationUnits.QuadPart;
	vi.bytesFree = (long long)fsi.BytesPerSector * (long long)fsi.SectorsPerAllocationUnit * fsi.ActualAvailableAllocationUnits.QuadPart;

	if (!GetVolumeInformationByHandleW(volumeFileHandle, vi.name, MAX_PATH + 1, NULL, NULL, NULL, fileSystemName, MAX_PATH))
		goto cleanup;

	if (!DeviceIoControl(volumeFileHandle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, sizeof(VOLUME_DISK_EXTENTS), &bytesCopied, NULL))
		goto cleanup;

	swprintf(diskPath, MAX_PATH, L"\\\\.\\\PHYSICALDRIVE%d", vde->Extents->DiskNumber);
	diskFileHandle = CreateFileW(diskPath, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (diskFileHandle == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == 5)
		{
			MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to get partition info: Access denied. Please re-run Panther2K as Administrator. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			RequestExit();
			return false;
		}
		else goto cleanup;
	}

	do
	{
		if (!DeviceIoControl(diskFileHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, dli, 48 + (partitionCount * 144), &bytesCopied, NULL))
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				goto cleanup;

			size_t size = offsetof(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry[++partitionCount]);
			free(dli);
			dli = (DRIVE_LAYOUT_INFORMATION_EX*)safeMalloc(GetLogger(), size);
		}
		else done = true;
	} while (!done);

	for (int i = 0; i < dli->PartitionCount; i++)
	{
		if (vde->Extents->StartingOffset.QuadPart >= dli->PartitionEntry[i].StartingOffset.QuadPart &&
			vde->Extents->StartingOffset.QuadPart < dli->PartitionEntry[i].StartingOffset.QuadPart + dli->PartitionEntry[i].PartitionLength.QuadPart)
		{
			vi.diskNumber = vde->Extents->DiskNumber;
			vi.partitionNumber = dli->PartitionEntry[i].PartitionNumber;
			vi.partOffset = vde->Extents->StartingOffset.QuadPart;
		}
	}

	free(vde);
	free(dli);
	memcpy(vi.fileSystem, fileSystemName, sizeof(wchar_t) * (MAX_PATH + 1));
	lstrcpyW(vi.guid, szNextVolNameNoBSlash);
	memcpy(pvi, &vi, sizeof(VOLUME_INFO));
	returnValue = true;
cleanup:
	if (volumeFileHandle) CloseHandle(volumeFileHandle);
	return returnValue;
}

/*bool WindowsSetup::LoadPartitionFromMount(const wchar_t* buffer, const wchar_t** destVolume, const wchar_t** destMount)
{
	int c = lstrlenW(buffer);
	wchar_t* partition = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * c);
	memcpy(partition, buffer + 1, c * sizeof(wchar_t));
	*destMount = partition;
	wchar_t* tempVolume = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * 50);
	GetVolumeNameForVolumeMountPointW(*destMount, tempVolume, 50);
	tempVolume[lstrlenW(tempVolume) - 1] = '\x0';
	*destVolume = tempVolume;
	return true;
}

bool WindowsSetup::LoadPartitionFromVolume(wchar_t* buffer, const wchar_t* rootPath, const wchar_t* mountPath, const wchar_t** destVolume, const wchar_t** destMount)
{
	int c = lstrlenW(buffer);
	wchar_t* partition = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * c);
	memcpy(partition, buffer + 1, c * sizeof(wchar_t));
	*destVolume = partition;
	DWORD length = 0;
	GetVolumePathNamesForVolumeNameW(*destVolume, buffer, length, &length);
	if (length != 0)
	{
		wchar_t* pathBuffer = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * length);
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
		wchar_t* mountPoint = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * 4);
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
		c = CreateDirectory(*destMount, NULL);
		if (c == 0) 
		{
			c = GetLastError();
			if (c != ERROR_ALREADY_EXISTS)
			{
				swprintf(buffer, MAX_PATH, L"Creating mount directory for partition failed (0x%08x). Panther2K will exit.", c);
				MessageBoxPage* msgBox = new MessageBoxPage(buffer, true, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				return false;
			}
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

	(const_cast<wchar_t*>(*destVolume))[lstrlenW(*destVolume) - 1] = '\x0';
}*/

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
			memcpy(pathBuffer, L"0:\\sources\\install.swm", sizeof(wchar_t) * 23);
			pathBuffer[0] = chars[i % 26];
			if (FileExists(pathBuffer))
			{
				memcpy(buffer, pathBuffer, sizeof(wchar_t) * 23);
				return true;
			}
		}
	return false;
}

bool WindowsSetup::LoadConfig()
{
	const wchar_t* INIFileRelative = L".\\panther2k.ini";

	DWORD length = 0; 
	MessageBoxPage* msgBox = 0;

	wchar_t rootCwdPath[MAX_PATH] = L"";
	_wgetcwd(rootCwdPath, MAX_PATH);
	PathStripToRoot(rootCwdPath);

	wchar_t rootFilePath[MAX_PATH] = L"";
	GetModuleFileNameW(NULL, rootFilePath, MAX_PATH);
	PathStripToRoot(rootFilePath);

	wchar_t INIFile[MAX_PATH] = L"";
	wchar_t iniBuffer[MAX_PATH] = L"";
	GetFullPathNameW(L"panther2k.ini", MAX_PATH, INIFile, NULL);

	wchar_t logBuffer[MAX_PATH] = L"";
	wsprintfW(logBuffer, L"Loading configuration file '%s'...", INIFile);
	logger->Write(PANTHER_LL_DETAILED, logBuffer);

	// Generic 
	GetPrivateProfileStringW(L"Generic", L"CanGoBack", L"Yes", iniBuffer, 4, INIFile);
	CanGoBack = lstreqW(iniBuffer, L"Yes");

	// Sources
	GetPrivateProfileStringW(L"Sources", L"WimFile", L"Auto", iniBuffer, MAX_PATH, INIFile);
	
	if (lstreqW(iniBuffer, L"Auto"))
	{
		if (!LocateWimFile(iniBuffer))
		{
			logger->Write(PANTHER_LL_BASIC, L"[Sources\\WimFile] The WIM file could not be found automatically.");
			msgBox = new MessageBoxPage(L"The WIM file could not be found automatically. Please specify the location of the WIM file to use in the config. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}

		wchar_t* wimPath = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * (lstrlenW(iniBuffer) + 1));
		memcpy(wimPath, iniBuffer, sizeof(wchar_t) * (lstrlenW(iniBuffer) + 1));
		WimFile = wimPath;
		if (!LoadWimFile())
		{
			wsprintfW(logBuffer, L"[Sources\\WimFile] The WIM file '%s' could not be loaded.", WimFile);
			logger->Write(PANTHER_LL_BASIC, logBuffer);
			msgBox = new MessageBoxPage(L"The WIM file could not be loaded. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
	}

	wchar_t* wimPath;
	if (iniBuffer[0] == L'\\')
		wimPath = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * (lstrlenW(iniBuffer) + lstrlenW(rootFilePath)));
	else
		wimPath = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * (lstrlenW(iniBuffer) + 1));
	if (iniBuffer[0] == L'\\')
	{
		memcpy(wimPath, rootFilePath, sizeof(wchar_t) * (lstrlenW(rootFilePath)));
		memcpy(wimPath + lstrlenW(rootFilePath) - 1, iniBuffer, sizeof(wchar_t) * (lstrlenW(iniBuffer) + 1));
	}
	else
		memcpy(wimPath, iniBuffer, sizeof(wchar_t) * (lstrlenW(iniBuffer) + 1));

	WimFile = wimPath;
	if (!LoadWimFile())
	{
		wsprintfW(logBuffer, L"[Sources\\WimFile] The WIM file '%s' could not be loaded.", WimFile);
		logger->Write(PANTHER_LL_BASIC, logBuffer);
		msgBox = new MessageBoxPage(L"The WIM file specified in the config could not be loaded. Panther2K will exit.", true, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
		return false;
	}

	// WelcomePhase
	GetPrivateProfileStringW(L"WelcomePhase", L"SkipWelcome", L"No", iniBuffer, 4, INIFile);
	SkipPhase1 = lstreqW(iniBuffer, L"Yes");

	// ImageSelectionPhase
	int index = GetPrivateProfileIntW(L"ImageSelectionPhase", L"WimImageIndex", -1, INIFile);
	if (index != -1)
	{
		WimImageIndex = index;
		HANDLE hImage = WIMLoadImage(WimHandle, WimImageIndex);
		if (!hImage)
		{
			wsprintfW(logBuffer, L"[ImageSelectionPhase\\WimImageIndex] Failed to load the image index specified in the config (%d)", WimImageIndex);
			logger->Write(PANTHER_LL_BASIC, logBuffer);
			msgBox = new MessageBoxPage(L"Failed to load the image index specified in the config. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
	}

	// BootSelectionPhase
	GetPrivateProfileStringW(L"BootSelectionPhase", L"BootMethod", L"Ask", iniBuffer, MAX_PATH, INIFile);
	if (!lstreqW(iniBuffer, L"Ask"))
	{
		SkipPhase3 = true;
		if (lstrcmpW(L"UEFI", iniBuffer) == 0)
			UseLegacy = false;
		else if (lstrcmpW(L"Legacy", iniBuffer) == 0)
			UseLegacy = true;
		else
		{
			wsprintfW(logBuffer, L"[Phase3\\BootMethod] Invalid value '%s'. Must be 'UEFI' or 'Legacy'.", iniBuffer);
			logger->Write(PANTHER_LL_BASIC, logBuffer);
			msgBox = new MessageBoxPage(L"The value Phase3\\BootMethod specified in the config file is invalid. Panther2K will exit.", true, currentPage);
			msgBox->ShowDialog();
			delete msgBox;
			return false;
		}
	}

	GetPrivateProfileStringW(L"BootSelectionPhase", L"UseRecovery", L"Yes", iniBuffer, MAX_PATH, INIFile);
	UseRecovery = lstreqW(iniBuffer, L"Yes");

	// TargetSelectionPhase
	GetPrivateProfileStringW(L"TargetSelectionPhase", L"AllowOtherFileSystems", L"No", iniBuffer, 4, INIFile);
	AllowOtherFileSystems = lstreqW(iniBuffer, L"Yes");

	GetPrivateProfileStringW(L"TargetSelectionPhase", L"AllowSmallVolumes", L"No", iniBuffer, 4, INIFile);
	AllowSmallVolumes = lstreqW(iniBuffer, L"Yes");
	
	// InstallationPhase
	GetPrivateProfileStringW(L"InstallationPhase", L"ShowFileNames", L"Yes", iniBuffer, 4, INIFile);
	ShowFileNames = lstreqW(iniBuffer, L"Yes");

	// BootPreparationPhase
	GetPrivateProfileStringW(L"BootPreparationPhase", L"ContinueWithoutRecovery", L"Yes", iniBuffer, 4, INIFile);
	ContinueWithoutRecovery = lstreqW(iniBuffer, L"Yes");

	// EndPhase
	RebootTimer = GetPrivateProfileIntW(L"EndPhase", L"RebootTimer", 10000, INIFile);

	return true;
}

void WindowsSetup::LoadDrivers()
{
	logger->Write(PANTHER_LL_DETAILED, L"Loading drivers for installation...");

	std::vector<WIN32_FIND_DATAW> failedFiles;
	wchar_t commandBuffer[MAX_PATH + 25];
	wchar_t pathBuffer[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, pathBuffer);
	GetModuleFileNameW(NULL, commandBuffer, MAX_PATH + 25);
	PathCchRemoveFileSpec(commandBuffer, MAX_PATH + 25);
	SetCurrentDirectoryW(commandBuffer);

	WIN32_FIND_DATAW ffd;
	DWORD dwAttrib = GetFileAttributesW(L".\\pedrivers");

	if (!(dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)))
	{
		SetCurrentDirectoryW(pathBuffer);
		return;
	}

	HANDLE hFind = FindFirstFileW(L".\\pedrivers\\*", &ffd);
	int lastError = GetLastError();

	wchar_t buffer[MAX_PATH * 2];
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
			if (dot && !wcscmp(dot, L".inf") && !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				swprintf_s(buffer, MAX_PATH * 2, L"Loading driver %s...", ffd.cFileName);
				logger->Write(PANTHER_LL_DETAILED, buffer);

				swprintf_s(commandBuffer, MAX_PATH + 25, L"drvload.exe .\\pedrivers\\%s", ffd.cFileName);
				int ret = _wsystem(commandBuffer);
				if (ret) 
				{
					failedFiles.push_back(ffd);
					continue;
				}
			}

		} while (FindNextFileW(hFind, &ffd) != 0);
		FindClose(hFind);
	}
	else
	{
		logger->Write(PANTHER_LL_NORMAL, L"Could not enumerate Windows PE drivers. Panther2K will start, but a device required for installation might not be available.");
		MessageBoxPage* msgBox = new MessageBoxPage(L"An error occurred while enumerating available drivers. Panther2K will start, but a device required for installation might not be available.", false, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
	}

	for (auto failedFile : failedFiles) 
	{
		swprintf_s(buffer, MAX_PATH * 2, L"An error occurred while loading driver %s. Panther2K will start, but a device required for installation might not be available.", failedFile.cFileName);
		logger->Write(PANTHER_LL_NORMAL, buffer);
		MessageBoxPage* msgBox = new MessageBoxPage(buffer, false, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
	}

	SetCurrentDirectoryW(pathBuffer);
}

void WindowsSetup::InstallDrivers()
{
	logger->Write(PANTHER_LL_DETAILED, L"Installing drivers onto the system...");

	wchar_t commandBuffer[MAX_PATH + 25];
	wchar_t pathBuffer[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, pathBuffer);
	GetModuleFileNameW(NULL, commandBuffer, MAX_PATH + 25);
	PathCchRemoveFileSpec(commandBuffer, MAX_PATH + 25);
	SetCurrentDirectoryW(commandBuffer);

	WIN32_FIND_DATAW ffd;
	DWORD dwAttrib = GetFileAttributesW(L".\\drivers");

	if (!(dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)))
	{
		SetCurrentDirectoryW(pathBuffer);
		return;
	}

	HANDLE hFind = FindFirstFileW(L".\\drivers\\*", &ffd);
	int lastError = GetLastError();

	wchar_t buffer[MAX_PATH * 2];
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
			if (dot && !wcscmp(dot, L".inf") && !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				swprintf_s(buffer, MAX_PATH * 2, L"Installing driver %s...", ffd.cFileName);
				logger->Write(PANTHER_LL_DETAILED, buffer);

				swprintf_s(commandBuffer, MAX_PATH + 25, L"dism /image:\"%s\\\" /add-driver /driver:\".\\drivers\\%s\"", SystemPartition.mountPoint, ffd.cFileName);
				logger->Write(PANTHER_LL_NORMAL, commandBuffer);
				int ret = _wsystem(commandBuffer);
				if (ret)
				{
					swprintf_s(buffer, MAX_PATH * 2, L"An error occurred while installing driver %s (0x%08x). Installation will continue, but a device required for booting might not be available.", ffd.cFileName, ret);
					logger->Write(PANTHER_LL_NORMAL, buffer);
					MessageBoxPage* msgBox = new MessageBoxPage(buffer, false, currentPage);
					msgBox->ShowDialog();
					delete msgBox;
					continue;
				}
			}

		} while (FindNextFileW(hFind, &ffd) != 0);
		FindClose(hFind);
	}
	else
	{
		logger->Write(PANTHER_LL_NORMAL, L"Could not enumerate Windows installation drivers. Installation will continue, but a device required for booting might not be available.");
		MessageBoxPage* msgBox = new MessageBoxPage(L"An error occurred while enumerating available drivers. Installation will continue, but a device required for booting might not be available.", false, currentPage);
		msgBox->ShowDialog();
		delete msgBox;
	}

	SetCurrentDirectoryW(pathBuffer);
}

int WindowsSetup::RunSetup()
{
	MSG msg;

#ifdef DEBUG
	AllocConsole();
#endif

	// Start logger immediately
	logger = new LibPanther::Logger(L"panther2k.log", PANTHER_LL_VERBOSE);
	logger->Write(PANTHER_LL_BASIC, L"Starting Panther2K version " PANTHER_VERSION "...");

	// Initialize GDI+.
	logger->Write(PANTHER_LL_DETAILED, L"Initializing GDI+...");
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;
	if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Gdiplus::Ok) 
	{
		logger->Write(PANTHER_LL_BASIC, L"GDI Initialization failed.");
		MessageBoxW(NULL, L"Failed to initialize GDI+. Panther2K can not load.", L"Panther2K Early Init", MB_OK | MB_ICONERROR);
		return false;
	}

	// Load font
	logger->Write(PANTHER_LL_DETAILED, L"Loading font resources...");
	HRSRC res = FindResourceW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDR_FONT_IBM), RT_RCDATA);
	HGLOBAL mem = LoadResource(GetModuleHandleW(NULL), res);
	void* data = LockResource(mem);
	size_t len = SizeofResource(GetModuleHandleW(NULL), res);
	DWORD nFonts = 0;
	HANDLE hFontRes = AddFontMemResourceEx(data, len, NULL, &nFonts);
	if (hFontRes == 0)
	{
		logger->Write(PANTHER_LL_BASIC, L"Failed to load font.");
		MessageBoxW(NULL, L"Failed to load font. Panther2K can not load.", L"Panther2K Early Init", MB_OK | MB_ICONERROR);
		return false;
	}
	
	// Create console
	//AllocConsole();
	bool win32 = false;
	if (win32) 
	{
		logger->Write(PANTHER_LL_DETAILED, L"Creating Win32 console...");
		console = new Win32Console();
		console->Init();
	}
	else 
	{
		logger->Write(PANTHER_LL_DETAILED, L"Creating custom console...");
		console = new CustomConsole();
		console->Init();
		ShowWindow(((CustomConsole*)console)->WindowHandle, SW_SHOW);
		SendMessage(((CustomConsole*)console)->WindowHandle, WM_KEYDOWN, VK_F11, 0);
	}

	// Start loading the setup
	Page* loadPage = new Page();
	LoadPage(loadPage);
	loadPage->text = L"Panther2K version " PANTHER_VERSION;

	KeyHandler(VK_F7);
	KeyHandler(VK_NUMPAD0);

	// Load PE drivers
	IsWinPE = true;
	if (IsWinPE) 
	{
		logger->Write(PANTHER_LL_DETAILED, L"Loading WinPE drivers...");
		loadPage->statusText = L"  Windows is loading drivers...";
		loadPage->Draw();
		LoadDrivers();
	}

	// Load config
	logger->Write(PANTHER_LL_DETAILED, L"Loading configuration...");
	loadPage->statusText = L"  Windows is starting Panther2K...";
	loadPage->Draw();
	DoEvents();
	if (!LoadConfig())
	{
		logger->Write(PANTHER_LL_BASIC, L"Failed to load config file...");
		return ERROR_INVALID_PARAMETER;
	}

	// TEMPORARY: code page 437 is unsupported currently
	// WinPE is unsupported currently
	UseCp437 = false;
	SkipPhase3 = false;

	logger->Write(PANTHER_LL_NORMAL, L"Finished initialization.");
	
	// Start welcome phase
	LoadPhase(1);

	// Run console loop
	for (KEY_EVENT_RECORD* record = console->Read();
		!record->bKeyDown || KeyHandler(record->wVirtualKeyCode);
		record = console->Read()) {
		free(record);
		if (exitRequested)
			break;
	}

	// Stop GDI+
	GdiplusShutdown(gdiplusToken);
	return 0;
}

void WindowsSetup::LoadPhase(int phase)
{
	wchar_t logBuffer[MAX_PATH * 2];
	swprintf_s(logBuffer, L"Starting phase %d...", phase);
	logger->Write(PANTHER_LL_DETAILED, logBuffer);

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
				logger->Write(PANTHER_LL_NORMAL, L"Reading WIM information...");
				GetWimImageCount();
				EnumerateImageInfo();
				swprintf_s(logBuffer, L"The WIM file has %d images.", WimImageCount);
				logger->Write(PANTHER_LL_VERBOSE, logBuffer);
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
			logger->Write(PANTHER_LL_NORMAL, L"Reading WIM information...");
			GetWimImageCount();
			EnumerateImageInfo();
			swprintf_s(logBuffer, L"The WIM file has %d images.", WimImageCount);
			logger->Write(PANTHER_LL_VERBOSE, logBuffer);
		}
	case 3:
		if (!SkipPhase3)
		{
			page = new BootMethodSelectionPage();
			break;
		}
	case 4:
		page = new DiskSelectionPage();
		break;
		// TODO: Find out what partitions the user wants to install to
		//if (!SkipPhase4_1)
		//{
	case 5:
	{
		wchar_t messageBuffer[MAX_PATH];
		wchar_t letterBuffer[2];
		const wchar_t* mountPoint;
		wcscpy_s(letterBuffer, L"0");
		logger->Write(PANTHER_LL_NORMAL, L"Mounting target partitions...");

		// Mount all target partitions
		if (lstrlenW(SystemPartition.mountPoint) == 0)
		{
			logger->Write(PANTHER_LL_DETAILED, L"Mounting system partition...");
			letterBuffer[0] = GetFirstFreeDrive();
			if (IsWinPE && letterBuffer[0] == '0')
			{
				wlogf(logger, PANTHER_LL_BASIC, MAX_PATH, L"Failed: no drive letters available, requesting exit");
				MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to mount partition. There are no drive letters available. Panther2K will exit.", false, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				RequestExit();
				return;
			}
			mountPoint = !IsWinPE ? L"$Panther2K\\System\\" : letterBuffer;
			HRESULT hRes = WinPartedDll::MountPartition(console, logger, SystemPartition.diskNumber, SystemPartition.partOffset, mountPoint);
			if (FAILED(hRes))
			{
				wlogf(logger, PANTHER_LL_BASIC, MAX_PATH, L"Mounting partition failed (0x%08X), requesting exit", hRes);
				swprintf_s(messageBuffer, L"Failed to mount partition (0x%08X). Panther2K will exit.", hRes);
				MessageBoxPage* msgBox = new MessageBoxPage(messageBuffer, false, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				RequestExit();
				return;
			}
			wcscpy_s(SystemPartition.mountPoint, mountPoint);
			if (IsWinPE) wcscat_s(SystemPartition.mountPoint, L":\\");
		}
		if (UseRecovery && lstrlenW(RecoveryPartition.mountPoint) == 0)
		{
			logger->Write(PANTHER_LL_DETAILED, L"Mounting recovery partition...");
			letterBuffer[0] = GetFirstFreeDrive();
			if (IsWinPE && letterBuffer[0] == '0')
			{
				wlogf(logger, PANTHER_LL_BASIC, MAX_PATH, L"Failed: no drive letters available, requesting exit");
				MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to mount partition. There are no drive letters available. Panther2K will exit.", false, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				RequestExit();
				return;
			}
			mountPoint = !IsWinPE ? L"$Panther2K\\Recovery\\" : letterBuffer;
			HRESULT hRes = WinPartedDll::MountPartition(console, logger, RecoveryPartition.diskNumber, RecoveryPartition.partOffset, mountPoint);
			if (FAILED(hRes)) 
			{
				wlogf(logger, PANTHER_LL_BASIC, MAX_PATH, L"Mounting partition failed (0x%08X), requesting exit", hRes);
				swprintf_s(messageBuffer, L"Failed to mount partition (0x%08X). Panther2K will exit.", hRes);
				MessageBoxPage* msgBox = new MessageBoxPage(messageBuffer, false, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				RequestExit();
				return;
			}
			wcscpy_s(RecoveryPartition.mountPoint, mountPoint);
			if (IsWinPE) wcscat_s(RecoveryPartition.mountPoint, L":\\");
		}
		if (lstrlenW(BootPartition.mountPoint) == 0)
		{
			logger->Write(PANTHER_LL_DETAILED, L"Mounting boot partition...");
			letterBuffer[0] = GetFirstFreeDrive();
			if (letterBuffer[0] == '0')
			{
				wlogf(logger, PANTHER_LL_BASIC, MAX_PATH, L"Failed: no drive letters available, requesting exit");
				MessageBoxPage* msgBox = new MessageBoxPage(L"Failed to mount partition. There are no drive letters available. Panther2K will exit.", false, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				RequestExit();
				return;
			}
			mountPoint = letterBuffer;
			HRESULT hRes = WinPartedDll::MountPartition(console, logger, BootPartition.diskNumber, BootPartition.partOffset, mountPoint);
			if (FAILED(hRes))
			{
				wlogf(logger, PANTHER_LL_BASIC, MAX_PATH, L"Mounting partition failed (0x%08X), requesting exit", hRes);
				swprintf_s(messageBuffer, L"Failed to mount partition (0x%08X). Panther2K will exit.", hRes);
				MessageBoxPage* msgBox = new MessageBoxPage(messageBuffer, false, currentPage);
				msgBox->ShowDialog();
				delete msgBox;
				RequestExit();
				return;
			}
			wcscpy_s(BootPartition.mountPoint, mountPoint);
			if (IsWinPE) wcscat_s(BootPartition.mountPoint, L":\\");
		}
	}

		// Apply the image
		page = new WimApplyPage();
		LoadPage(page);
		((WimApplyPage*)page)->ApplyImage();
		if (exitRequested) return;

		// Install drivers
		page = new Page();
		LoadPage(page);
		page->text = L"Panther2K is installing drivers...";
		page->Draw();
		InstallDrivers();
	case 6:
		// Generate boot manager files and BCD
		page = new BootPreparationPage();
		LoadPage(page);
		((BootPreparationPage*)page)->PrepareBootFilesNew();
		if (exitRequested) return;
		// Doing this is going to produce the following call stack:
		//   
		//   WindowsSetup::RunSetup()
		//	 WindowsSetup::KeyHandler(VK_RETURN)
		//   PartitionSelectionPage::HandleKey(VK_RETURN)
		//   WindowsSetup::SelectNextPartition(3)
		//   WindowsSetup::LoadPhase(5)
		//   (fall through to case label 6)
		//   BootPreparationPage::PrepareBootFiles()
		// 
		// Only when the creation of the boot files finishes will the execution return to the normal
		// loop. If you are reading this, I need help figuring out how to keep the stack restricted
		// to a single page instead of this misery.
		//  - Leet
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

bool WindowsSetup::SelectPartitionsWithDisk(int diskNumber)
{	
	// Has to be on the heap, otherwise the partition manager can't read it
	wchar_t** mountPoints = (wchar_t**)safeMalloc(logger, sizeof(wchar_t*) * 3);
	for (int i = 0; i < 3; i++)
		mountPoints[i] = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * MAX_PATH);

	wchar_t** volumeList = (wchar_t**)safeMalloc(logger, sizeof(wchar_t*) * 3);
	for (int i = 0; i < 3; i++)
		volumeList[i] = (wchar_t*)safeMalloc(logger, sizeof(wchar_t) * MAX_PATH);

	HRESULT res = (UseLegacy ? WinPartedDll::ApplyP2KLayoutToDiskMBR : WinPartedDll::ApplyP2KLayoutToDiskGPT)
		(console, logger, diskNumber, IsWinPE, &mountPoints, &volumeList);

	if (res != ERROR_SUCCESS)
	{
		wchar_t displayMessage[MAX_PATH * 2];
		wchar_t errorMessage[MAX_PATH];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, res, NULL, errorMessage, MAX_PATH, NULL);
		swprintf_s(displayMessage, L"An error occurred while preparing the disk. %s", errorMessage);

		MessageBoxPage* msgBox = new MessageBoxPage(displayMessage, false, currentPage);
		msgBox->ShowDialog();
		delete msgBox;

		currentPage->Draw();
		return false;
	}
	else 
	{
		// System
		ZeroMemory(&BootPartition, sizeof(VOLUME_INFO));
		if (!GetVolumeInfoFromName(volumeList[0], &BootPartition))
			return false;

		// Recovery
		ZeroMemory(&RecoveryPartition, sizeof(VOLUME_INFO));
		if (!GetVolumeInfoFromName(volumeList[2], &RecoveryPartition))
			return false;

		// Windows
		ZeroMemory(&SystemPartition, sizeof(VOLUME_INFO));
		if (!GetVolumeInfoFromName(volumeList[1], &SystemPartition))
			return false;

		currentPage->Draw();
		return true;
	}
}

void WindowsSetup::SelectPartition(int stringIndex, VOLUME_INFO volume)
{
	switch (stringIndex) 
	{
	case 0:
		SystemPartition = volume;
		break;
	case 1:
	case 2:
		BootPartition = volume;
		break;
	case 3:
		RecoveryPartition = volume;
		break;
	default:
		return;
	}
}

void WindowsSetup::SelectNextPartition(int index)
{
	PartitionSelectionPage* page;
	switch (index)
	{
	case -1:
		LoadPhase(3);
		return;
	case 0:
		if (!SkipPhase4_3)
		{
			// Windows files (Phase 4_3)
			// NTFS partition at least as big as the size of the WIM + the installed size
			// For now it is set to at least 500MB
			page = new PartitionSelectionPage(L"NTFS", 500000000, 0, 0, 0);
			break;
		}
	case 1:
		if (!SkipPhase4_1)
		{
			// ESP or System Reserved (Phase 4_1)
			// FAT32 partition with 40MB free space for ESP
			// NTFS partition with 40MB free space for System Reserved
			page = new PartitionSelectionPage(L"FAT32", 0, 40000000,UseLegacy ? 2 : 1, 1);
			break;
		}
	case 2:
		if (!SkipPhase4_2 && UseRecovery && !UseLegacy)
		{
			// Recovery (Phase 4_2)
			// 500MB NTFS partition 
			page = new PartitionSelectionPage(L"NTFS", 500000000, 0, 3, 2);
			break;
		}
	case 3:
		LoadPhase(5);
		return;
	default:
		return;
	}

	LoadPage(page);
	page->EnumeratePartitions();
}

bool waitingForKey = false;
const wchar_t* oldStatus = 0;
Page* oldPage = 0;
bool WindowsSetup::KeyHandler(WPARAM wParam)
{
	if (waitingForKey)
	{
		switch (wParam)
		{
		case VK_NUMPAD0:
		{
			COLOR* colorTable = (COLOR*)safeMalloc(logger, sizeof(COLOR) * 6);
			colorTable[BackgroundColor] = ConfigBackgroundColor;
			colorTable[ForegroundColor] = ConfigForegroundColor;
			colorTable[ErrorColor] = ConfigErrorColor;
			colorTable[ProgressBarColor] = ConfigProgressBarColor;
			colorTable[LightForegroundColor] = ConfigLightForegroundColor;
			colorTable[DarkForegroundColor] = ConfigDarkForegroundColor;
			console->SetColorTable(colorTable, 6);
			break;
		}
		case VK_NUMPAD1:
		{
			COLOR* colorTable = (COLOR*)safeMalloc(logger, sizeof(COLOR) * 6);
			colorTable[BackgroundColor] = COLOR{ 0, 0, 170 };
			colorTable[ForegroundColor] = COLOR{ 170, 170, 170 };
			colorTable[ErrorColor] = COLOR{ 170, 0, 0 };
			colorTable[ProgressBarColor] = COLOR{ 255, 255, 0 };
			colorTable[LightForegroundColor] = COLOR{ 255, 255, 255 };
			colorTable[DarkForegroundColor] = COLOR{ 0, 0, 0 };
			console->SetColorTable(colorTable, 6);
			break;
		}
		case VK_NUMPAD2:
		{
			COLOR* colorTable = (COLOR*)safeMalloc(logger, sizeof(COLOR) * 6);
			colorTable[BackgroundColor] = COLOR{ 0, 0, 168 };
			colorTable[ForegroundColor] = COLOR{ 168, 168, 168 };
			colorTable[ErrorColor] = COLOR{ 168, 0, 0 };
			colorTable[ProgressBarColor] = COLOR{ 255, 255, 0 };
			colorTable[LightForegroundColor] = COLOR{ 255, 255, 255 };
			colorTable[DarkForegroundColor] = COLOR{ 0, 0, 0 };
			console->SetColorTable(colorTable, 6);
			break;
		}
		case VK_NUMPAD3:
		{
			COLOR* colorTable = (COLOR*)safeMalloc(logger, sizeof(COLOR) * 6);
			colorTable[BackgroundColor] = COLOR{ 0, 0, 0 };
			colorTable[ForegroundColor] = COLOR{ 170, 170, 170 };
			colorTable[ErrorColor] = COLOR{ 170, 0, 0 };
			colorTable[ProgressBarColor] = COLOR{ 255, 255, 0 };
			colorTable[LightForegroundColor] = COLOR{ 255, 255, 255 };
			colorTable[DarkForegroundColor] = COLOR{ 0, 0, 0 };
			console->SetColorTable(colorTable, 6);
			break; 
		}
		}
		if (oldPage == currentPage)
		{
			currentPage->statusText = oldStatus;
			currentPage->Draw();
		}
		waitingForKey = false;
		return true;
	}
	else if (wParam == VK_F7)
	{
		oldPage = currentPage;
		oldStatus = currentPage->statusText;
		currentPage->statusText = L"  NUM0=Default  NUM1=BIOS (Blue)  NUM2=Windows Setup  NUM3=BIOS (Dark)";
		currentPage->Draw();
		waitingForKey = true;
		return true;
	}
	else
		return currentPage->HandleKey(wParam);
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

void WindowsSetup::RequestExit()
{
	exitRequested = true;
}

int EndsWith(const wchar_t* str, const wchar_t* suffix)
{
	if (!str || !suffix)
		return 0;
	size_t lenstr = lstrlenW(str);
	size_t lensuffix = lstrlenW(suffix);
	if (lensuffix > lenstr)
		return 0;
	return _wcsnicmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool WindowsSetup::LoadWimFile()
{
	wchar_t path[MAX_PATH];
	unsigned int flags = EndsWith(WimFile, L".esd") ? WIM_FLAG_SOLIDCOMPRESSION : 0;
	wsprintfW(path, L"Loading file '%s' (%s)...", WimFile, flags ? L"solid" : L"not solid");
	logger->Write(PANTHER_LL_DETAILED, path);
	for (int compression = WIM_COMPRESS_NONE; compression <= WIM_COMPRESS_LZMS; compression++) 
	{
		WimHandle = WIMCreateFile(WimFile, WIM_GENERIC_READ, WIM_OPEN_EXISTING, flags, compression, NULL);
		if (WimHandle) break;
	}
	if (!WimHandle)
	{
		wsprintfW(path, L"WimCreateFile failed: (0x%08x).", MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, GetLastError()));
		logger->Write(PANTHER_LL_BASIC, path);
		return false;
	}

	GetTempPathW(MAX_PATH, path);
	WIMSetTemporaryPath(WimHandle, path);
	return true;
}

void WindowsSetup::GetWimImageCount()
{
	if (WimHandle) WimImageCount = WIMGetImageCount(WimHandle);
}

char* getCharPointer(wchar_t* string, int count = -1)
{
	if (count == -1)
		count = lstrlenW(string) + 1;
	char* oldtext = (char*)string;
	char* text = (char*)malloc(sizeof(char) * count);
	if (text == 0)
		return NULL;
	for (int i = 0; i <= count; i++)
	{
		int j = i * 2;
		if (i == count)
			text[i] = 0;
		else
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
	ImageInfo* pointer = (ImageInfo*)safeMalloc(logger, sizeof(ImageInfo) * WimImageCount);
	if (pointer == NULL)
		return;

	WIMGetImageInformation(WimHandle, (PVOID*)&str_ptr, (PDWORD)&str_len);
	str_ptr++;

	char* xml = getCharPointer(str_ptr);
	int count = strlen(xml);
	doc->parse<0>(xml);

	// Parse XML
	rapidxml::xml_node<>* wimNode = doc->first_node();
	rapidxml::xml_node<>* imageNode = wimNode->first_node("IMAGE");
	rapidxml::xml_node<>* creationTimeNode;
	rapidxml::xml_node<>* windowsNode;
	rapidxml::xml_node<>* node;

	for (int i = 1; i <= WimImageCount; i++, imageNode = imageNode->next_sibling("IMAGE"))
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


		node = imageNode->first_node("TOTALBYTES");
		wchar_t* byteCountStr = getWcharPointer(node->value());
		(pointer + i - 1)->TotalSize = wcstoull(creationLow, &end_str, 0);
	}

	delete doc;
	WimImageInfos = pointer;
}

void WindowsSetup::ShowError(const wchar_t* errorMessage, int systemError, int logLevel)
{
	wchar_t displayedMessage[1024];
	wchar_t* systemMessage;
	if (!FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, systemError, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&systemMessage, 128, NULL))
	{
		systemMessage = (wchar_t*)LocalAlloc(LMEM_FIXED, 128 * sizeof(wchar_t));
		if (systemMessage)
			swprintf_s(systemMessage, 128, L"Error code 0x%08X. Failed to retrieve a localized message of the error (0x%08X).", systemError, GetLastError());
		else
			logger->Write(logLevel, L"WARNING: failed to format error code.");
	}
	swprintf_s(displayedMessage, errorMessage, systemMessage);
	LocalFree(systemMessage);

	logger->Write(logLevel, displayedMessage);
	
	MessageBoxPage* msgBox = new MessageBoxPage(displayedMessage, true, currentPage);
	msgBox->ShowDialog();
	delete msgBox;
}

LibPanther::Logger* WindowsSetup::GetLogger()
{
	return logger;
}
