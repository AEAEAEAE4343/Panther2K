#include "ImageSelectionPage.h"
#include "WindowsSetup.h"
#include "QuitingPage.h"

void ImageSelectionPage::Init()
{
	text = L"Leet's Panther2K Setup";
	statusText = L"  ENTER=Select  ESC=Back  F3=Quit";

	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * 64);
	if (buffer == 0)
		return;

	::std::array<wchar_t, 64> a;

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

		swprintf(buffer, 64, L"%-47s %-6s 00/69/0420", info->DisplayName, arch);
		memcpy(a.data(), buffer, sizeof(wchar_t) * 64);
		//::std::copy(::std::begin(buffer), ::std::end(buffer), a.begin());
		//MessageBoxW(console->WindowHandle, a.data(), L"", MB_OK);
		FormattedStrings.push_back(a);
	}
	// Turn ImageInfo into formatted strings (sprintf?)
	// Later, we can use the index to set the wanted image
	free(buffer);
}

void ImageSelectionPage::Drawer()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::LightForegroundColor);

	console->SetPosition(3, 4);
	console->Write(L"Select the operating system to be installed.");

	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->SetPosition(3, 6);
	console->Write(L"Multiple operating systems were detected inside the WIM or ESD image.\n   Please select the copy of Microsoft(R) Windows(TM) you would like to\n   install onto your computer.\n\n   Use the UP and DOWN arrow keys to select an operating system.");

	SIZE consoleSize = console->GetSize();
	int boxWidth = consoleSize.cx - 6;
	int boxHeight = consoleSize.cy - 14;
	int maxItems = boxHeight - 3;
	int boxX = 3;
	int boxY = 12;
	DrawBox(boxX, boxY, boxWidth, boxHeight, false);

	console->SetPosition(8, 13);
	console->Write(L"Name                                            Arch   Date");
}

void ImageSelectionPage::Redrawer()
{
	SIZE consoleSize = console->GetSize();
	int boxHeight = consoleSize.cy - 14;
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
		console->SetPosition(8, 14 + i);
		console->Write(text);
	}

	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);
	if (canScrollDown)
	{
		console->SetPosition(74, 21);
		console->Write(WindowsSetup::UseCp437 ? L"\x19" : L"↓");
	}
	if (canScrollUp)
	{
		console->SetPosition(74, 14);
		console->Write(WindowsSetup::UseCp437 ? L"\x18" : L"↑");
	}
}

void ImageSelectionPage::KeyHandler(WPARAM wParam)
{
	SIZE consoleSize = console->GetSize();
	int boxWidth = consoleSize.cx - 6;
	int boxHeight = consoleSize.cy - 14;
	int maxItems = boxHeight - 3;
	int totalItems = FormattedStrings.size();
	int boxX = 3;
	int boxY = 12;

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
}
