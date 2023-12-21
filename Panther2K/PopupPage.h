#pragma once

#include <PantherConsole.h>

class Page;

class PopupPage
{
public:
	void Initialize(Console* con, Page* par);
	void Draw();
	bool HandleKey(WPARAM wParam);
	Page* parent;
private:
	virtual void Init();
	virtual void Drawer();
	virtual bool KeyHandler(WPARAM wParam);
protected:
	bool customColor = false;
	int fore, back;
	int width, height;
	const wchar_t* statusText;
	Console* console;
};

#include "Page.h"

