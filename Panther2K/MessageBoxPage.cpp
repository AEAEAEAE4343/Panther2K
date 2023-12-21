#include "MessageBoxPage.h"
#include "WindowsSetup.h"

MessageBoxPage::MessageBoxPage(const wchar_t* text, bool isError, Page* par)
{
	fullText = text;
	error = isError;
	lineCount = 0;
	lines = 0;
	shown = false;
	parent = par;
}

void MessageBoxPage::ShowDialog()
{
	MSG msg;
	parent->AddPopup(this);

	for (KEY_EVENT_RECORD* record = console->Read();
		!record->bKeyDown || HandleKey(record->wVirtualKeyCode);
		record = console->Read()) {
	}

	parent->RemovePopup();
}

void MessageBoxPage::Init()
{
	SIZE consoleSize = console->GetSize();
	width = consoleSize.cx - (consoleSize.cx / 8 * 3);

	lines = SplitStringToLines(fullText, width - 2, &lineCount);
	height = lineCount + 2;

	statusText = L"  ENTER=Continue";

	customColor = true;

	back = WindowsSetup::ForegroundColor;
	if (error)
		fore = WindowsSetup::ErrorColor;
	else 
		fore = WindowsSetup::DarkForegroundColor;
}

void MessageBoxPage::Drawer()
{
	SIZE d = console->GetSize();
	int boxWidth = width + 2;
	int boxHeight = height + 2;
	int boxX = (d.cx / 2) - (boxWidth / 2);
	int boxY = ((d.cy - 1) / 2) - (boxHeight / 2);

	for (int y = 0; y < lineCount; y++)
	{
		//console->SetPosition(boxX + ((width / 2) - (lstrlenW(lines[y]) / 2)), boxY + 1 + y);
		console->SetPosition(boxX + 1 + ((width / 2) - (lstrlenW(lines[y]) / 2)), boxY + 1 + y);
		console->Write(lines[y]);
	}
}

bool MessageBoxPage::KeyHandler(WPARAM wParam)
{
	if (wParam == VK_RETURN)
		return false;
	return true;
}
