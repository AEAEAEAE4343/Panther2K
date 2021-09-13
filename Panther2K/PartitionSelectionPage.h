#pragma once
#include "Page.h"

typedef struct VOLUME_INFO
{
	wchar_t mountPoint[MAX_PATH + 1];
	wchar_t fileSystem[MAX_PATH + 1];
	wchar_t name[MAX_PATH + 1];
	long long totalBytes;
	long long bytesFree;
} *PVOLUME_INFO;

class PartitionSelectionPage : public Page
{
public:
	PartitionSelectionPage(const wchar_t* fileSystem, unsigned long long minimumBytesAvailable);
	void EnumeratePartitions();
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual void KeyHandler(WPARAM wParam) override;
};

