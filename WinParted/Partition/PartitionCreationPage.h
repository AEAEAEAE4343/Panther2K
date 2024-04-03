#pragma once
#include "..\Page.h"
#include "PartitionTable.h"

typedef struct SectorSpan {
	unsigned long long startSector;
	unsigned long long endSector;
	int partitionA;
	int partitionB;

	unsigned long long GetSize() {
		return endSector - startSector + 1;
	};
} *PSectorSpan;

class PartitionCreationPage : public Page
{
public:
	void InitPage();
	void DrawPage();
	void UpdatePage();
	void RunPage();
private:
	// Location
	SectorSpan* unallocatedSpans;
	int unallocatedSpanCount;
	int selectionIndex = 0;
	int scrollIndex = 0;
	int maxItems;

	// Size
	void ParseSize();
	bool enteringSize = false;
	wchar_t sizeString[20];
	unsigned long long size;

	int drawY = 0;
};

