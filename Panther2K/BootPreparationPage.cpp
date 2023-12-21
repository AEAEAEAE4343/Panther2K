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

		if (WindowsSetup::UseRecovery)
		{
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
				if (WindowsSetup::ContinueWithoutRecovery)
				{
					swprintf(errorMessage, MAX_PATH, L"Failed to enable Windows RE (0x%08x). Panther2K will continue without setting up Windows Recovery Environment.", retval);
					MessageBoxPage* msgBox = new MessageBoxPage(errorMessage, false, this);
					msgBox->ShowDialog();
					delete msgBox;
					goto end;
				}

				swprintf(errorMessage, MAX_PATH, L"Failed to enable Windows RE (0x%08x).", retval);
				goto fail;
			}
		}
	}
end:
	Wow64RevertWow64FsRedirection(wow64State);
	return;
fail:
	wprintf(errorMessage);
	MessageBoxPage* msgBox = new MessageBoxPage(errorMessage, true, this);
	msgBox->ShowDialog();
	delete msgBox;
	goto end;
}

void BootPreparationPage::PrepareBootFilesNew()
{
	wchar_t pathBuffers[2][MAX_PATH];
	
	/*
	Legacy:
	Copy W:\Windows\Boot\PCAT\ to S:\Boot\
	Rename S:\Boot\bootmgr to S:\bootmgr
	Create BCD store at S:\Boot\BCD
	Add Windows Boot Manager to the BCD store

	UEFI:
	Copy W:\Windows\Boot\EFI\ to S:\EFI\Microsoft\Boot\
	Copy S:\EFI\Microsoft\Boot\bootmgfw.efi to S:\EFI\Boot\bootx64.efi
	Create BCD store at S:\EFI\Microsoft\Boot\BCD
	Add Windows Boot Manager to the BCD store

	S:\ = WindowsSetup::Partition1Mount
	R:\ = WindowsSetup::Partition2Mount
	W:\ = WindowsSetup::Partition3Mount
	*/

	/*
	* Copy boot files
	*/

	SHFILEOPSTRUCTW fos = { 0 };
	fos.hwnd = GetDesktopWindow();
	fos.wFunc = FO_COPY;
	fos.pFrom = pathBuffers[0];
	fos.pTo = pathBuffers[1];

	swprintf_s(pathBuffers[0], WindowsSetup::UseLegacy ? L"%sWindows\\Boot\\PCAT\\*"
													   : L"%sWindows\\Boot\\EFI\\*", WindowsSetup::Partition3Mount);
	swprintf_s(pathBuffers[1], WindowsSetup::UseLegacy ? L"%sBoot\\"
													   : L"%sEFI\\Microsoft\\Boot\\", WindowsSetup::Partition1Mount);
	
	pathBuffers[0][lstrlenW(pathBuffers[0]) + 1] = 0;
	pathBuffers[1][lstrlenW(pathBuffers[1]) + 1] = 0;
	if (SHCreateDirectory(NULL, pathBuffers[1]) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		// Failed
		int temp = GetLastError();
		return;
	}
	if (int temp = SHFileOperationW(&fos))
	{
		// Failed
		return;
	}

	/*
	* Move Windows Boot Manager to the correct location
	*/

	if (!WindowsSetup::UseLegacy)
	{
		swprintf_s(pathBuffers[1], L"%sEFI\\Boot", WindowsSetup::Partition1Mount);
		if (SHCreateDirectory(NULL, pathBuffers[1]) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			// Failed
			int temp = GetLastError();
			return;
		}
	}

	fos.wFunc = WindowsSetup::UseLegacy ? FO_MOVE : FO_COPY;
	swprintf_s(pathBuffers[0], WindowsSetup::UseLegacy ? L"%sBoot\\bootmgr"
													   : L"%sEFI\\Microsoft\\Boot\\bootmgfw.efi", WindowsSetup::Partition1Mount);
	swprintf_s(pathBuffers[1], WindowsSetup::UseLegacy ? L"%s"
													   : L"%sEFI\\Boot\\bootx64.efi", WindowsSetup::Partition1Mount);

	pathBuffers[0][lstrlenW(pathBuffers[0]) + 1] = 0;
	pathBuffers[1][lstrlenW(pathBuffers[1]) + 1] = 0;
	if (int temp = SHFileOperationW(&fos))
	{
		// Failed
		return;
	}

	/*
	* Check if BCD store already exists
	*/
	wchar_t commandBuffer[MAX_PATH + 128];
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"%sBoot\\BCD"
													  : L"%sEFI\\Microsoft\\Boot\\BCD", WindowsSetup::Partition1Mount);
	DWORD dwAttrib = GetFileAttributes(commandBuffer);

	if (dwAttrib == INVALID_FILE_ATTRIBUTES ||
		dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
	{
		/*
		* Create BCD store
		*/
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /createstore %sBoot\\BCD"
														  : L"bcdedit /createstore %sEFI\\Microsoft\\Boot\\BCD", WindowsSetup::Partition1Mount);
		if (_wsystem(commandBuffer))
		{
			// Failed
			return;
		}

		/*
		* Add Windows Boot Manager entry
		*/
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /create {bootmgr}"
														  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /create {bootmgr}", WindowsSetup::Partition1Mount);
		if (_wsystem(commandBuffer))
		{
			// Failed
			return;
		}

		/*
		* Configure Windows Boot Manager entry
		*/
		// device = s:
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {bootmgr} device partition=%s"
														  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {bootmgr} device partition=%s", WindowsSetup::Partition1Mount, WindowsSetup::Partition1Mount);
		if (_wsystem(commandBuffer))
		{
			// Failed
			return;
		}
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {bootmgr} path \\bootmgr"
														  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {bootmgr} path \\EFI\\Microsoft\\Boot\\bootmgfw.efi", WindowsSetup::Partition1Mount);
		if (_wsystem(commandBuffer))
		{
			// Failed
			return;
		}
	}

	/*
	* Add Windows Boot Loader entry
	*/

	// Create pipe for redirecting STDOUT
	HANDLE stdOutRd;
	HANDLE stdOutWr;
	SECURITY_ATTRIBUTES attributes = { 0 };
	attributes.bInheritHandle = TRUE;
	attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	if (!CreatePipe(&stdOutRd, &stdOutWr, &attributes, 0)) 
	{
		// Failed
		int temp = GetLastError();
		return;
	}

	// Call bcdedit while redirecting stdout
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /create /application osloader"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /create /application osloader", WindowsSetup::Partition1Mount);
	STARTUPINFOW si = { 0 };
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = stdOutWr;

	PROCESS_INFORMATION pi = { 0 };
	if (!CreateProcessW(NULL, commandBuffer, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi)) 
	{
		// Failed
		int temp = GetLastError();
		return;
	}

	// Close all handles except stdOutRd
	// We know bcdedit is done when stdOutRd receives an EOF
	CloseHandle(stdOutWr);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	wchar_t guidString[128];
	char guidBuffer[128];
	DWORD bytesRead;
	BOOL retCode;

	// Read until the end of the pipe is reached or when the buffer is full
	for (char* buffer = guidBuffer; (buffer - guidBuffer < 128) && (retCode = ReadFile(stdOutRd, buffer, 1, &bytesRead, NULL)); buffer += bytesRead);
	MultiByteToWideChar(GetConsoleCP(), MB_PRECOMPOSED, guidBuffer, 128, guidString, 128);

	// If the end of the pipe was not reached, read it to the end
	while (retCode) retCode = ReadFile(stdOutRd, guidBuffer, 1, &bytesRead, NULL);
	if (GetLastError() != ERROR_BROKEN_PIPE)
	{
		// Failed
		int temp = GetLastError();
		return;
	}

	// Find GUID of created entry
	guidString[127] = 0;
	wchar_t* guid = wcschr(guidString, L'{');
	if (guid == 0)
	{
		// Failed - no guid
		return;
	}
	guid[38] = 0;

	/*
	* Configure Windows Boot Loader entry
	*/
	// Set it to default
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /default %s"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /default %s", WindowsSetup::Partition1Mount, guid);
	if (_wsystem(commandBuffer))
	{
		// Failed
		return;
	}
	// osdevice = W:
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {default} osdevice partition=%s"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {default} osdevice partition=%s", WindowsSetup::Partition1Mount, WindowsSetup::Partition3Mount);
	if (_wsystem(commandBuffer))
	{
		// Failed
		return;
	}
	// device = W:
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {default} device partition=%s"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {default} device partition=%s", WindowsSetup::Partition1Mount, WindowsSetup::Partition3Mount);
	if (_wsystem(commandBuffer))
	{
		// Failed
		return;
	}
	// systemroot = \Windows
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {default} systemroot \\Windows"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {default} systemroot \\Windows", WindowsSetup::Partition1Mount);
	if (_wsystem(commandBuffer))
	{
		// Failed
		return;
	}
	// path = \Windows\System32\winload.exe / .efi
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {default} path \\Windows\\System32\\winload.exe"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {default} path \\Windows\\System32\\winload.efi", WindowsSetup::Partition1Mount);
	if (_wsystem(commandBuffer))
	{
		// Failed
		return;
	}

	/*
	* Configure boot sector for Legacy
	*/
	if (!WindowsSetup::UseLegacy)
		return;
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

	DrawTextCenter(L"Please wait while Panther2K prepares Windows Boot Manager to boot Windows on your computer. This should not take more than a minute to complete.", cSize.cx, 6);
}

void BootPreparationPage::Redrawer()
{
}

bool BootPreparationPage::KeyHandler(WPARAM wParam)
{
	return true;
}
