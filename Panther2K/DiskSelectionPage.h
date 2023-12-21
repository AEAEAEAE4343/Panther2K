#pragma once
#include "Page.h"

typedef struct DISK_INFO
{
	int diskNumber;
	wchar_t name[MAX_PATH];
	MEDIA_TYPE mediaType;
	unsigned long long size;
} *PDISK_INFO;

class DiskSelectionPage :
	public Page
{
protected:
	virtual void Init() override;
	virtual void Drawer() override;
	virtual void Redrawer() override;
	virtual bool KeyHandler(WPARAM wParam) override;
private:
	int boxY = 0;
	int selectionIndex = 0;
	int scrollIndex = 0;
	int diskCount = 0;
	DISK_INFO* diskInfo = 0;

};

