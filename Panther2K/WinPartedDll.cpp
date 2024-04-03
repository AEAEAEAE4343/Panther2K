#include "WinPartedDll.h"
#include "iatpatch.h"
#include "WindowsSetup.h"

/*
EXPORTS
   InitializeCRT   @1
   RunWinParted   @2
   ApplyP2KLayoutToDiskGPT   @3
   ApplyP2KLayoutToDiskMBR   @4
   SetPartType   @5
   FormatAndOrMountPartition   @6
*/

#define ORD_InitializeCRT             (LPCSTR)1
#define ORD_RunWinParted              (LPCSTR)2
#define ORD_ApplyP2KLayoutToDiskGPT   (LPCSTR)3
#define ORD_ApplyP2KLayoutToDiskMBR   (LPCSTR)4
#define ORD_SetPartType               (LPCSTR)5
#define ORD_FormatAndOrMountPartition (LPCSTR)6

typedef void (*InitializeCRTStub)();
typedef int (*RunWinPartedStub)(Console*, LibPanther::Logger*);
typedef HRESULT(*ApplyP2KLayoutToDiskGPTStub)(Console*, LibPanther::Logger*, int, bool, wchar_t***, wchar_t***);
typedef HRESULT(*ApplyP2KLayoutToDiskMBRStub)(Console*, LibPanther::Logger*, int, bool, wchar_t***, wchar_t***);
typedef HRESULT(*SetPartTypeStub)(Console*, LibPanther::Logger*, int, unsigned long long, short);

bool WinPartedDll::partedInitialized = false;
HMODULE WinPartedDll::hWinParted = NULL;

int WinPartedDll::RunWinParted(Console* console, LibPanther::Logger* logger)
{
	if (!partedInitialized && InitParted() != ERROR_SUCCESS)
		return ERROR_BAD_FORMAT;
	auto runWinParted = (RunWinPartedStub)GetProcAddress(hWinParted, ORD_RunWinParted);
	return runWinParted(console, logger);
}

HRESULT WinPartedDll::ApplyP2KLayoutToDiskGPT(Console* console, LibPanther::Logger* logger, int diskNumber, bool letters, wchar_t*** mountPath, wchar_t*** volumeList)
{
	HRESULT res;
	if (!partedInitialized && (res = InitParted()) != ERROR_SUCCESS)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_BAD_FORMAT);

	auto applyP2kLayout = (ApplyP2KLayoutToDiskGPTStub)GetProcAddress(hWinParted, ORD_ApplyP2KLayoutToDiskGPT);
	return applyP2kLayout(console, logger, diskNumber, letters, mountPath, volumeList);
}

HRESULT WinPartedDll::ApplyP2KLayoutToDiskMBR(Console* console, LibPanther::Logger* logger, int diskNumber, bool letters, wchar_t*** mountPath, wchar_t*** volumeList)
{
	HRESULT res;
	if (!partedInitialized && (res = InitParted()) != ERROR_SUCCESS)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, res);

	auto applyP2kLayout = (ApplyP2KLayoutToDiskMBRStub)GetProcAddress(hWinParted, ORD_ApplyP2KLayoutToDiskMBR);
	return applyP2kLayout(console, logger, diskNumber, letters, mountPath, volumeList);
}

HRESULT WinPartedDll::SetPartType(Console* console, LibPanther::Logger* logger, int diskNumber, unsigned long long partOffset, short partType)
{
	HRESULT res;
	if (!partedInitialized && (res = InitParted()) != ERROR_SUCCESS)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, res);

	auto setPartType = (SetPartTypeStub)GetProcAddress(hWinParted, ORD_SetPartType);
	return setPartType(console, logger, diskNumber, partOffset, partType);
}

HRESULT WinPartedDll::InitParted()
{
	if (partedInitialized) return ERROR_SUCCESS;

	// Try loading WinParted
	hWinParted = LoadLibraryA("WinParted.exe");
	if (!hWinParted)
	{
		wlogf(WindowsSetup::GetLogger(), PANTHER_LL_BASIC, 60, L"Error occurred while loading WinParted (0x%08x).", GetLastError());
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, GetLastError());
	}

	ParseIAT(hWinParted);
	auto initializeCRT = (InitializeCRTStub)GetProcAddress(hWinParted, ORD_InitializeCRT);
	if (!initializeCRT) 
	{
		wlogf(WindowsSetup::GetLogger(), PANTHER_LL_BASIC, 80, L"Error occurred while initializing WinParted CRT runtime (0x%08x).", GetLastError());
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, GetLastError());
	};

	initializeCRT();

	partedInitialized = true;
	return ERROR_SUCCESS;
}