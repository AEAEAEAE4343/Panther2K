#include "WinPartedDll.h"
#include "iatpatch.h"

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
typedef HRESULT(*ApplyP2KLayoutToDiskGPTStub)(Console*, LibPanther::Logger*, int);

int WinPartedDll::RunWinParted(Console* console, LibPanther::Logger* logger)
{
	if (!partedInitialized && !InitParted())
		return ERROR_BAD_FORMAT;
	auto runWinParted = (RunWinPartedStub)GetProcAddress(hWinParted, ORD_RunWinParted);
	return runWinParted(console, logger);
}

bool WinPartedDll::InitParted()
{
	if (partedInitialized) return true;

	// Try loading WinParted
	hWinParted = LoadLibraryA("WinParted.exe");
	if (!hWinParted) return false;

	ParseIAT(hWinParted);
	auto initializeCRT = (InitializeCRTStub)GetProcAddress(hWinParted, ORD_InitializeCRT);
	if (!initializeCRT) return false;

	initializeCRT();

	partedInitialized = true;
	return true;
}
