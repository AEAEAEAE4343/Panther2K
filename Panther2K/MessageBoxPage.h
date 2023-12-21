#pragma once
#include "PopupPage.h"
class MessageBoxPage : public PopupPage
{
public:
	MessageBoxPage(const wchar_t* text, bool isError, Page* par);
	void ShowDialog();
private:
	const wchar_t* fullText;
	int lineCount;
	wchar_t** lines;
	bool error;
	bool shown;

	virtual void Init() override;
	virtual void Drawer() override;
	virtual bool KeyHandler(WPARAM wParam) override;
};

