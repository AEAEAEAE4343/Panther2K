#pragma once
#include <PantherLogger.h>
#include <PantherConsole.h>

class WinPartedDll 
{
public:
	static int RunWinParted(Console*, LibPanther::Logger*);
	static HRESULT ApplyP2KLayoutToDiskGPT(Console*, LibPanther::Logger*, int, bool, wchar_t***, wchar_t***);
	static HRESULT ApplyP2KLayoutToDiskMBR(Console*, LibPanther::Logger*, int, bool, wchar_t***, wchar_t***);
	static HRESULT SetPartType(Console*, LibPanther::Logger*, int, unsigned long long, short);
	static HRESULT MountPartition(Console*, LibPanther::Logger*, int, unsigned long long, const wchar_t*);
private:
	static HMODULE hWinParted;
	static HRESULT InitParted();
	static bool partedInitialized;
};