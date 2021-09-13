#include "QuitingPage.h"
#include "WelcomePage.h"
#include "WindowsSetup.h"

void QuitingPage::Init()
{
	width = 52;
	height = 8;

	statusText = L"  F3=Quit  ENTER=Continue";

	customColor = true;
	back = WindowsSetup::ForegroundColor;
	fore = WindowsSetup::ErrorColor;
}

void QuitingPage::Drawer()
{
	int boxWidth = width + 2;
	int boxHeight = height + 2;

	SIZE d = console->GetSize();

	int boxX = (d.cx / 2) - (boxWidth / 2);
	int boxY = ((d.cy - 1) / 2) - (boxHeight / 2);

	console->SetPosition(boxX + 3, boxY + 1);
	console->Write(L"Windows is not completely set up on your");
	console->SetPosition(boxX + 3, boxY + 2);
	console->Write(L"computer. If you quit Setup now, you will need");
	console->SetPosition(boxX + 3, boxY + 3);
	console->Write(L"to run Setup again to set up Windows.");

	console->SetPosition(boxX + 6, boxY + 5);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->Write(L" To continue Setup, press ENTER.");
	console->SetPosition(boxX + 6, boxY + 6);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->Write(L" To quit Setup, press F3.");
}

void QuitingPage::KeyHandler(WPARAM wParam)
{
	switch (wParam)
	{
	case VK_RETURN:
		parent->RemovePopup();
		break;
	case VK_F3:
		PostQuitMessage(ERROR_CANCELLED);
		break;
	}
}
