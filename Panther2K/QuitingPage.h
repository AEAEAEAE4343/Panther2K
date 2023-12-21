#pragma once

#include "PopupPage.h"

class QuitingPage : public PopupPage
{
private:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual bool KeyHandler(WPARAM wParam) override;
};

