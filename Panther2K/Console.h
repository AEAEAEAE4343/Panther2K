#pragma once

#include <windows.h>

namespace Gdiplus
{
	class Font;
	class Bitmap;
	class Graphics;
};

struct COLOR
{
	unsigned char R, G, B;

	COLORREF ToColor()
	{
		return RGB(R, G, B);
	}
};

struct DISPLAYCHAR 
{
	wchar_t character;
	COLOR backColor;
	COLOR foreColor;
};

wchar_t** SplitStringToLines(const wchar_t* string, int maxWidth, int* lineCount);

class Console
{
public:
	HWND WindowHandle;

	static void Init();
	static Console* CreateConsole();
	void Clear();
    void Write(const wchar_t* string);
    void WriteLine(const wchar_t* string);
	void RedrawImmediately();

	void SetPosition(long x, long y);
	POINT GetPosition();
	void SetSize(long rows, long columns);
	SIZE GetSize();
	void SetBackgroundColor(COLOR color);
	COLOR GetBackgroundColor();
	void SetForegroundColor(COLOR color);
	COLOR GetForegroundColor();
	long GetThreadId();

	void WriteToConhost();

	DISPLAYCHAR* WcharPointerToDisplayCharPointer(const wchar_t* string);
	void ReloadSettings(long columns, long rows, HFONT font);

	void (*OnKeyPress)(WPARAM);
private:
	/*Gdiplus::Bitmap* bBuf;
	Gdiplus::Graphics* gBuf;
	Gdiplus::Font* fBuf;*/
	HBITMAP hBuf;
	HDC hdcBuf;
	bool fullScreen;

	COLOR backColor;
	COLOR foreColor;
	
	long columns;
	long rows;
	long screenPointerX;
	long screenPointerY;
	DISPLAYCHAR* screenBuffer;
	char* screenBufferUpdateFlags;
	bool screenBufferUpdated;
	long threadId;

	static bool isWindowClassCreated;
	static Console* lastCreatedConsole;
	static HFONT font;
	static long fontWidth;
	static long fontHeight;

	Console();
	void incrementX();
	void incrementY();
	static bool createWindowClass();

	LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
};

