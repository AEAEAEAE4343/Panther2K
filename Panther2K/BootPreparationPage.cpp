#include "BootPreparationPage.h"
#include "WindowsSetup.h"
#include <iostream>
#include "shlobj.h"
#include "MessageBoxPage.h"
#include "WinPartedDll.h"

BOOL SetPrivilege(
	HANDLE hToken,              // access token handle
	LPCWSTR nameOfPrivilege,   // name of privilege to enable/disable
	BOOL bEnablePrivilege     // to enable or disable privilege
)
{
	TOKEN_PRIVILEGES tp;
	LUID luid;

	if (!LookupPrivilegeValue(
		NULL,               // lookup privilege on local system
		nameOfPrivilege,   // privilege to lookup 
		&luid))           // receives LUID of privilege
	{
		printf("LookupPrivilegeValue error: %u\n", GetLastError());
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	if (bEnablePrivilege)
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes = 0;

	// Enable the privilege or disable all privileges.

	if (!AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		(PTOKEN_PRIVILEGES)NULL,
		(PDWORD)NULL))
	{
		printf("AdjustTokenPrivileges error: %u\n", GetLastError());
		return FALSE;
	}

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

	{
		printf("The token does not have the specified privilege. \n");
		return FALSE;
	}

	return TRUE;
}

