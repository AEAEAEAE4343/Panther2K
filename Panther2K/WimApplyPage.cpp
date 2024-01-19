#include "WimApplyPage.h"
#include "WindowsSetup.h"
#include <iostream>
#include <process.h>
#include "MessageBoxPage.h"

#define WM_PROGRESSUPDATE WM_APP
#define WM_FILENAMEUPDATE WM_APP + 1
#define WM_FINISHUPDATE WM_APP + 2

int hResult = 0;
bool runMessageLoop = true;
bool canSendProgress = true;
bool canSendFileName = true;
unsigned int progress = 0;
LibPanther::Logger* installLog = nullptr;

void WriteToFile(const wchar_t* string)
{
	DWORD bytes;
	if (!installLog)
	{
		wchar_t buffer[MAX_PATH];
		swprintf_s(buffer, L"%s%s", WindowsSetup::Partition3Mount, L"panther2k.log");
		installLog = new LibPanther::Logger(buffer, PANTHER_LL_VERBOSE);

		WriteToFile(L"Starting Panther2K installation log...");
		swprintf_s(buffer, L"   Installing %s to %s.", 
			WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName,
			WindowsSetup::Partition3Volume);
		WriteToFile(buffer);
		swprintf_s(buffer, L"   Total installation size: %llu bytes", WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].TotalSize);
	}

	if (string)
	{
		WindowsSetup::GetLogger()->Write(PANTHER_LL_BASIC, string);
		installLog->Write(PANTHER_LL_BASIC, string);
	}
}

DWORD __stdcall MessageCallback(IN DWORD Msg, IN WPARAM wParam, IN LPARAM lParam, IN PDWORD dwThreadId)
{
	wchar_t buffer[MAX_PATH * 2];
	switch (Msg)
	{
	case WIM_MSG_PROGRESS:
		swprintf_s(buffer, L"Installation progress: %lld%%\n", wParam);
		WriteToFile(buffer);
		PostThreadMessageW(*dwThreadId, WM_PROGRESSUPDATE, wParam, 0);
		break;
	case WIM_MSG_PROCESS:
		if (WindowsSetup::ShowFileNames && canSendFileName)
			if (PostThreadMessageW(*dwThreadId, WM_FILENAMEUPDATE, wParam, lParam))
				canSendFileName = false;
		break;
	case WIM_MSG_INFO:
	{
		wchar_t messageBuffer[MAX_PATH];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lParam, NULL, messageBuffer, MAX_PATH, NULL);
		swprintf_s(buffer, L"System message (File %s): %s", (wchar_t*)wParam, messageBuffer);
		WriteToFile(buffer);
		break;
	}
	case WIM_MSG_ERROR:
	{
		wchar_t messageBuffer[MAX_PATH];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lParam, NULL, messageBuffer, MAX_PATH, NULL);
		swprintf_s(buffer, L"Error (File %s): %s", (wchar_t*)wParam, messageBuffer);
		WriteToFile(buffer);
		break;
	}
	case WIM_MSG_WARNING:
	{
		wchar_t messageBuffer[MAX_PATH];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lParam, NULL, messageBuffer, MAX_PATH, NULL);
		swprintf_s(buffer, L"Warning (File %s): %s", (wchar_t*)wParam, messageBuffer);
		WriteToFile(buffer);
		break;
	}
	case WIM_MSG_TEXT:
		swprintf_s(buffer, L"%s\n", (wchar_t*)lParam);
		WriteToFile(buffer);
		break;
	case WIM_MSG_SETRANGE:
		swprintf_s(buffer, L"   Total number of files to be applied: %lld.", lParam);
		WriteToFile(buffer); 
		WriteToFile(L"Starting Windows installation...");
		break;
	case WIM_MSG_SETPOS:
		swprintf_s(buffer, L"Number of files applied: %lld.", lParam);
		WriteToFile(buffer);
		break;
	case WIM_MSG_QUERY_ABORT:
		WriteToFile(L"Abort opportunity given, but not aborting.");
		break;
	case WIM_MSG_METADATA_EXCLUDE:
	case WIM_MSG_STEPIT:
	case WIM_MSG_STEPNAME:
		break;
	default:
		swprintf_s(buffer, L"Unknown message: %d.", Msg);
		WriteToFile(buffer);
		break;
	}
	return WIM_MSG_SUCCESS;
}

void __stdcall WimApplyThread(PDWORD dwThreadId)
{
	WIMRegisterMessageCallback(WindowsSetup::WimHandle, (FARPROC)MessageCallback, dwThreadId);
	HANDLE him = WIMLoadImage(WindowsSetup::WimHandle, WindowsSetup::WimImageIndex);
	BOOL result = WIMApplyImage(him, WindowsSetup::Partition3Mount, WIM_FLAG_INDEX);
	hResult = GetLastError();
	WIMUnregisterMessageCallback(WindowsSetup::WimHandle, (FARPROC)MessageCallback);
	runMessageLoop = false;
	PostThreadMessageW(*dwThreadId, WM_FINISHUPDATE, 0, 0);
}

