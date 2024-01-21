#pragma once
#include <PantherLogger.h>
#include <PantherConsole.h>

class WinPartedDll 
{
public:
	static int RunWinParted(Console* console, LibPanther::Logger* logger);
private:
	static HMODULE hWinParted;
	static bool InitParted();
	static bool partedInitialized;
};