int CreateProcessPiped(char* outputBuffer, int bufferSize, wchar_t* commandLine) 
{
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
		WindowsSetup::ShowError(L"Failed to create pipe. %s", temp, PANTHER_LL_BASIC);
		return;
	}
	STARTUPINFOW si = { 0 };
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = stdOutWr;

	PROCESS_INFORMATION pi = { 0 };
	if (!CreateProcessW(NULL, commandLine, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi))
	{
		// Failed
		int temp = GetLastError();
		WindowsSetup::ShowError(L"Failed to create OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	// Close all handles except stdOutRd
	// We know bcdedit is done when stdOutRd receives an EOF
	CloseHandle(stdOutWr);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	// Read until the end of the pipe is reached or when the buffer is full
	DWORD bytesRead;
	BOOL retCode;
	for (char* buffer = outputBuffer; (buffer - outputBuffer < bufferSize) && (retCode = ReadFile(stdOutRd, buffer, 1, &bytesRead, NULL)); buffer += bytesRead);
	
	// Check the error code
	if (int temp = GetLastError() && temp != ERROR_BROKEN_PIPE)
		return temp;

	// If the end of the pipe was not reached but the buffer is full, read it to the end
	CHAR voidBuffer;
	while (retCode) retCode = ReadFile(stdOutRd, &voidBuffer, 1, &bytesRead, NULL);
	
	// Check the error code again to make sure nothing went wrong
	if (int temp = GetLastError() != ERROR_BROKEN_PIPE)
		return temp;

	return ERROR_SUCCESS;
}

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
	WindowsSetup::GetLogger()->Write(PANTHER_LL_BASIC, errorMessage);
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
	* Determine if Partition1 and Partition3 are on the same disk
	* (i.e. If the system partition is on the same disk as the boot partition.)
	* 
	* If they are, a different bcdedit flag (hd_partition=) must be used,
	* otherwise the BCD might reference a VHD file that is actually mounted
	* in the VM instead of the partitions themselves.
	* 
	* This was found during test installs on a VHD. It ouright refused to 
	* boot until I realized it was trying to find a non-existent VHD.
	*/

	LibPanther::Logger* logger = WindowsSetup::GetLogger();
	logger->Write(PANTHER_LL_NORMAL, L"Generating Windows Boot Manager files...");

	logger->Write(PANTHER_LL_DETAILED, L"Determining whether the system will be installed on a single disk...");
	VOLUME_DISK_EXTENTS* vde = (VOLUME_DISK_EXTENTS*)safeMalloc(WindowsSetup::GetLogger(), sizeof(VOLUME_DISK_EXTENTS));

	HANDLE volumeFileHandleP1 = CreateFileW(WindowsSetup::Partition1Volume, FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (volumeFileHandleP1 == INVALID_HANDLE_VALUE)
		return;
	HANDLE volumeFileHandleP3 = CreateFileW(WindowsSetup::Partition3Volume, FILE_READ_ATTRIBUTES | SYNCHRONIZE | FILE_TRAVERSE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (volumeFileHandleP3 == INVALID_HANDLE_VALUE)
		return;

	DWORD bytesCopied;
	if (!DeviceIoControl(volumeFileHandleP1, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, sizeof(VOLUME_DISK_EXTENTS), &bytesCopied, NULL))
		return;
	int p1Disk = vde->Extents[0].DiskNumber;
	unsigned long long p1Offset = vde->Extents[0].StartingOffset.QuadPart;
	if (!DeviceIoControl(volumeFileHandleP3, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, vde, sizeof(VOLUME_DISK_EXTENTS), &bytesCopied, NULL))
		return;
	bool singleDisk = p1Disk == vde->Extents[0].DiskNumber;
	CloseHandle(volumeFileHandleP1);
	CloseHandle(volumeFileHandleP3);
	free(vde);

	logger->Write(PANTHER_LL_DETAILED, singleDisk ? L"The system will be installed on a single disk, not using VHD detection."
		: L"The system will be installed on multiple disks, using VHD detection.");

	/*
	* Copy boot files
	*/

	logger->Write(PANTHER_LL_DETAILED, L"Copying boot files...");
	SHFILEOPSTRUCTW fos = { 0 };
	fos.hwnd = GetDesktopWindow();
	fos.wFunc = FO_COPY;
	fos.pFrom = pathBuffers[0];
	fos.pTo = pathBuffers[1];
	fos.fFlags = FOF_NO_UI;

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
		WindowsSetup::ShowError(L"Failed to create directory for boot files. %s", temp, PANTHER_LL_BASIC);
		return;
	}
	if (int temp = SHFileOperationW(&fos))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to copy boot files. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	/*
	* Move Windows Boot Manager to the correct location
	*/

	logger->Write(PANTHER_LL_DETAILED, L"Copying boot manager to proper location...");
	if (!WindowsSetup::UseLegacy)
	{
		swprintf_s(pathBuffers[1], L"%sEFI\\Boot", WindowsSetup::Partition1Mount);
		if (SHCreateDirectory(NULL, pathBuffers[1]) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			// Failed
			int temp = GetLastError();
			WindowsSetup::ShowError(L"Failed to create directory for boot files. %s", temp, PANTHER_LL_BASIC);
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
		WindowsSetup::ShowError(L"Failed to copy boot files. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	/*
	* Check if BCD store already exists
	*/

	logger->Write(PANTHER_LL_DETAILED, L"Looking for existing BCD...");
	wchar_t commandBuffer[MAX_PATH + 128];
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"%sBoot\\BCD"
													  : L"%sEFI\\Microsoft\\Boot\\BCD", WindowsSetup::Partition1Mount);
	DWORD dwAttrib = GetFileAttributes(commandBuffer);

	if (dwAttrib == INVALID_FILE_ATTRIBUTES ||
		dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
	{
		logger->Write(PANTHER_LL_DETAILED, L"No BCD exists. Creating a new store...");
		/*
		* Create BCD store
		*/
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /createstore %sBoot\\BCD"
														  : L"bcdedit /createstore %sEFI\\Microsoft\\Boot\\BCD", WindowsSetup::Partition1Mount);
		if (int temp = _wsystem(commandBuffer))
		{
			// Failed
			WindowsSetup::ShowError(L"Failed to create BCD store. %s", temp, PANTHER_LL_BASIC);
			return;
		}

		/*
		* Add Windows Boot Manager entry
		*/
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /create {bootmgr}"
														  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /create {bootmgr}", WindowsSetup::Partition1Mount);
		if (int temp = _wsystem(commandBuffer))
		{
			// Failed
			WindowsSetup::ShowError(L"Failed to create {bootmgr} entry. %s", temp, PANTHER_LL_BASIC);
			return;
		}

		/*
		* Configure Windows Boot Manager entry
		*/
		// device = s:
		swprintf_s(commandBuffer, L"bcdedit /store %s%s /set {bootmgr} device %s%s",
			WindowsSetup::Partition1Mount,
			WindowsSetup::UseLegacy ? L"Boot\\BCD" : L"EFI\\Microsoft\\Boot\\BCD",
			singleDisk ? L"hd_partition=" : L"partition=",
			WindowsSetup::Partition1Mount);
		if (int temp = _wsystem(commandBuffer))
		{
			// Failed
			WindowsSetup::ShowError(L"Failed to configure {bootmgr} entry. %s", temp, PANTHER_LL_BASIC);
			return;
		}
		swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {bootmgr} path \\bootmgr"
														  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {bootmgr} path \\EFI\\Microsoft\\Boot\\bootmgfw.efi", WindowsSetup::Partition1Mount);
		if (int temp = _wsystem(commandBuffer))
		{
			// Failed
			WindowsSetup::ShowError(L"Failed to configure {bootmgr} entry. %s", temp, PANTHER_LL_BASIC);
			return;
		}

		logger->Write(PANTHER_LL_DETAILED, L"Adding newly installed OS to BCD...");
	}
	else 
	{
		logger->Write(PANTHER_LL_DETAILED, L"A BCD already exists, adding newly installed OS to it...");
	}

	/*
	* Add Windows Boot Loader entry
	*/


	// Call bcdedit while redirecting stdout
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /create /application osloader"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /create /application osloader", WindowsSetup::Partition1Mount);
	
	// Read the GUID of the created osloader entry
	char guidBuffer[256];
	if (int temp = CreateProcessPiped(guidBuffer, 256, commandBuffer)) 
	{
		// Failed - no guid
		WindowsSetup::ShowError(L"Failed to retrieve OS loader GUID. %s", ERROR_INVALID_DATA, PANTHER_LL_BASIC);
		return;
	}

	// Convert the string containing the GUID into a wide char string
	wchar_t guidString[128];
	MultiByteToWideChar(GetConsoleCP(), MB_PRECOMPOSED, guidBuffer, 128, guidString, 128);

	// Find GUID of created entry
	guidString[127] = 0;
	wchar_t* guid = wcschr(guidString, L'{');
	if (guid == 0)
	{
		// Failed - no guid
		WindowsSetup::ShowError(L"Failed to retrieve OS loader GUID. %s", ERROR_INVALID_DATA, PANTHER_LL_BASIC);
		return;
	}
	guid[38] = 0;

	/*
	* Configure Windows Boot Loader entry
	*/

	logger->Write(PANTHER_LL_DETAILED, L"Configuring Windows Boot Loader...");
	
	// Set it to default
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /default %s"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /default %s", WindowsSetup::Partition1Mount, guid);
	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	// osdevice = W:
	swprintf_s(commandBuffer, L"bcdedit /store %s%s /set {default} osdevice %s%s",
		WindowsSetup::Partition1Mount,
		WindowsSetup::UseLegacy ? L"Boot\\BCD" : L"EFI\\Microsoft\\Boot\\BCD",
		singleDisk ? L"hd_partition=" : L"partition=",
		WindowsSetup::Partition3Mount);
	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	// device = W:
	swprintf_s(commandBuffer, L"bcdedit /store %s%s /set {default} device %s%s",
		WindowsSetup::Partition1Mount,
		WindowsSetup::UseLegacy ? L"Boot\\BCD" : L"EFI\\Microsoft\\Boot\\BCD",
		singleDisk ? L"hd_partition=" : L"partition=",
		WindowsSetup::Partition3Mount);
	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}
	// systemroot = \Windows
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {default} systemroot \\Windows"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {default} systemroot \\Windows", WindowsSetup::Partition1Mount);
	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}
	// path = \Windows\System32\winload.exe / .efi
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"bcdedit /store %sBoot\\BCD /set {default} path \\Windows\\System32\\winload.exe"
													  : L"bcdedit /store %sEFI\\Microsoft\\Boot\\BCD /set {default} path \\Windows\\System32\\winload.efi", WindowsSetup::Partition1Mount);
	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	// Set a name and display order, otherwise Boot Manager will refuse to boot the entry
	swprintf_s(commandBuffer, L"bcdedit /store %s%s /set {default} description \"Windows\"",
		WindowsSetup::Partition1Mount,
		WindowsSetup::UseLegacy ? L"Boot\\BCD" : L"EFI\\Microsoft\\Boot\\BCD");

	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	swprintf_s(commandBuffer, L"bcdedit /store %s%s /displayorder {default} /addfirst",
		WindowsSetup::Partition1Mount,
		WindowsSetup::UseLegacy ? L"Boot\\BCD" : L"EFI\\Microsoft\\Boot\\BCD");

	if (int temp = _wsystem(commandBuffer))
	{
		// Failed
		WindowsSetup::ShowError(L"Failed to configure OS loader entry. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	/*
	* TODO: SETUP WINDOWS RECOVERY ENVIRONMENT
	* 1. Create files
	* 2. Create recovery entry
	* 3. Configure using reagentc
	* 4. Set partition type to recovery
	*/
	if (WindowsSetup::UseRecovery) 
	{

	}

	/*
	* Convert the hive into a system hive
	* This is done through the registry as bcdedit does not allow this
	* This is REQUIRED for bcdedit to work in the installed system 
	* and thus for sysprep to succeed in specializing the system
	*/

	logger->Write(PANTHER_LL_DETAILED, L"Converting BCD into a system boot configuration store...");

	// Enable registry access first
	HANDLE proccessHandle = GetCurrentProcess();
	DWORD typeOfAccess = TOKEN_ADJUST_PRIVILEGES;
	HANDLE tokenHandle;
	if (OpenProcessToken(proccessHandle, typeOfAccess, &tokenHandle))
	{
		// Enabling RESTORE and BACKUP privileges
		SetPrivilege(tokenHandle, SE_RESTORE_NAME, TRUE);
		SetPrivilege(tokenHandle, SE_BACKUP_NAME, TRUE);
	}
	else
	{
		// Failed
		int temp = GetLastError();
		WindowsSetup::ShowError(L"Failed to convert BCD into system hive. %s", temp, PANTHER_LL_BASIC);
		return;
	}

	// TODO: Add error checking here
	// Set the settings for system hives
	LSTATUS status;
	swprintf_s(commandBuffer, WindowsSetup::UseLegacy ? L"%sBoot\\BCD" : L"%sEFI\\Microsoft\\Boot\\BCD", WindowsSetup::Partition1Mount);
	HKEY bcdKey;
	status = RegLoadKeyW(HKEY_LOCAL_MACHINE, L"p2k_bcd", commandBuffer);
	status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"p2k_bcd", &bcdKey);
	status = RegDeleteKeyValueW(bcdKey, L"Description", L"FirmwareModified");
	DWORD value = 1;
	status = RegSetKeyValueW(bcdKey, L"Description", L"System", REG_DWORD, &value, sizeof(DWORD));
	status = RegSetKeyValueW(bcdKey, L"Description", L"TreatAsSystem", REG_DWORD, &value, sizeof(DWORD));
	status = RegFlushKey(bcdKey);
	status = RegCloseKey(bcdKey);
	status = RegUnLoadKeyW(HKEY_LOCAL_MACHINE, L"p2k_bcd");

	// Restore privileges and close token
	SetPrivilege(tokenHandle, SE_RESTORE_NAME, FALSE);
	SetPrivilege(tokenHandle, SE_BACKUP_NAME, FALSE);
	CloseHandle(tokenHandle);

	/*
	* Configure boot sector for Legacy
	*/
	if (WindowsSetup::UseLegacy)
	{
		logger->Write(PANTHER_LL_DETAILED, L"Writing legacy boot sector...");
		swprintf_s(commandBuffer, L"bootsect /nt60 %c: /force /mbr", WindowsSetup::Partition1Mount[0]);
		if (int temp = _wsystem(commandBuffer))
		{
			// Failed
			WindowsSetup::ShowError(L"Failed to write boot sector. %s", temp, PANTHER_LL_BASIC);
			return;
		}
	}

	/*
	* Set partition type for EFI or System partition
	* This is REQUIRED for sysprep to succeed on UEFI, optional for Legacy
	*/
	logger->Write(PANTHER_LL_DETAILED, L"Setting partition type of the boot partition...");
	WinPartedDll::SetPartType(console, WindowsSetup::GetLogger(), p1Disk, p1Offset, WindowsSetup::UseLegacy ? 0x2700 : 0xef00);
	

	logger->Write(PANTHER_LL_DETAILED, L"Successfully prepared the newly installed OS for booting.");
}

void BootPreparationPage::Init()
{
	wchar_t* displayName = WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName;
	int length = lstrlenW(displayName);
	wchar_t* textBuffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), length * sizeof(wchar_t) + 14);
	memcpy(textBuffer, displayName, length * sizeof(wchar_t));
	memcpy(textBuffer + length, L" Setup", 14);
	text = textBuffer;
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
