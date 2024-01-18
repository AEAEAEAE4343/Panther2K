#pragma once
#include "..\Page.h"
#include "..\CoreFunctions\PartitionManager.h"

class PartitionTypeSelectionPage : public Page
{
protected:
	void InitPage();
	void DrawPage();
	void UpdatePage();
	void RunPage();
private:
	void LoadItems(bool showAll);
	int scrollIndex;
	int selectionIndex;
	int maxItems;
	int totalItems;
	int boxY;
	PartitionType items[PartitionTypeCount];
	wchar_t enteredChars[5];
	POINT textLocation;
	int inputIndex;
	bool inputSelected;
	bool allItemsShown;
};

