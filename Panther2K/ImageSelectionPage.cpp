#include "ImageSelectionPage.h"
#include "WindowsSetup.h"
#include "QuitingPage.h"

void ImageSelectionPage::Init()
{
	text = L"Leet's Panther2K Setup";
	statusText = L"  ENTER=Select  ESC=Back  F3=Quit";

	wchar_t buffer[256];

	::std::array<wchar_t, 256> a;

	WindowsSetup::EnumerateImageInfo();

	for (int i = 0; i < WindowsSetup::WimImageCount; i++)
	{
		ImageInfo* info = WindowsSetup::WimImageInfos + i;
		const wchar_t* arch;
		switch (info->Architecture)
		{
		case 0:
			arch = L"x86";
			break;
		case 9:
			arch = L"x86_64";
			break;
		case 5:
			arch = L"ARM64";
			break;
		default:
			arch = L"NaN";
			break;
		}

		swprintf(buffer, 256, L"%-*s %-6s %02d/%02d/%04d", console->GetSize().cx - 16 - 18, info->DisplayName, arch, info->CreationTime.wDay, info->CreationTime.wMonth, info->CreationTime.wYear);
		memcpy(a.data(), buffer, sizeof(wchar_t) * 256);
		//::std::copy(::std::begin(buffer), ::std::end(buffer), a.begin());
		//MessageBoxW(console->WindowHandle, a.data(), L"", MB_OK);
		FormattedStrings.push_back(a);
	}
}

void ImageSelectionPage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);
	console->SetPosition(3, 4);
	console->Write(L"Select the operating system to be installed.");

	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	DrawTextLeft(L"Multiple operating systems were detected inside the WIM or ESD image. Please select the copy of Microsoft(R) Windows(TM) you would like to install onto your computer.", console->GetSize().cx - 6, 6);
	DrawTextLeft(L"Use the UP and DOWN arrow keys to select an operating system.", console->GetSize().cx - 6, console->GetPosition().y + 2);

	SIZE consoleSize = console->GetSize();
	int boxX = 3;
	int boxWidth = consoleSize.cx - 6;
	boxY = console->GetPosition().y + 2;
	int boxHeight = consoleSize.cy - boxY - 2;
	int maxItems = boxHeight - 3;
	DrawBox(boxX, boxY, boxWidth, boxHeight, false);

	DrawTextLeft(L"Name", console->GetSize().cx - 16, boxY + 1);
	DrawTextRight(L"Arch   Date      ", console->GetSize().cx - 16, boxY + 1);
}

void ImageSelectionPage::Redrawer()
{
	SIZE consoleSize = console->GetSize();
	int boxHeight = consoleSize.cy - boxY - 2;
	int maxItems = boxHeight - 3;

	bool canScrollDown = (scrollIndex + maxItems) < WindowsSetup::WimImageCount;
	bool canScrollUp = scrollIndex != 0;

	for (int i = 0; i < min(maxItems, WindowsSetup::WimImageCount); i++)
	{
		int j = i + scrollIndex;
		wchar_t* text = FormattedStrings[j].data();
		/*wchar_t buffer[100];
		swprintf(buffer, 100, L"maxItems: %i\nimageCount: %i\nstringCount: %i\ni: %i\nscroll: %i\nj: %i", maxItems, WindowsSetup::WimImageCount, FormattedStrings.size(), i, scrollIndex, j);
		MessageBoxW(console->WindowHandle, buffer, L"", MB_OK);*/

		if (i == selectionIndex)
		{
			console->SetBackgroundColor(WindowsSetup::ForegroundColor);
			console->SetForegroundColor(WindowsSetup::BackgroundColor);
		}
		else
		{
			console->SetBackgroundColor(WindowsSetup::BackgroundColor);
			console->SetForegroundColor(WindowsSetup::ForegroundColor);
		}
		console->SetPosition(8, boxY + 2 + i);
		console->Write(text);
	}

	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->SetPosition(console->GetSize().cx - 6, boxY + boxHeight - 2);
	if (canScrollDown) console->Write(WindowsSetup::UseCp437 ? L"\x19" : L"↓");
	else console->Write(L" ");
	
	console->SetPosition(console->GetSize().cx - 6, boxY + 2);
	if (canScrollUp) console->Write(WindowsSetup::UseCp437 ? L"\x18" : L"↑");
	else console->Write(L" ");
}

bool ImageSelectionPage::KeyHandler(WPARAM wParam)
{
	SIZE consoleSize = console->GetSize();
	int boxHeight = consoleSize.cy - boxY - 2;
	int maxItems = boxHeight - 3;
	int totalItems = FormattedStrings.size();

	switch (wParam)
	{
	case VK_DOWN:
		if (selectionIndex + 1 < min(maxItems, totalItems))
			selectionIndex++;
		else if (selectionIndex + scrollIndex + 1 < totalItems)
			scrollIndex++;
		Redraw();
		break;
	case VK_UP:
		if (selectionIndex - 1 >= 0)
			selectionIndex--;
		else if (selectionIndex + scrollIndex - 1 >= 0)
			scrollIndex--;
		Redraw();
		break;
	case VK_RETURN:
		WindowsSetup::WimImageIndex = scrollIndex + selectionIndex + 1;
		WindowsSetup::LoadPhase(3);
		break;
	case VK_F3:
		AddPopup(new QuitingPage());
		break;
	case VK_ESCAPE:
		WindowsSetup::LoadPhase(1);
		break;
	}
	return true;
}
