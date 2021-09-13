#pragma once

#include "Console.h"

class Page;

class PopupPage
{
public:
	void Initialize(Console* con, Page* par);
	void Draw();
	void HandleKey(WPARAM wParam);
	Page* parent;
private:
	virtual void Init();
	virtual void Drawer();
	virtual void KeyHandler(WPARAM wParam);
protected:
	bool customColor = false;
	COLOR fore, back;
	int width, height;
	const wchar_t* statusText;
	Console* console;
};

#include "Page.h"

