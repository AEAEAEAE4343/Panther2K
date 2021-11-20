#include "WelcomePage.h"
#include "QuitingPage.h"
#include "WindowsSetup.h"
#include "ImageSelectionPage.h"

void WelcomePage::Init()
{
	text = L"Leet's Panther2K";
	statusText = L"  ENTER=Continue  R=Repair  F3=Quit";
}

void WelcomePage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);
	console->SetPosition(3, 4);
	console->Write(L"Welcome to Panther2K.");

	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	DrawTextLeft(L"The Setup portion of the Panther2K utility prepares Microsoft(R) Windows to run on your computer.", console->GetSize().cx - 6, 6);
	
	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	DrawTextLeft(L"To launch Setup, press ENTER", console->GetSize().cx - 18, console->GetPosition().y);

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	DrawTextLeft(L"To repair a Windows installation, press R", console->GetSize().cx - 18, console->GetPosition().y);

	console->SetPosition(6, console->GetPosition().y + 2);
	console->Write(WindowsSetup::UseCp437 ? L"\x07" : L"•");
	DrawTextLeft(L"To quit Panther2K without installing Windows, press F3", console->GetSize().cx - 18, console->GetPosition().y);
}

void WelcomePage::Redrawer()
{
}

void WelcomePage::KeyHandler(WPARAM wParam)
{
	switch (wParam) 
	{
	case VK_RETURN:
		statusText = L"  Please wait while Setup loads data...";
		Draw();
		WindowsSetup::LoadWimFile();
		WindowsSetup::LoadPhase(2);
		break;
	case (WPARAM)'R':
		break;
	case VK_F3:
		AddPopup(new QuitingPage());
		break;
	}
}
