#pragma once

#include "Page.h"

class WelcomePage : public Page
{
private:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual void KeyHandler(WPARAM wParam) override;
};

