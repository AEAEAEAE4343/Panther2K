#pragma once
#include "Page.h"
#include <vector>

typedef struct VOLUME_INFO
{
	wchar_t mountPoint[MAX_PATH + 1];
	wchar_t fileSystem[MAX_PATH + 1];
	wchar_t name[MAX_PATH + 1];
	wchar_t guid[MAX_PATH + 1];
	long long totalBytes;
	long long bytesFree;
	int diskNumber;
	int partitionNumber;
} *PVOLUME_INFO;

class PartitionSelectionPage : public Page
{
public:
	PartitionSelectionPage(const wchar_t* fileSystem, long long minimumSize, long long minimumBytesAvailable, int stringIndex);
	void EnumeratePartitions();
	VOLUME_INFO GetSelectedVolume();
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual void KeyHandler(WPARAM wParam) override;
private:
	int boxY = 0;
	int selectionIndex = 0;
	int scrollIndex = 0;
	std::vector<VOLUME_INFO> volumeInfo;
	int stringTableIndex;
	bool showAll = false;
	struct
	{
		int partitionSize;
		int partitionFree;
		const wchar_t* fileSystem;
	} requirements;
};

