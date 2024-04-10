#include "BootMethodSelectionPage.h"
#include "WindowsSetup.h"
#include "QuitingPage.h"

BootMethodSelectionPage::~BootMethodSelectionPage()
{
	free((wchar_t*)text);
}

void BootMethodSelectionPage::Init()
{
	wchar_t* displayName = WindowsSetup::WimImageInfos[WindowsSetup::WimImageIndex - 1].DisplayName;
	int length = lstrlenW(displayName);
	wchar_t* textBuffer = (wchar_t*)safeMalloc(WindowsSetup::GetLogger(), length * sizeof(wchar_t) + 14);
	memcpy(textBuffer, displayName, length * sizeof(wchar_t));
	memcpy(textBuffer + length, L" Setup", 14);
	text = textBuffer;
	statusText = L"  ENTER=Select  ESC=Back  F3=Quit";
}

void BootMethodSelectionPage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);

	console->SetPosition(3, 4);
	console->Write(L"Select how you want to boot your computer.");

	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->SetPosition(3, 6);
	console->Write(L"Windows can be set up to boot in two ways:");

	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	y = console->GetPosition().y + 2;
	console->SetPosition(6, y);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->SetForegroundColor(legacy ? WindowsSetup::ForegroundColor : WindowsSetup::BackgroundColor);
	console->SetBackgroundColor(legacy ? WindowsSetup::BackgroundColor : WindowsSetup::ForegroundColor);
	DrawTextLeft(L"UEFI (Recommended): Modern method of booting. Uses a separate partition to store files required for booting the computer. (Required for Windows 11 and up)", console->GetSize().cx - 18, console->GetPosition().y);

	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->SetForegroundColor(legacy ? WindowsSetup::BackgroundColor : WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(legacy ? WindowsSetup::ForegroundColor : WindowsSetup::BackgroundColor);
	DrawTextLeft(L"Legacy/BIOS: Traditional method of booting. Uses the first sector of your hard drive to store code for loading Windows.", console->GetSize().cx - 18, console->GetPosition().y);

	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	DrawTextLeft(L"To select the boot method, use the UP and DOWN arrow keys.", console->GetSize().cx - 6, console->GetPosition().y + 2);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	DrawTextLeft(L"To continue with the installation, press Enter.", console->GetSize().cx - 6, console->GetPosition().y + 2);
}

void BootMethodSelectionPage::Redrawer()
{
	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetPosition(6, y);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->SetForegroundColor(legacy ? WindowsSetup::ForegroundColor : WindowsSetup::BackgroundColor);
	console->SetBackgroundColor(legacy ? WindowsSetup::BackgroundColor : WindowsSetup::ForegroundColor);
	DrawTextLeft(L"UEFI (Recommended): Modern method of booting. Uses a separate partition to store files required for booting the computer. (Required for Windows 11 and up)", console->GetSize().cx - 18, console->GetPosition().y);

	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->SetForegroundColor(legacy ? WindowsSetup::BackgroundColor : WindowsSetup::ForegroundColor);
	console->SetBackgroundColor(legacy ? WindowsSetup::ForegroundColor : WindowsSetup::BackgroundColor);
	DrawTextLeft(L"Legacy/BIOS: Traditional method of booting. Uses the first sector of your hard drive to store code for loading Windows.", console->GetSize().cx - 18, console->GetPosition().y);
}

bool BootMethodSelectionPage::KeyHandler(WPARAM wParam)
{
	switch (wParam)
	{
	case VK_UP:
	case VK_DOWN:
		legacy = !legacy;
		Redraw();
		break;
	case VK_ESCAPE:
		WindowsSetup::LoadPhase(2);
		break;
	case VK_RETURN:
		WindowsSetup::GetLogger()->Write(PANTHER_LL_DETAILED, legacy ? L"Using legacy boot." : L"Using UEFI boot.");
		WindowsSetup::UseLegacy = legacy;
		WindowsSetup::LoadPhase(4);
		break;
	case VK_F3:
		AddPopup(new QuitingPage());
		break;
	}
	return true;
}
