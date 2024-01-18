#pragma once
#include "..\Page.h"
class PartitionGuidSelectionPage : public Page
{
protected:
	void InitPage();
	void DrawPage();
	void UpdatePage();
	void RunPage();
private:
	wchar_t enteredChars[37];
	POINT textLocation;
	int inputIndex;
};

