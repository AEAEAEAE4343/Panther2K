#include "MessagePage.h"

const wchar_t MessagePage::messageTypeButtons[][10] = {
	L"",
	L"OK",
	L"Cancel",
	L"Abort",
	L"Retry",
	L"Ignore",
	L"Yes",
	L"No",
	L"Try again",
	L"Continue"
};

const MessagePageResult MessagePage::returnValues[7][3] = {
	{ MessagePageResult::OK,     MessagePageResult::Fail,     MessagePageResult::Fail },
	{ MessagePageResult::OK,     MessagePageResult::Cancel,   MessagePageResult::Fail },
	{ MessagePageResult::Abort,  MessagePageResult::Retry,    MessagePageResult::Ignore },
	{ MessagePageResult::Yes,    MessagePageResult::No,       MessagePageResult::Cancel },
	{ MessagePageResult::Yes,    MessagePageResult::No,       MessagePageResult::Fail },
	{ MessagePageResult::Retry,  MessagePageResult::Cancel,   MessagePageResult::Fail },
	{ MessagePageResult::Cancel, MessagePageResult::TryAgain, MessagePageResult::Continue }
};
const MessagePageResult MessagePage::escReturnValues[7] = {
	  MessagePageResult::Fail,   MessagePageResult::Cancel,   MessagePageResult::Abort ,
      MessagePageResult::Cancel, MessagePageResult::Fail,     MessagePageResult::Cancel,
	  MessagePageResult::Cancel
};

MessagePage::MessagePage() : MessagePage(NULL) { }
MessagePage::MessagePage(Console* console) : MessagePage(console, L"No message was supplied for this page") { }
MessagePage::MessagePage(Console* console, const wchar_t* message) : MessagePage(console, message, MessagePageType::OK) { }
MessagePage::MessagePage(Console* console, const wchar_t* message, MessagePageType type) : MessagePage(console, message, type, MessagePageUI::Normal) { }
MessagePage::MessagePage(Console* console, const wchar_t* message, MessagePageType type, MessagePageUI uiType)
{
	messageText = message;
	messageType = type;
	messageUI = uiType;
	returnValue = MessagePageResult::Fail;

	SetStatusText(L"");

	selection = 0; selectionMax = 0;
	boxX = 0; boxY = 0; boxWidth = 0; boxHeight = 0;
	Initialize(console);
}

MessagePageResult MessagePage::ShowDialog()
{
	returnValue = MessagePageResult::Fail;
	if (GetConsole())
	{
		Draw();
		Run();
	}
	return returnValue;
}

void MessagePage::InitPage()
{
	SetDecorations(false, true, false);
}

void MessagePage::DrawPage()
{
	// Box with centered text, two indent on each side.
	// Should be 3/8 of the size of the window
	int lineCount;
	Console* console = GetConsole();
	SIZE consoleSize = console->GetSize();
	wchar_t* buffer = (wchar_t*)malloc(sizeof(wchar_t) * consoleSize.cx);
	int textWidth = consoleSize.cx - (consoleSize.cx / 8 * 3);
	wchar_t** lines = SplitStringToLines(messageText, textWidth - 2, &lineCount);
	int textHeight = lineCount + 2;

	const int UIBoxColors[] = { CONSOLE_COLOR_DARKFG, CONSOLE_COLOR_ERROR, CONSOLE_COLOR_PROGBAR };
	const int UIFgColors[] = { CONSOLE_COLOR_DARKFG, CONSOLE_COLOR_ERROR, CONSOLE_COLOR_DARKFG };

	console->SetBackgroundColor(CONSOLE_COLOR_FG);
	console->SetForegroundColor(UIBoxColors[(int)messageUI]);

    boxWidth = textWidth + 2;
	boxHeight = textHeight + 4;
	boxX = (consoleSize.cx / 2) - (boxWidth / 2);
	boxY = ((consoleSize.cy - 1) / 2) - (boxHeight / 2);
	console->DrawBox(boxX, boxY, boxWidth, boxHeight, true);

	console->SetForegroundColor(UIFgColors[(int)messageUI]);

	for (int y = 0; y < lineCount; y++)
	{
		//console->SetPosition(boxX + ((width / 2) - (lstrlenW(lines[y]) / 2)), boxY + 1 + y);
		console->SetPosition(boxX + 1 + ((textWidth / 2) - (lstrlenW(lines[y]) / 2)), boxY + 2 + y);
		console->Write(lines[y]);
	}
	
	/*console->SetPosition(boxX, boxY + boxHeight - 3);
	console->Write(L"\x255F");
	for (int i = 0; i < boxWidth - 2; i++)
		console->Write(L"\x2500");
	console->Write(L"\x2562");*/

	free(buffer);
}

void MessagePage::UpdatePage()
{
	Console* console = GetConsole();

	selectionMax = 0;
	for (int i = 0; i < 3; i++)
		if (returnValues[(int)messageType][i] != MessagePageResult::Fail)
			selectionMax++;

	int totalWidth = boxWidth - 2;
	int regionWidth = totalWidth / selectionMax;

	for (int i = 0; i < 3; i++)
	{
		if (returnValues[(int)messageType][i] != MessagePageResult::Fail)
		{
			int buttonWidth = 4 + lstrlenW(messageTypeButtons[(int)returnValues[(int)messageType][i]]);
			console->SetPosition(boxX + (i * regionWidth) + (regionWidth / 2) - (buttonWidth / 2), boxY + boxHeight - 3);

			console->SetBackgroundColor(CONSOLE_COLOR_LIGHTFG);
			console->SetForegroundColor(CONSOLE_COLOR_DARKFG);

			bool isSelected = i == selection;
			if (isSelected)
			{
				console->SetBackgroundColor(CONSOLE_COLOR_DARKFG);
				console->SetForegroundColor(CONSOLE_COLOR_LIGHTFG);
			}

			console->Write(isSelected ? L"< " : L"  ");
			console->Write(messageTypeButtons[(int)returnValues[(int)messageType][i]]);
			console->Write(isSelected ? L" >" : L"  ");
		}
	}
	selectionMax--;
}

void MessagePage::RunPage()
{
	Console* console = GetConsole();

	while (KEY_EVENT_RECORD* key = console->Read())
	{
		switch (key->wVirtualKeyCode)
		{
		case VK_LEFT:
			selection--;
			if (selection < 0)
				selection = 0;
			Update();
			break;
		case VK_RIGHT:
			selection++;
			if (selection > selectionMax)
				selection = selectionMax;
			Update();
			break;
		case VK_RETURN:
			returnValue = returnValues[(int)messageType][selection];
			return;
		case VK_ESCAPE:
			returnValue = escReturnValues[(int)messageType];
			if (returnValue != MessagePageResult::Fail)
				return;
		}
	}
}
