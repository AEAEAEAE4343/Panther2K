#include "pch.h"
#include "CustomConsole.h"
#include <iostream>
#include "Win32Console.h"

#define IDR_FONT_IBM                    3

#define WM_CREATECONSOLE WM_APP
#define WM_RESIZECONSOLE WM_APP + 1
#define WM_CLEARCONSOLE WM_APP + 2
#define WM_UPDATECONSOLE WM_APP + 3
#define WM_CREATEBUFFER WM_APP + 4

HFONT CustomConsole::font = 0;
long CustomConsole::fontWidth = 0;
long CustomConsole::fontHeight = 0;
bool CustomConsole::isWindowClassCreated = false;

DWORD CustomConsole::consoleThreadId = -1;
bool CustomConsole::isThreadRunning = false;

HBRUSH* brushes;
bool brushesCreated = false;

void __stdcall CustomConsole::ConsoleThreadProc(HANDLE pEvent)
{
	if (!isWindowClassCreated)
		if (!createWindowClass())
			return;
	
	if (!createFont())
		return;

	SetEvent(pEvent);

	ConsoleMessageLoop();
}

void CustomConsole::ConsoleMessageLoop()
{
	MSG msg;
	while (GetMessageW(&msg, NULL, NULL, NULL)) 
	{
		switch (msg.message)
		{
		case WM_CREATECONSOLE:
			CustomConsole* console = (CustomConsole*)msg.wParam;
			if (!brushes)
				console->UpdateColorTable();

			console->hBuf = 0;
			console->hdcBuf = 0;
			console->fullScreen = false;
			console->columns = 80;
			console->rows = 25;
			console->backColor = COLOR{ 0, 0, 170 };
			console->foreColor = COLOR{ 170, 170, 170 };
			console->backColorIndex = CONSOLE_COLOR_BG;
			console->foreColorIndex = CONSOLE_COLOR_FG;
			console->screenPointerX = 0;
			console->screenPointerY = 0;
			console->screenBufferUpdated = false;

			console->WindowHandle = CreateWindowW(L"Panther2KConsole", L"Panther Console Window", WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, 640, 400, nullptr, nullptr, GetModuleHandle(NULL), console);
			SetEvent((HANDLE)msg.lParam);
			if (!console->WindowHandle)
				break;;

			console->screenBuffer = (DISPLAYCHAR*)malloc(sizeof(DISPLAYCHAR) * console->columns * console->rows);
			ZeroMemory(console->screenBuffer, sizeof(DISPLAYCHAR) * console->columns * console->rows);
			PostMessageW(console->WindowHandle, WM_KEYDOWN, VK_HOME, 0);
			break;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

CustomConsole::CustomConsole() : Console()
{
	inputBuffer = new std::queue<KEY_EVENT_RECORD>();
	outputBuffer = new std::deque<int>();
}

void CustomConsole::Init()
{
	HANDLE event;
	if (!isThreadRunning)
	{
		event = CreateEventW(NULL, TRUE, FALSE, NULL);
		if (event == INVALID_HANDLE_VALUE)
			return;

		HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ConsoleThreadProc, event, 0, &consoleThreadId);
		if (hThread == INVALID_HANDLE_VALUE)
			return;

		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	event = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (event == INVALID_HANDLE_VALUE)
		return;

	PostThreadMessageW(consoleThreadId, WM_CREATECONSOLE, (WPARAM)this, (LPARAM)event);

	WaitForSingleObject(event, INFINITE);
	CloseHandle(event);
}

void CustomConsole::SetPosition(long x, long y)
{
	screenPointerX = x;
	screenPointerY = y;
}

POINT CustomConsole::GetPosition()
{
	return POINT{ screenPointerX, screenPointerY };
}

void CustomConsole::SetSize(long columns, long rows)
{
	SIZE t;
	t.cx = columns; t.cy = rows;
	SendMessage(WindowHandle, WM_RESIZECONSOLE, (WPARAM)&t, 0);
}

SIZE CustomConsole::GetSize()
{
	return SIZE{ columns, rows };
}

void CustomConsole::SetPixelScale(int scale) 
{
	RECT rect = { 0 };
	GetWindowRect(WindowHandle, &rect);
	rect.right = rect.left + (columns * fontWidth);
	rect.bottom = rect.top + (rows * fontHeight);
	DWORD style = GetWindowLongPtrW(WindowHandle, GWL_STYLE);
	DWORD exStyle = GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
	AdjustWindowRectEx(&rect, style, false, exStyle);
	SetWindowPos(WindowHandle, HWND_TOP, 0, 0, scale * (rect.right - rect.left), scale * (rect.bottom - rect.top), NULL);
}

void CustomConsole::SetCursor(bool enabled, bool blinking)
{

}

wchar_t ConvertCharacter(wchar_t charr)
{
	switch (charr)
	{
	case L'\x2554':
		return L'\xC9';
	case L'\x2557':
		return L'\xBB';
	case L'\x255A':
		return L'\xC8';
	case L'\x255D':
		return L'\xBC';
	case L'\x2550':
		return L'\xCD';
	case L'\x2551':
		return L'\xBA';
	
	case L'\x250C':
		return L'\xDA';
	case L'\x2510':
		return L'\xBF';
	case L'\x2514':
		return L'\xC0';
	case L'\x2518':
		return L'\xD9';
	case L'\x2500':
		return L'\xC4';
	case L'\x2502':
		return L'\xB3';

	case L'\x2022':
		return L'\x07';

	case L'\x2191':
		return L'\x18';
	case L'\x2193':
		return L'\x19';

	case L'\x255F':
		return L'\xC7';
	case L'\x2562':
		return L'\xB6';
	case L'\x2567':
		return L'\xCF';
	case L'\x2564':
		return L'\xD1';

	case L'\x2534':
		return L'\xC1';
	case L'\x252C':
		return L'\xC2';
	case L'\x251C':
		return L'\xC3';
	case L'\x2524':
		return L'\xB4';

	case L'\x2588':
		return L'\xDB';

	default:
		return charr;
	}
}

void CustomConsole::Write(const wchar_t* string)
{
	RECT rect;
	DISPLAYCHAR* characters = WcharPointerToDisplayCharPointer(string);

	int length = lstrlenW(string);
	for (int i = 0; i < length; i++)
	{
		if (characters[i].character == L'\n')
		{
			screenPointerX = 0;
			incrementY();
			continue;
		}
		else if (characters[i].character == L'\t')
		{
			for (int j = 0; j < 3; j++)
			{
				incrementX();
			}
			continue;
		}

		characters[i].character = ConvertCharacter(characters[i].character);

		int j = screenPointerX + (screenPointerY * columns);
		screenBuffer[j].backColor = characters[i].backColor;
		screenBuffer[j].character = characters[i].character;
		screenBuffer[j].foreColor = characters[i].foreColor;
		screenBuffer[j].backColorIndex = characters[i].backColorIndex;
		screenBuffer[j].foreColorIndex = characters[i].foreColorIndex;
		screenBuffer[j].updated = true;
		incrementX();
	}

	free(characters);

	PostMessageW(WindowHandle, WM_UPDATECONSOLE, 0, 0);
}

void CustomConsole::WriteLine(const wchar_t* string)
{
	Write(string);
	Write(L"\n");
}

KEY_EVENT_RECORD* CustomConsole::Read(int count)
{
	MSG msg;
	while (inputBuffer->size() < count) { Sleep(10); }

	KEY_EVENT_RECORD* buffer = (KEY_EVENT_RECORD*)malloc(sizeof(KEY_EVENT_RECORD) * count);
	if (!buffer)
		return NULL;

	for (int i = 0; i < count; i++)
	{
		buffer[i] = inputBuffer->front();
		inputBuffer->pop();
	}
	return buffer;
}

KEY_EVENT_RECORD* CustomConsole::ReadLine()
{
	return NULL;
}

void CustomConsole::Update()
{
	if (screenBufferUpdated)
		InvalidateRect(WindowHandle, NULL, FALSE);

	MSG msg;
	while (PeekMessageW(&msg, nullptr, NULL, NULL, PM_REMOVE | PM_QS_PAINT))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

void CustomConsole::Clear()
{
	SendMessageW(WindowHandle, WM_CLEARCONSOLE, 0, 0);
}

void CustomConsole::RedrawImmediately()
{
	RedrawWindow(WindowHandle, NULL, NULL, RDW_INVALIDATE);
}

void CustomConsole::UpdateColorTable()
{
	brushesCreated = false;
}

bool CustomConsole::createFont()
{
	HRSRC res = FindResourceW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_FONT_IBM), RT_RCDATA);
	HGLOBAL mem = LoadResource(GetModuleHandle(NULL), res);
	void* data = LockResource(mem);
	size_t len = SizeofResource(GetModuleHandle(NULL), res);
	DWORD nFonts = 0;
	HANDLE hFontRes = AddFontMemResourceEx(data, len, NULL, &nFonts);

	if (hFontRes == 0)
	{
		return false;
	}

	font = CreateFontW(16, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bm437 IBM VGA 8x16");
	//font = CreateFontW(12, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
	fontWidth = 8;
	fontHeight = 16;

	return font;
}

bool CustomConsole::createWindowClass()
{
	WNDCLASSEXW wcex = { 0 };

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = StaticWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszClassName = L"Panther2KConsole";

	if (RegisterClassExW(&wcex) == 0)
		return false;

	isWindowClassCreated = true;
	return true;
}

//Win32Console* console;

LRESULT CALLBACK CustomConsole::WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	HANDLE hOld;

	RECT rect;
	HFONT oldFont;
	HBRUSH oldBrush;
	HPEN oldPen;

	switch (Msg)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		exit(0);
	case WM_CREATE:
	case WM_CREATEBUFFER:
		hdc = BeginPaint(hWnd, &ps);
		//hBuf = CreateCompatibleBitmap(hdc, columns * fontWidth, rows * fontHeight);

		if (hdcBuf)
		{
			DeleteObject(hdcBuf);
			DeleteObject(hBuf);
		}

		hdcBuf = CreateCompatibleDC(hdc);
		bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapInfo.bmiHeader.biWidth = columns * fontWidth;
		bitmapInfo.bmiHeader.biHeight = -(rows * fontHeight);
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;
		bitmapInfo.bmiHeader.biBitCount = 24;
		hBuf = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, NULL, 0);

		/*bBuf = new Bitmap(columns * fontWidth, rows * fontHeight, PixelFormat32bppPARGB);
		fBuf = new Font(hdc, font);
		gBuf = Graphics::FromImage(bBuf);*/

		SetBkMode(hdcBuf, TRANSPARENT);
		SelectObject(hdcBuf, font);
		SelectObject(hdcBuf, hBuf);
		EndPaint(hWnd, &ps);
		break;
	case WM_SYSKEYDOWN:
		if (wParam != VK_F10) break;
	case WM_KEYDOWN:
		long style;
		long exStyle;
		switch (wParam)
		{
		case VK_F11:
			if (!fullScreen)
			{
				SetWindowLongPtrW(hWnd, GWL_STYLE, WS_VISIBLE | WS_CLIPSIBLINGS);
				HMONITOR hmon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFO mi = { sizeof(mi) };
				if (!GetMonitorInfo(hmon, &mi))
					break;
				SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, NULL);
				fullScreen = true;
				break;
			}
			fullScreen = false;
			SetWindowLongPtrW(hWnd, GWL_STYLE, WS_VISIBLE | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW);
		case VK_OEM_3:
		case VK_HOME:
			if (!fullScreen)
			{
				SetPixelScale(1);
			}
			break;
		default:
		{
			KEY_EVENT_RECORD keyEvent = { 0 };
			keyEvent.bKeyDown = TRUE;
			keyEvent.wVirtualKeyCode = wParam;
			keyEvent.wRepeatCount = lParam & 0xFFFF; // bit 0-15
			keyEvent.wVirtualScanCode = lParam & 0xFF0000;

			if (GetKeyState(VK_SHIFT) & 0x8000)
				keyEvent.dwControlKeyState |= SHIFT_PRESSED;
			if (GetKeyState(VK_LCONTROL) & 0x8000)
				keyEvent.dwControlKeyState |= LEFT_CTRL_PRESSED;
			if (GetKeyState(VK_RCONTROL) & 0x8000)
				keyEvent.dwControlKeyState |= RIGHT_CTRL_PRESSED;
			if (GetKeyState(VK_LMENU) & 0x8000)
				keyEvent.dwControlKeyState |= LEFT_ALT_PRESSED;
			if (GetKeyState(VK_RMENU) & 0x8000)
				keyEvent.dwControlKeyState |= RIGHT_ALT_PRESSED;

			if (GetKeyState(VK_CAPITAL) & 1)
				keyEvent.dwControlKeyState |= CAPSLOCK_ON;
			if (GetKeyState(VK_NUMLOCK) & 1)
				keyEvent.dwControlKeyState |= NUMLOCK_ON;
			if (GetKeyState(VK_SCROLL) & 1)
				keyEvent.dwControlKeyState |= SCROLLLOCK_ON;

			inputBuffer->push(keyEvent);
			break; 
		}
		}
		return 0;
	case WM_TIMER:
		PostQuitMessage(0);
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rect);
		StretchBlt(hdc, rect.left, rect.top, rect.right, rect.bottom, hdcBuf, 0, 0, columns * fontWidth, rows * fontHeight, SRCCOPY);
		EndPaint(hWnd, &ps);
		return 0;
	case WM_SHOWWINDOW:
		if (wParam)
			SendMessageW(hWnd, WM_KEYDOWN, VK_HOME, 0);
		break;
	case WM_SETCURSOR:
		if (fullScreen)
		{
			::SetCursor(LoadCursorW(NULL, NULL));
			return TRUE;
		}
		else 
		{
			::SetCursor(LoadCursorW(NULL, IDC_ARROW));
		}
		break;
	case WM_RESIZECONSOLE:
	{
		SIZE* consoleSize = (SIZE*)wParam;
		columns = consoleSize->cx;
		rows = consoleSize->cy;
		if (screenBuffer) free(screenBuffer);
		screenBuffer = (DISPLAYCHAR*)malloc(sizeof(DISPLAYCHAR) * columns * rows); 
		ZeroMemory(screenBuffer, sizeof(DISPLAYCHAR) * columns * rows);
		SendMessageW(WindowHandle, WM_CREATEBUFFER, VK_HOME, 0);
		break;
	}
	case WM_CLEARCONSOLE:
	{
		RECT rect;

		for (int i = 0; i < (columns * rows); i++)
		{
			screenBuffer[i].character = L' ';
			screenBuffer[i].backColor = backColor;
			screenBuffer[i].foreColor = foreColor;
			screenBuffer[i].updated = false;
			screenBuffer[i].backColorIndex = backColorIndex;
			screenBuffer[i].foreColorIndex = foreColorIndex;
		}

		rect.left = 0;
		rect.top = 0;
		rect.right = fontWidth * columns;
		rect.bottom = fontHeight * rows;

		HBRUSH bgBr = CreateSolidBrush(backColor.ToColor());
		FillRect(hdcBuf, &rect, bgBr);
		DeleteObject(bgBr);

		screenBufferUpdated = true;
		screenPointerX = 0;
		screenPointerY = 0;
		break;
	}
	case WM_UPDATECONSOLE:
		/*if (!console)
		{
			console = new Win32Console();
			console->Init();
		}*/
		if (!brushesCreated)
		{
			if (brushes) free(brushes);
			brushes = (HBRUSH*)malloc(sizeof(HBRUSH) * colorTableSize);
			for (int i = 0; i < colorTableSize; i++)
			{
				COLORREF ref = colorTable[i].ToColor();
				HBRUSH brush = CreateSolidBrush(ref);
				brushes[i] = brush;
			}
			brushesCreated = true;
		}
		for (int i = 0; i < columns * rows; i++)
		{
			// Find updated characters
			if (screenBuffer[i].updated)
			{
				// Immediately set updated to false.
				// There's a race condition he......
				screenBuffer[i].updated = false;

				int x = i % columns;
				int y = i / columns;

				/*console->SetPosition(x, y);
				if (screenBuffer[i].backColorIndex == -1)
					console->SetBackgroundColor(screenBuffer[i].backColor);
				else console->SetBackgroundColor(screenBuffer[i].backColorIndex);
				if (screenBuffer[i].foreColorIndex == -1)
					console->SetForegroundColor(screenBuffer[i].foreColor);
				else console->SetForegroundColor(screenBuffer[i].foreColorIndex);
				screenBuffer[i].reserved = 0;
				console->Write(&screenBuffer[i].character);*/

				rect.left = x * fontWidth;
				rect.top = y * fontHeight;
				rect.right = rect.left + fontWidth;
				rect.bottom = rect.top + fontHeight;

				HBRUSH bgBr;
			    if (screenBuffer[i].backColorIndex == -1)
					bgBr = CreateSolidBrush(screenBuffer[i].backColor.ToColor());
				else
				{
					colorTable[screenBuffer[i].backColorIndex].ToColor() == screenBuffer[i].backColor.ToColor();
					bgBr = brushes[screenBuffer[i].backColorIndex];
				}

				FillRect(hdcBuf, &rect, bgBr);
				if (screenBuffer[i].backColorIndex == -1)
					DeleteObject(bgBr);

				SetTextColor(hdcBuf, screenBuffer[i].foreColor.ToColor());
				//wprintf(L"drawing char '%s'...", &screenBuffer[i].character);
				DrawTextW(hdcBuf, &screenBuffer[i].character, 1, &rect, DT_LEFT | DT_TOP);
			}
		}
		RedrawImmediately();
		break;
	default:
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
}

