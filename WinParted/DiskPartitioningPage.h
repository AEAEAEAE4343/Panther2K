#pragma once
#include "Page.h"

class DiskPartitioningPage : public Page
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
};

