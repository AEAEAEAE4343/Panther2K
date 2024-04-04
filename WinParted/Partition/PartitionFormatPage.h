#pragma once
#include "..\Page.h"

class PartitionFormatPage : public Page
{
public:
	void InitPage();
	void DrawPage();
	void UpdatePage();
	void RunPage();
private:
	int selectionIndex = 0;
	int scrollIndex = 0;
	int maxItems;

	bool enteringName = false;
	wchar_t nameString[64];

	wchar_t* supportedFileSystems[5]; /* at most 5 filesystems for now */
	int supportedFsCount = 0;
	int biggestFsName = 0;
	int drawY = 0;
};

