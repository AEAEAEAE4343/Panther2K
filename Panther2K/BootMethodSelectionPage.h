#pragma once

#include "Page.h"

class BootMethodSelectionPage : public Page
{
public:
	~BootMethodSelectionPage();
private:
	bool legacy = false;
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual void KeyHandler(WPARAM wParam) override;
};

