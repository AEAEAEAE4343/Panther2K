#pragma once
#include "Console.h"

class Win32Console : public Console
{
public:
	bool Init();
	void Init(bool createNewConsole);

	void SetPosition(long x, long y);
	POINT GetPosition();
	void SetSize(long columns, long rows);
	SIZE GetSize();

	void SetCursor(bool enabled, bool blinking);

	void Write(const wchar_t* string);
	void WriteLine(const wchar_t* string);
	KEY_EVENT_RECORD* Read(int count = 1);
	KEY_EVENT_RECORD* ReadLine();
	void Update();
	void Clear();
protected:
	void UpdateColorTable();
private:
	HANDLE hScreenBuffer;
	HANDLE hInputBuffer;
};

