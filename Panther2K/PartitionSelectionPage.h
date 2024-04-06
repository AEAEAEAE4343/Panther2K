#pragma once
#include "Page.h"
#include <vector>

typedef struct VOLUME_INFO;

class PartitionSelectionPage : public Page
{
public:
	PartitionSelectionPage(const wchar_t* fileSystem, long long minimumSize, long long minimumBytesAvailable, int stringIndex, int displayIndex);
	void EnumeratePartitions();
	VOLUME_INFO GetSelectedVolume();
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual bool KeyHandler(WPARAM wParam) override;
private:
	int boxY = 0;
	int selectionIndex = 0;
	int scrollIndex = 0;
	std::vector<VOLUME_INFO> volumeInfo;
	int stringTableIndex;
	int dispIndex;
	bool showAll = false;
	struct
	{
		int partitionSize;
		int partitionFree;
		const wchar_t* fileSystem;
	} requirements;
};

