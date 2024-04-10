#pragma once
#include "P2KBaseConsole.h"
#include <queue>

namespace Gdiplus
{
	class Font;
	class Bitmap;
	class Graphics;
};

struct DISPLAYCHAR
{
	wchar_t character;
	wchar_t reserved;
	int backColorIndex;
	int foreColorIndex;
	COLOR backColor;
	COLOR foreColor;
	bool updated;
};

class CustomConsole : public Console
{
public:
	// Constructor
	CustomConsole();

	bool Init();
	void SetPosition(long x, long y);
	POINT GetPosition();
	void SetSize(long columns, long rows);
	SIZE GetSize();

	void SetPixelScale(int scale);

	void SetCursor(bool enabled, bool blinking);

	void Write(const wchar_t* string);
	void WriteLine(const wchar_t* string);
	KEY_EVENT_RECORD* Read(int count = 1);
	KEY_EVENT_RECORD* ReadLine();
	void Update();
	void Clear();

	//void ReloadSettings(long columns, long rows, HFONT font);
	void RedrawImmediately();
	HWND WindowHandle;
protected:
	void UpdateColorTable();
private:
	// Message loop
	static DWORD consoleThreadId;
	static bool isThreadRunning;
	static void __stdcall ConsoleThreadProc(HANDLE pEvent);
	static void ConsoleMessageLoop();
	static SIZE newConsoleSize;

	// Output information
	BITMAPINFO bitmapInfo;
	void* bitmapBits;
	HBITMAP hBuf;
	HDC hdcBuf;
	bool fullScreen;

	// Font information
	static HFONT font;
	static long fontWidth;
	static long fontHeight;
	static bool createFont();

	// Screen buffer
	long columns;
	long rows;
	long screenPointerX;
	long screenPointerY;
	bool screenBufferUpdated;
	DISPLAYCHAR* screenBuffer;
	std::queue<KEY_EVENT_RECORD>* inputBuffer;
	std::deque<int>* outputBuffer;

	// Window
	static bool isWindowClassCreated;
	static bool createWindowClass();
	LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	// Helpers
	void incrementX();
	void incrementY();
	DISPLAYCHAR* WcharPointerToDisplayCharPointer(const wchar_t* string);
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
};