void CustomConsole::incrementX()
{
	screenPointerX++;
	if (screenPointerX == columns)
	{
		screenPointerX = 0;
		incrementY();
	}
}

void CustomConsole::incrementY()
{
	screenPointerY++;
	if (screenPointerY == rows)
	{
		screenPointerY = 0;
	}
}

DISPLAYCHAR* CustomConsole::WcharPointerToDisplayCharPointer(const wchar_t* string)
{
	int length = lstrlenW(string);
	if (length <= 0)
		return NULL;

	DISPLAYCHAR* displayCharPointer = (DISPLAYCHAR*)malloc(sizeof(DISPLAYCHAR) * length);
	ZeroMemory(displayCharPointer, sizeof(DISPLAYCHAR) * length);

	if (displayCharPointer == 0)
		return NULL;

	for (int i = 0; i < length; i++)
	{
		if (string[i] == 0)
			continue;

		displayCharPointer[i].backColor = backColor;
		displayCharPointer[i].foreColor = foreColor;
		displayCharPointer[i].backColorIndex = backColorIndex;
		displayCharPointer[i].foreColorIndex = foreColorIndex;
		displayCharPointer[i].character = string[i];
	}

	return displayCharPointer;
}

LRESULT CALLBACK CustomConsole::StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	CustomConsole* classPointer;

	if (Msg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		classPointer = static_cast<CustomConsole*>(lpcs->lpCreateParams);
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(classPointer));
	}
	else
	{
		classPointer = reinterpret_cast<CustomConsole*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	}

	return classPointer->WndProc(hWnd, Msg, wParam, lParam);
}