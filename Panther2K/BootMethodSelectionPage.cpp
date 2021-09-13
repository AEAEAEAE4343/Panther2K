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

	console->SetPosition(6, 8);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->Write(L"  UEFI (Recommended): Modern method of booting. Uses a seperate");
	console->SetPosition(6, 9);
	console->Write(L"   partition to store files required for booting the computer.");
	console->SetPosition(6, 10);
	console->Write(L"   (Required for Windows 11 and up)");

	console->SetPosition(6, 12);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	console->Write(L"  Legacy/BIOS: Traditional method of booting. Uses the first sector");
	console->SetPosition(6, 13);
	console->Write(L"   of your harddrive to store code for loading Windows.");

	console->SetPosition(3, 15);
	console->Write(L"Use the LEFT and RIGHT arrow keys to select boot method.");

	DrawBox(23, 18, 12, 3, false);
	DrawBox(43, 18, 14, 3, false);
}

void BootMethodSelectionPage::Redrawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	if (legacy)
	{
		console->SetPosition(27, 19);
		console->Write(L"UEFI");
	}
	else
	{
		console->SetPosition(47, 19);
		console->Write(L"Legacy");
	}

	console->SetBackgroundColor(WindowsSetup::ForegroundColor);
	console->SetForegroundColor(WindowsSetup::BackgroundColor);

	if (legacy)
	{
		console->SetPosition(47, 19);
		console->Write(L"Legacy");
	}
	else
	{
		console->SetPosition(27, 19);
		console->Write(L"UEFI");
	}
}

void BootMethodSelectionPage::KeyHandler(WPARAM wParam)
{
	switch (wParam)
	{
	case VK_LEFT:
	case VK_RIGHT:
		legacy = !legacy;
		Redraw();
		break;
	case VK_ESCAPE:
		WindowsSetup::LoadPhase(2);
		break;
	case VK_RETURN:
		WindowsSetup::UseLegacy = legacy;
		WindowsSetup::LoadPhase(4);
		break;
	case VK_F3:
		AddPopup(new QuitingPage());
		break;
	}
}
