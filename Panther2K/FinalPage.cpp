#include "FinalPage.h"
#include "WindowsSetup.h"

void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD)
{
	PostQuitMessage(0);
}

void FinalPage::Init()
{
	text = L"Panther2K";
	statusText = L"  ENTER=Restart";

	SetTimer(NULL, NULL, WindowsSetup::RebootTimer, TimerProc);
}

void FinalPage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);

	console->SetPosition(3, 4);
	console->Write(L"Setup completed succesfully.");

	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->SetPosition(3, 6);
	console->Write(L"Setup has finished installing Windows onto your computer. The Windows \n   Out-Of-Box Experience (OOBE) will guide you through the rest of the\n   installation. Panther2K will restart your computer in 10 seconds.");

	console->SetPosition(3, 10);
	console->Write(L"To restart now press the ENTER key.");
}

void FinalPage::Redrawer()
{
}

bool FinalPage::KeyHandler(WPARAM wParam)
{
	return wParam != VK_RETURN;
}
