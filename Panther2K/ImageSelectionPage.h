#pragma once
#include "Page.h"
#include <vector>
#include <array>

class ImageSelectionPage : public Page
{
private:
	::std::vector<::std::array<wchar_t, 256>> FormattedStrings;
	int scrollIndex;
	int selectionIndex;
	int boxY;

	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual void KeyHandler(WPARAM wParam) override;
};

