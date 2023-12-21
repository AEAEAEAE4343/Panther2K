#pragma once
#include "Page.h"

class DiskSelectionPage : public Page
{
protected:
	void InitPage();
	void DrawPage();
	void UpdatePage();
	void RunPage();
private:
	int scrollIndex;
	int selectionIndex;
	int maxItems;
	int boxY;
};

