#pragma once

#include "Console.h"
#include "PopupPage.h"

class Page
{
public:
	Page();
	void Initialize(Console* con);
	void Draw();
	void Redraw(bool redraw = true);
	void HandleKey(WPARAM wParam);
	void AddPopup(PopupPage* popup);
	void RemovePopup();
	void DrawBox(int x, int y, int cx, int cy, bool useDouble);
	const wchar_t* text;
	const wchar_t* statusText;
	Console* console;
private:
	virtual void Init();
	virtual void Drawer();
	virtual void Redrawer();
	virtual void KeyHandler(WPARAM wParam);
protected:
	PopupPage* page;
};

