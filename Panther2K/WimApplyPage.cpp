#include "WimApplyPage.h"
#include "WindowsSetup.h"
#include <iostream>
#include <process.h>
#include "MessageBoxPage.h"

#define WM_PROGRESSUPDATE WM_APP
#define WM_FILENAMEUPDATE WM_APP + 1

int hResult = 0;
bool runMessageLoop = true;
bool canSendProgress = true;
bool canSendFileName = true;
unsigned int progress = 0;
DWORD __stdcall MessageCallback(IN DWORD Msg, IN WPARAM wParam, IN LPARAM lParam, IN PDWORD dwThreadId)
{
	switch (Msg)
	{
	case WIM_MSG_PROGRESS:
		if (canSendProgress)
			if (PostThreadMessageW(*dwThreadId, WM_PROGRESSUPDATE, wParam, 0))
				canSendProgress = false;
		break;
	case WIM_MSG_PROCESS:
		if (WindowsSetup::ShowFileNames && canSendFileName)
			if (PostThreadMessageW(*dwThreadId, WM_FILENAMEUPDATE, wParam, 0))
				canSendFileName = false;
		break;
	}
	return WIM_MSG_SUCCESS;
}

void __stdcall WimApplyThread(PDWORD dwThreadId)
{
	WIMRegisterMessageCallback(WindowsSetup::WimHandle, (FARPROC)MessageCallback, dwThreadId);
	HANDLE him = WIMLoadImage(WindowsSetup::WimHandle, WindowsSetup::WimImageIndex);
	WIMApplyImage(him, WindowsSetup::Partition3Mount, WindowsSetup::ShowFileNames ? WIM_FLAG_FILEINFO : 0);
	hResult = GetLastError();
	WIMUnregisterMessageCallback(WindowsSetup::WimHandle, (FARPROC)MessageCallback);
	runMessageLoop = false;
}

void WimApplyPage::WimMessageLoop()
{
	MSG msg;
	int progress = 0;
    wchar_t* filename = 0;
	runMessageLoop = true;
	while (runMessageLoop) 
	{
		WaitMessage();
		progress = 0;
		filename = 0;
		while (PeekMessageW(&msg, nullptr, WM_PROGRESSUPDATE, WM_FILENAMEUPDATE, true))
		{
			switch (msg.message)
			{
			case WM_PROGRESSUPDATE:
				progress = msg.wParam;
				break;
			case WM_FILENAMEUPDATE:
				filename = (wchar_t*)msg.wParam;
				break;
			}
		}
		if (progress)
			Update(progress);
		if (filename)
			Update(filename);

		canSendProgress = true;
		canSendFileName = true;

		while (PeekMessageW(&msg, nullptr, 0, WM_APP - 1, true))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		Sleep(25);
	}

	if (hResult)
	{
		MessageBoxPage* msgBox = new MessageBoxPage(L"The installation has failed. WIMApplyImage returned (0x%08x).", true, this);
		msgBox->ShowDialog();
		delete msgBox;
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

void WimApplyPage::Update(wchar_t* fileName)
{
	if (fileName)
	{
		int length = console->GetSize().cx;
		int bufferSize = length + 1;
		int nameX = length - (12 + WindowsSetup::FileNameLength);

		if (!filename)
		{
			wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * bufferSize);
			if (!buffer)
				return; 
			statusText = buffer;
		}
		
		filename = fileName;

		swprintf((wchar_t*)statusText, bufferSize, L"  Panther2K is installing Windows...%*s%cCopying: %s", nameX - 36, L"", WindowsSetup::UseCp437 ? L'\xB3' : L'│', filename + (lstrlenW(filename) - WindowsSetup::FileNameLength));
		Redraw();
	}
}

void WimApplyPage::ApplyImage()
{
	DWORD bytesCopied;
	wchar_t fileBuffer[1024];
	ULONGLONG ticksBefore = GetTickCount64();

	DWORD dwThreadId = GetCurrentThreadId();
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WimApplyThread, &dwThreadId, 0, 0);
	//_beginthreadex(NULL, NULL, (_beginthreadex_proc_type)WimApplyThread, &dwThreadId, NULL, NULL);
	WimMessageLoop();

	ULONGLONG ticksAfter = GetTickCount64();
	ULONGLONG ticksSpent = ticksAfter - ticksBefore;
#ifdef _DEBUG
	HANDLE hFile = CreateFileW(L"installstats.txt", GENERIC_ALL, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	swprintf(fileBuffer, 1024, L"Installed OS: %s\n", WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName);
	WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

	swprintf(fileBuffer, 1024, L"Installation time: %02llum:%02llus\n", (ticksSpent / 1000) / 60, (ticksSpent / 1000) % 60);
	WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);

	swprintf(fileBuffer, 1024, L"Result: 0x%08X\n", hResult);
	WriteFile(hFile, fileBuffer, lstrlenW(fileBuffer) * 2, &bytesCopied, NULL);
#endif
}

void WimApplyPage::Init()
{
	wchar_t* displayName = WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName;
	int length = lstrlenW(displayName);
	wchar_t* textBuffer = (wchar_t*)malloc(length * sizeof(wchar_t) + 14);
	if (textBuffer)
	{
		memcpy(textBuffer, displayName, length * sizeof(wchar_t));
		memcpy(textBuffer + length, L" Setup", 14);
		text = textBuffer;
	}
	else
	{
		text = L"Panther2K Setup";
	}
	statusText = L"  Panther2K is installing Windows...";
}

void WimApplyPage::Drawer()
{
	SIZE consoleSize = console->GetSize();
	
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->SetPosition((consoleSize.cx / 2) - 18, 6);
	console->Write(L"Please wait while Setup copies files");

	console->SetPosition((consoleSize.cx / 2) - 18, 7);
	console->Write(L"to the Windows installation folders.");

	console->SetPosition((consoleSize.cx / 2) - 22, 8);
	console->Write(L"This might take several minutes to complete.");
	
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

void WimApplyPage::KeyHandler(WPARAM wParam)
{
}
