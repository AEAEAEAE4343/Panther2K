#include "Page.h"
#include "PartitionManager.h"
#include "MessagePage.h"

Page::Page()
{
	console = 0;
	statusText = L"WinParted is inspecting your computer's hardware configuration...";
	text = L"WinParted 1.2.0m12";
	drawHeader = true;
	drawStatus = true;
	drawClear = true;
}

void Page::Initialize(Console* con)
{
	console = con;

	InitPage();
}

void Page::Draw()
{
	console->SetCursor(false, false);
	if (drawClear)
	{
		console->SetBackgroundColor(CONSOLE_COLOR_BG);
		console->SetForegroundColor(CONSOLE_COLOR_FG);

		console->Clear();
	}
	if (drawHeader)
	{
		console->SetBackgroundColor(CONSOLE_COLOR_BG);
		console->SetForegroundColor(CONSOLE_COLOR_FG);

		console->SetPosition(1, 1);
		console->WriteLine(text);
		for (int i = 0; i < lstrlen(text) + 2; i++)
			console->Write(L"\x2550");
	}
	DrawPage();
	Update();
}

void Page::Update()
{
	if (drawStatus)
	{
		console->SetBackgroundColor(CONSOLE_COLOR_FG);
		console->SetForegroundColor(CONSOLE_COLOR_DARKFG);

		SIZE f = console->GetSize();
		console->SetPosition(0, f.cy - 1);
		for (int i = 0; i < f.cx; i++)
			console->Write(L" ");

		console->SetPosition(2, f.cy - 1);
		console->Write(statusText);	
	}

	UpdatePage();
	console->Update();
}

void Page::Run()
{
	RunPage();
}

void Page::SetDecorations(bool header, bool status, bool clear)
{
	drawHeader = header;
	drawStatus = status;
	drawClear = clear;
}

void Page::SetText(const wchar_t* txt)
{
	text = txt;
}

void Page::SetStatusText(const wchar_t* txt)
{
	statusText = txt;
}

void Page::InitPage()
{
}

void Page::DrawPage()
{
}

void Page::UpdatePage()
{
}

void Page::RunPage()
{
	//console->Read(1);
}

Console* Page::GetConsole()
{
	return console;
}
