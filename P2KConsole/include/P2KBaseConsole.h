#pragma once

#include <windows.h>

#define VKEY unsigned int

#define CONSOLE_COLOR_BG 0
#define CONSOLE_COLOR_FG 1
#define CONSOLE_COLOR_ERROR 2
#define CONSOLE_COLOR_PROGBAR 3
#define CONSOLE_COLOR_LIGHTFG 4
#define CONSOLE_COLOR_DARKFG 5

struct COLOR
{
	unsigned char R, G, B;

	COLORREF ToColor()
	{
		return RGB(R, G, B);
	}
};

wchar_t* CleanString(const wchar_t* string);
wchar_t** SplitStringToLines(const wchar_t* string, int maxWidth, int* lineCount);

class Console
{
public:
	Console();
	virtual bool Init();

	virtual void SetPosition(long x, long y);
	virtual POINT GetPosition();
	virtual void SetSize(long columns, long rows);
	virtual SIZE GetSize();

	void SetColors(COLOR* colorTable);
	void SetBackgroundColor(COLOR color);
	void SetBackgroundColor(int index);
	COLOR GetBackgroundColor();
	void SetForegroundColor(COLOR color);
	void SetForegroundColor(int index);
	COLOR GetForegroundColor();

	void SetColorTable(COLOR* colorTable, int colorTableSize);

	virtual void SetCursor(bool enabled, bool blinking);

	virtual void Write(const wchar_t* string);
	virtual void WriteLine(const wchar_t* string);
	virtual KEY_EVENT_RECORD* Read(int count = 1);
	virtual KEY_EVENT_RECORD* ReadLine();
	virtual void Update();
	virtual void Clear();

	void DrawBox(int boxX, int boxY, int boxWidth, int boxHeight, bool useDouble);
	void DrawTextLeft(const wchar_t* string, int cx, int y);
	void DrawTextRight(const wchar_t* string, int cx, int y);
	void DrawTextCenter(const wchar_t* string, int cx, int y);
protected:
	int backColorIndex;
	int foreColorIndex;
	COLOR backColor;
	COLOR foreColor;
	COLOR* colorTable;
	int colorTableSize;

	virtual void UpdateColorTable();
};