void WimApplyPage::WimMessageLoop()
{
	MSG msg;
	int progress = 0;
    wchar_t* filename = 0;
	runMessageLoop = true;

	while (GetMessageW(&msg, nullptr, WM_PROGRESSUPDATE, WM_FINISHUPDATE) && runMessageLoop)
	{
		switch (msg.message)
		{
		case WM_PROGRESSUPDATE:
			Update(msg.wParam);
			break;
		case WM_FILENAMEUPDATE:
			Update((wchar_t*)msg.wParam);
			break;
		case WM_FINISHUPDATE:
			runMessageLoop = false;
			break;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		Sleep(25);
		canSendFileName = true;
	}

	if (hResult)
	{
		MessageBoxPage* msgBox = new MessageBoxPage(L"The installation has failed. See the installation log for more details.", true, this);
		msgBox->ShowDialog();
		delete msgBox;
		exit(hResult);
	}

end:
	return;
}

WimApplyPage::~WimApplyPage()
{
	free((wchar_t*)text);
}

void WimApplyPage::Update(int prog)
{
	progress = prog;
	Redraw();
}

LPTSTR PathFindFileName(
	LPTSTR pPath)
{
	LPTSTR pT;

	for (pT = pPath; *pPath; pPath++) {
		if ((pPath[0] == TEXT('\\') || pPath[0] == TEXT(':') || pPath[0] == TEXT('/'))
			&& pPath[1] && pPath[1] != TEXT('\\') && pPath[1] != TEXT('/'))
			pT = pPath + 1;
	}

	return pT;
}

void WimApplyPage::Update(wchar_t* fileName)
{
	int length = console->GetSize().cx;
	int bufferSize = length + 1;
	int nameX = length - 25;
	/// | Copying: 12345678.123  

	wmemset((wchar_t*)statusText, L' ', bufferSize);
	wmemcpy_s((wchar_t*)statusText, bufferSize, L"  Panther2K is installing Windows...", 36);

	if (fileName)
	{
		wchar_t buffer[24];
		fileName = PathFindFileName(fileName);
		swprintf_s(buffer, L"│ Copying: %.12s", fileName + (lstrlenW(fileName) - 12));
		wmemcpy_s((wchar_t*)statusText + nameX, bufferSize - nameX, buffer, 24);
	}
	else 
	{
		((wchar_t*)statusText)[36] = L'\x0';
	}

	((wchar_t*)statusText)[length] = L'\x0';
	_ASSERT(lstrlenW(statusText) <= length);
	Redraw();
}

void WimApplyPage::ApplyImage()
{
	DWORD bytesCopied;
	wchar_t fileBuffer[1024];
	ULONGLONG ticksBefore = GetTickCount64();

	swprintf_s(fileBuffer, L"Starting installation of %s\n", WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName);
	WriteToFile(fileBuffer);

	DWORD dwThreadId = GetCurrentThreadId();
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WimApplyThread, &dwThreadId, 0, 0);
	WimMessageLoop();

	ULONGLONG ticksAfter = GetTickCount64();
	ULONGLONG ticksSpent = ticksAfter - ticksBefore;
	
	swprintf_s(fileBuffer, L"The installation has finished.\nInstallation time: %02llum:%02llus\nResult: 0x%08X\n", (ticksSpent / 1000) / 60, (ticksSpent / 1000) % 60, hResult);
	WriteToFile(fileBuffer);
}

void WimApplyPage::Init()
{
	wchar_t* displayName = WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName;
	int length = lstrlenW(displayName);
	wchar_t* textBuffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), length * sizeof(wchar_t) + 14);
	memcpy(textBuffer, displayName, length * sizeof(wchar_t));
	memcpy(textBuffer + length, L" Setup", 14);
	text = textBuffer;
	statusText = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), sizeof(wchar_t) * (console->GetSize().cx + 1));
	memcpy(((wchar_t*)statusText), L"  Panther2K is installing Windows...", 37 * sizeof(wchar_t));
}

void WimApplyPage::Drawer()
{
	SIZE consoleSize = console->GetSize();
	
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->DrawTextCenter(L"Please wait while Setup copies files to the Windows installation folders. This might take several minutes to complete.", consoleSize.cx / 3 * 2, 6);
	
	int boxWidth = consoleSize.cx - 12;
	int boxHeight = 7;
	int boxX = 6;
	int boxY = consoleSize.cy / 2 - 1;
	DrawBox(boxX, boxY, boxWidth, boxHeight, true);
	console->SetPosition(boxX + 2, boxY + 1);
	console->Write(L"Setup is copying files...");
	boxX += 6;
	boxY += 3;
	boxWidth -= 12;
	boxHeight -= 4;
	DrawBox(boxX, boxY, boxWidth, boxHeight, false);
}

void WimApplyPage::Redrawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	SIZE consoleSize = console->GetSize();
	int boxWidth = consoleSize.cx - 12;
	int boxX = 6;
	int boxY = consoleSize.cy / 2 - 1;

	wchar_t buffer[5];
	swprintf(buffer, 5, L"%i%%", progress);
	console->SetPosition(boxX + ((boxWidth / 2) - (4 / 2)), boxY + 2);
	console->Write(buffer);

	boxX += 6;
	boxY += 3;
	boxWidth -= 12;

	console->SetPosition(boxX + 1, boxY + 1);
	int progWidth = boxWidth - 2;
	int interpolatedProgress = progress * progWidth / 100;
	console->SetForegroundColor(WindowsSetup::ProgressBarColor);
	for (int i = 0; i < interpolatedProgress; i++)
		console->Write(WindowsSetup::UseCp437 ? L"\xDB" : L"█");
	for (int i = interpolatedProgress; i < progWidth; i++)
		console->Write(L" ");
}

bool WimApplyPage::KeyHandler(WPARAM wParam)
{
	return true;
}
