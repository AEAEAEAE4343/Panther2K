#pragma once
#include "Page.h"
class BootPreparationPage : public Page
{
public:
	~BootPreparationPage();
	void PrepareBootFiles();
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual void KeyHandler(WPARAM wParam) override;
};

