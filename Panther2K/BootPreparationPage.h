#pragma once
#include "Page.h"
class BootPreparationPage : public Page
{
public:
	~BootPreparationPage();
	void PrepareBootFiles();
	void PrepareBootFilesNew();
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual bool KeyHandler(WPARAM wParam) override;
};

