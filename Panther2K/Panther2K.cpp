// Panther2K.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Panther2K.h"
#include "WindowsSetup.h"

// Command line builds (for testing and debugging)
int wmain(int argc, wchar_t** argv) 
{
    if (__argc == 2 && lstrcmpiW(__wargv[1], L"--pe") == 0) 
    {
        WindowsSetup::IsWinPE = true;
    }

    return WindowsSetup::RunSetup();
}

// Headless builds (for releases)
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    return wmain(0, nullptr);
}
