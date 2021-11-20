#include "BootPreparationPage.h"
#include "WindowsSetup.h"
#include <iostream>
#include "shlobj.h"
#include "MessageBoxPage.h"

BootPreparationPage::~BootPreparationPage()
{
	free((wchar_t*)text);
}

void BootPreparationPage::PrepareBootFiles()
{
	while (true)
		DoEvents();

	wchar_t buffer1[MAX_PATH];
	wchar_t buffer2[MAX_PATH];
	wchar_t errorMessage[MAX_PATH];
	wchar_t systemDirectory[MAX_PATH];
	DWORD retval = 0;
	PVOID wow64State = 0;
	SHELLEXECUTEINFOW ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = L"open";
	ShExecInfo.lpFile = L"";
	ShExecInfo.lpParameters = L"";
	ShExecInfo.lpDirectory = /*systemDirectory;*/ NULL;
	ShExecInfo.nShow = SW_HIDE;
	ShExecInfo.hInstApp = NULL;

	/* 
	 * For this phase of the installation, Panther2K cannot use filesystem redirections for 32-bit systems.
	 * This is because Panther2K needs access to bcdboot and reagentc, which are not present in SysWOW64.
	 */
	Wow64DisableWow64FsRedirection(&wow64State);

	GetSystemDirectoryW(systemDirectory, MAX_PATH);
	int strLen = lstrlenW(systemDirectory);
	systemDirectory[strLen] = L'\\';
	systemDirectory[strLen + 1] = L'\0';

	if (WindowsSetup::UseLegacy)
	{
		statusText = L"  Panther2K is creating the Boot Configuration Data...";
		Draw();
		DoEvents();
		swprintf(buffer1, MAX_PATH, L"bcdboot %sWindows /s %s /f BIOS", WindowsSetup::Partition3Mount, WindowsSetup::Partition1Mount);
		ShExecInfo.lpFile = L"cmd";
		ShExecInfo.lpParameters = buffer1;
		ShellExecuteExW(&ShExecInfo);
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		GetExitCodeProcess(ShExecInfo.hProcess, &retval);
		if (retval)
		{
			swprintf(errorMessage, MAX_PATH, L"Failed to create boot files (0x%08x).", retval);
			goto fail;
		}
		statusText = L"  Panther2K is mounting the Windows partition to a letter...";
		Draw();
		DoEvents();
		wchar_t letter = WindowsSetup::GetFirstFreeDrive();
		if (letter == L'0')
		{
			swprintf(errorMessage, MAX_PATH, L"Failed to create boot record. There are no drive letters available.");
			goto fail;
		}
		swprintf(buffer2, MAX_PATH, L"%c:", letter);
		if (!SetVolumeMountPointW(buffer2, WindowsSetup::Partition1Volume))
		{
			swprintf(errorMessage, MAX_PATH, L"Failed to create boot record. Mounting %s to mount point %s failed (0x%08x).", WindowsSetup::Partition1Volume, buffer2, GetLastError());
			goto fail;
		}
		statusText = L"  Panther2K is creating the Master Boot Record...";
		Draw();
		DoEvents();
		swprintf(buffer1, MAX_PATH, L"bootsect /nt60 %s /force /mbr", buffer2);
		ShExecInfo.lpFile = L"cmd";
		ShExecInfo.lpParameters = buffer1;
		ShellExecuteExW(&ShExecInfo);
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		GetExitCodeProcess(ShExecInfo.hProcess, &retval);
		if (retval)
		{
			swprintf(errorMessage, MAX_PATH, L"Failed to create Master Boot REcord (0x%08x).", retval);
			goto fail;
		}
	}
	else
	{
		statusText = L"  Panther2K is populating the EFI System Partition...";
		Draw();
		DoEvents();
		swprintf(buffer1, MAX_PATH, L"/c bcdboot.exe %sWindows /s %s /f UEFI", WindowsSetup::Partition3Mount, WindowsSetup::Partition1Mount);
		ShExecInfo.lpFile = L"cmd";
		ShExecInfo.lpParameters = buffer1;
		ShellExecuteExW(&ShExecInfo);
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		GetExitCodeProcess(ShExecInfo.hProcess, &retval);
		if (retval)
		{
			swprintf(errorMessage, MAX_PATH, L"Failed to create boot files (0x%08x).", retval);
			goto fail;
		}
		statusText = L"  Panther2K is copying the Windows RE files...";
		Draw();
		DoEvents();
		swprintf(buffer1, MAX_PATH, L"%sRecovery\\WindowsRE\\", WindowsSetup::Partition2Mount);
		if (retval = SHCreateDirectoryExW(NULL, buffer1, NULL))
		{
			if (WindowsSetup::ContinueWithoutRecovery)
			{
				swprintf(errorMessage, MAX_PATH, L"Failed to copy recovery image. Could not create the directory (0x%08x). Panther2K will continue without setting up Windows Recovery Environment.", retval);
				MessageBoxPage* msgBox = new MessageBoxPage(errorMessage, false, this);
				msgBox->ShowDialog();
				delete msgBox;
				goto end;
			}

			swprintf(errorMessage, MAX_PATH, L"Failed to copy recovery image. Could not create the directory (0x%08x).", retval);
			goto fail;
		}
		swprintf(buffer1, MAX_PATH, L"%sRecovery\\WindowsRE\\Winre.wim", WindowsSetup::Partition2Mount);
		swprintf(buffer2, MAX_PATH, L"%sWindows\\System32\\Recovery\\Winre.wim", WindowsSetup::Partition3Mount);
		if (!CopyFileW(buffer2, buffer1, FALSE))
		{
			if (WindowsSetup::ContinueWithoutRecovery)
			{
				swprintf(errorMessage, MAX_PATH, L"Failed to copy recovery image (0x%08x). Panther2K will continue without setting up Windows Recovery Environment.", GetLastError());
				MessageBoxPage* msgBox = new MessageBoxPage(errorMessage, false, this);
				msgBox->ShowDialog();
				delete msgBox;
				goto end;
			}

			swprintf(errorMessage, MAX_PATH, L"Failed to copy recovery image (0x%08x).", GetLastError());
			goto fail;
		}
		statusText = L"  Panther2K is enabling Windows RE...";
		Draw();
		DoEvents();
		swprintf(buffer1, MAX_PATH, L"/c reagentc /setreimage /path %sRecovery\\WindowsRE /target %sWindows", WindowsSetup::Partition2Mount, WindowsSetup::Partition3Mount);
		ShExecInfo.lpFile = L"cmd";
		ShExecInfo.lpParameters = buffer1;
		ShellExecuteExW(&ShExecInfo);
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		GetExitCodeProcess(ShExecInfo.hProcess, &retval);
		if (retval)
		{
			swprintf(errorMessage, MAX_PATH, L"Failed to enable Windows RE (0x%08x).", retval);
			goto fail;
		}
	}
end:
	Wow64RevertWow64FsRedirection(wow64State);
	return;
fail:
	MessageBoxPage* msgBox = new MessageBoxPage(errorMessage, true, this);
	msgBox->ShowDialog();
	delete msgBox;
	goto end;
}

void BootPreparationPage::Init()
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
	statusText = L"";
}

void BootPreparationPage::Drawer()
{
	SIZE cSize = console->GetSize();

	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	DrawTextCenter(L"Please wait while Setup prepares Windows", cSize.cx, 6);
	DrawTextCenter(L"Boot Manager to boot Windows on your computer.", cSize.cx, 7);
	DrawTextCenter(L"This shouldn't take more than a minute to complete.", cSize.cx, 8);
}

void BootPreparationPage::Redrawer()
{
}

void BootPreparationPage::KeyHandler(WPARAM wParam)
{
}
