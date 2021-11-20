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
	void DrawTextLeft(const wchar_t* string, int cx, int y);
	void DrawTextRight(const wchar_t* string, int cx, int y);
	void DrawTextCenter(const wchar_t* string, int cx, int y);
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

