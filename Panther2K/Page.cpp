#include "Page.h"
#include "WindowsSetup.h"

Page::Page()
{
	console = 0;
	page = 0;
	statusText = 0;
	text = 0;
}

void Page::Initialize(Console* con)
{
	console = con;

	Init();
	Draw();
}

void Page::Draw()
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->Clear();

	Drawer();
	Redraw(false);

	if (page != 0)
		page->Draw();

	console->RedrawImmediately();
}

void Page::Redraw(bool redraw)
{
	console->SetBackgroundColor(WindowsSetup::BackgroundColor);
	console->SetForegroundColor(WindowsSetup::ForegroundColor);

	console->SetPosition(1, 1);
	console->WriteLine(text);
	for (int i = 0; i < lstrlen(text) + 2; i++)
		console->Write(WindowsSetup::UseCp437 ? L"\xCD" : L"═");

	console->SetBackgroundColor(WindowsSetup::ForegroundColor);
	console->SetForegroundColor(WindowsSetup::DarkForegroundColor);

	SIZE f = console->GetSize();
	console->SetPosition(0, f.cy - 1);
	for (int i = 0; i < f.cx; i++)
		console->Write(L" ");

	console->SetPosition(0, f.cy - 1);
	console->Write(statusText);

	Redrawer();
	if (redraw)
		console->RedrawImmediately();
}

void Page::HandleKey(WPARAM wParam)
{
	if (wParam == VK_F5)
		Draw();
	if (page == 0)
		KeyHandler(wParam);
	else
		page->HandleKey(wParam);
}

void Page::DrawBox(int boxX, int boxY, int boxWidth, int boxHeight, bool useDouble)
{
	for (int i = 0; i < boxHeight; i++)
	{
		console->SetPosition(boxX, boxY + i);
		for (int j = 0; j < boxWidth; j++)
			console->Write(L" ");
	}

	console->SetPosition(boxX, boxY);
	console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xC9" : L"╔") : (WindowsSetup::UseCp437 ? L"\xDA" : L"┌"));
	console->SetPosition(boxX + (boxWidth - 1), boxY);
	console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xBB" : L"╗") : (WindowsSetup::UseCp437 ? L"\xBF" : L"┐"));
	console->SetPosition(boxX, boxY + (boxHeight - 1));
	console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xC8" : L"╚") : (WindowsSetup::UseCp437 ? L"\xC0" : L"└"));
	console->SetPosition(boxX + (boxWidth - 1), boxY + (boxHeight - 1));
	console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xBC" : L"╝") : (WindowsSetup::UseCp437 ? L"\xD9" : L"┘"));

	console->SetPosition(boxX + 1, boxY);
	for (int i = 0; i < boxWidth - 2; i++)
		console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xCD" : L"═") : (WindowsSetup::UseCp437 ? L"\xC4" : L"─"));

	console->SetPosition(boxX + 1, boxY + boxHeight - 1);
	for (int i = 0; i < boxWidth - 2; i++)
		console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xCD" : L"═") : (WindowsSetup::UseCp437 ? L"\xC4" : L"─"));

	for (int i = 0; i < boxHeight - 2; i++)
	{
		console->SetPosition(boxX, boxY + 1 + i);
		console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xBA" : L"║") : (WindowsSetup::UseCp437 ? L"\xB3" : L"│"));
	}

	for (int i = 0; i < boxHeight - 2; i++)
	{
		console->SetPosition(boxX + boxWidth - 1, boxY + 1 + i);
		console->Write(useDouble ? (WindowsSetup::UseCp437 ? L"\xBA" : L"║") : (WindowsSetup::UseCp437 ? L"\xB3" : L"│"));
	}
}

void Page::DrawTextLeft(const wchar_t* string, int cx, int y)
{
	int lineCount = 0;
	wchar_t** lines = SplitStringToLines(string, cx, &lineCount);
	
	int x = (console->GetSize().cx - cx) / 2;
	for (int i = 0; i < lineCount; i++, y++)
	{
		console->SetPosition(x, y);
		console->Write(lines[i]);
	}
}

void Page::DrawTextRight(const wchar_t* string, int cx, int y)
{
	int lineCount = 0;
	wchar_t** lines = SplitStringToLines(string, cx, &lineCount);

	int paragraphX = (console->GetSize().cx - cx) / 2;
	for (int i = 0; i < lineCount; i++, y++)
	{
		int x = paragraphX + (cx - lstrlenW(lines[i]));
		console->SetPosition(x, y);
		console->Write(lines[i]);
	}
}

void Page::DrawTextCenter(const wchar_t* string, int cx, int y)
{
	int lineCount = 0;
	wchar_t** lines = SplitStringToLines(string, cx, &lineCount);

	int paragraphX = (console->GetSize().cx - cx) / 2;
	for (int i = 0; i < lineCount; i++, y++)
	{
		int x = paragraphX + ((cx - lstrlenW(lines[i]) ) / 2);
		console->SetPosition(x, y);
		console->Write(lines[i]);
	}
}

void Page::AddPopup(PopupPage* popup)
{
	page = popup;
	page->Initialize(console, this);
	Draw();
}

void Page::RemovePopup()
{
	delete(page);
	page = 0;
	Draw();
}

void Page::Init()
{
}

void Page::Drawer()
{
}

void Page::Redrawer()
{
}

void Page::KeyHandler(WPARAM wParam)
{
}
