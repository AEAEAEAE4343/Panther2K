#pragma once
#include "Page.h"

enum class MessagePageResult
{
	Fail = 0,
	OK = IDOK,
	Cancel = IDCANCEL,
	Abort = IDABORT,
	Retry = IDRETRY,
	Ignore = IDIGNORE,
	Yes = IDYES,
	No = IDNO,
	TryAgain = IDTRYAGAIN,
	Continue = IDCONTINUE,
};

enum class MessagePageType
{
	OK = MB_OK,
	OKCancel = MB_OKCANCEL,
	AbortRetryIgnore = MB_ABORTRETRYIGNORE,
	YesNoCancel = MB_YESNOCANCEL,
	YesNo = MB_YESNO,
	RetryCancel = MB_RETRYCANCEL,
	CancelTryContinue = MB_CANCELTRYCONTINUE,
};

enum class MessagePageUI
{
	Normal = 0,
	Error = 1,
	Warning = 2,
};

class MessagePage : private Page
{
public:
	MessagePage();
	MessagePage(Console* console);
	MessagePage(Console* console, const wchar_t* message);
	MessagePage(Console* console, const wchar_t* message, MessagePageType type);
	MessagePage(Console* console, const wchar_t* message, MessagePageType type, MessagePageUI uiType);
	MessagePageResult ShowDialog();
private:
	const wchar_t* messageText;
	MessagePageType messageType;
	MessagePageUI messageUI;
	static const wchar_t messageTypeButtons[][10];
	static const MessagePageResult returnValues[7][3];
	static const MessagePageResult escReturnValues[7];
	MessagePageResult returnValue;
	int selection;
	int selectionMax;
	int boxX;
	int boxY;
	int boxWidth;
	int boxHeight;
	void InitPage();
	void DrawPage();
	void UpdatePage();
	void RunPage();
};

