#include "PopupPage.h"
#include "WindowsSetup.h"

void PopupPage::Initialize(Console* con, Page* par)
{
	console = con;
	parent = par;

	Init();
	Draw();
}

void PopupPage::Draw()
{
	COLOR a, b;

	if (customColor)
	{
		a = console->GetBackgroundColor();
		b = console->GetForegroundColor();
		console->SetBackgroundColor(back);
		console->SetForegroundColor(fore);
	}
	else
	{
		a = console->GetBackgroundColor();
		b = console->GetForegroundColor();
		console->SetBackgroundColor(b);
		console->SetForegroundColor(a);
	}

	int boxWidth = width + 2;
	int boxHeight = height + 2;

	SIZE d = console->GetSize();

	int boxX = (d.cx / 2) - (boxWidth / 2);
	int boxY = ((d.cy - 1) / 2) - (boxHeight / 2);

	for (int i = 0; i < boxHeight; i++)
	{
		console->SetPosition(boxX, boxY + i);
		for (int j = 0; j < boxWidth; j++)
			console->Write(L" ");
	}

	parent->DrawBox(boxX, boxY, boxWidth, boxHeight, true);

	/*console->SetPosition(boxX, boxY);
	console->Write(L"╔");
	console->SetPosition(boxX + (boxWidth - 1), boxY);
	console->Write(L"╗");
	console->SetPosition(boxX, boxY + (boxHeight - 1));
	console->Write(L"╚");
	console->SetPosition(boxX + (boxWidth - 1), boxY + (boxHeight - 1));
	console->Write(L"╝");

	console->SetPosition(boxX + 1, boxY);
	for (int i = 0; i < boxWidth - 2; i++)
		console->Write(L"═");

	console->SetPosition(boxX + 1, boxY + boxHeight - 1);
	for (int i = 0; i < boxWidth - 2; i++)
		console->Write(L"═");

	for (int i = 0; i < boxHeight - 2; i++)
	{
		console->SetPosition(boxX, boxY + 1 + i);
		console->Write(L"║");
	}

	for (int i = 0; i < boxHeight - 2; i++)
	{
		console->SetPosition(boxX + boxWidth - 1, boxY + 1 + i);
		console->Write(L"║");
	}*/

	console->SetPosition(boxX, boxY + boxHeight - 1 - 2);
	console->Write(WindowsSetup::UseCp437 ? L"\xC7" : L"╟");
	for (int i = 0; i < width; i++)
	{
		console->Write(WindowsSetup::UseCp437 ? L"\xC4" : L"─");
	}
	console->Write(WindowsSetup::UseCp437 ? L"\xB6" : L"╢");
	console->SetPosition(boxX + 1, boxY + boxHeight - 1 - 1);
	console->Write(statusText);

	Drawer();

	console->SetBackgroundColor(a);
	console->SetForegroundColor(b);
}

bool PopupPage::HandleKey(WPARAM wParam)
{
	return KeyHandler(wParam);
}

void PopupPage::Init()
{
}

void PopupPage::Drawer()
{
}

bool PopupPage::KeyHandler(WPARAM wParam)
{
	return true;
}
