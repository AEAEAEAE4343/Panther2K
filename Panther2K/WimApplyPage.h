#pragma once
#include "Page.h"
class WimApplyPage : public Page
{
public:
	~WimApplyPage();
	int progress = 0;
	const wchar_t* filename = 0; 
	void WimMessageLoop();
	void Update(int prog);
	void Update(wchar_t* filename);
	void ApplyImage();
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual bool KeyHandler(WPARAM wParam) override;
};

