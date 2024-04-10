#include "pch.h"
#include "Win32Console.h"
#include <iostream>

bool rgbMode = false;

bool Win32Console::Init()
{
	//hScreenBuffer = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, NULL, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    printf("Enabling virtual terminal...");
    hInputBuffer = GetStdHandle(STD_INPUT_HANDLE);
    hScreenBuffer = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hScreenBuffer, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hScreenBuffer, mode);

    printf("Setting color palette...");
    SetBackgroundColor(0);
    wchar_t buffer[32];

    if (rgbMode)
    for (int i = 0; i < 6; i++)
    {
        swprintf(buffer, 32, L"\u001b]4;%d;rgb:%x/%x/%x\u001b\\",
            i, colorTable[i].R, colorTable[i].G, colorTable[i].B);
        WriteConsoleW(hScreenBuffer, buffer, lstrlenW(buffer), NULL, NULL);
    }
    
    return true;
}

void Win32Console::Init(bool createNewConsole)
{
    if (createNewConsole)
        AllocConsole();

    Init();
}

void Win32Console::SetPosition(long x, long y)
{
	SetConsoleCursorPosition(hScreenBuffer, { static_cast<short>(x), static_cast<short>(y) });
}

POINT Win32Console::GetPosition()
{
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo = { 0 };
	GetConsoleScreenBufferInfo(hScreenBuffer, &bufferInfo);
	return { bufferInfo.dwCursorPosition.X, bufferInfo.dwCursorPosition.Y };
}

void Win32Console::SetSize(long columns, long rows)
{
	/*SMALL_RECT DisplayArea = { 0, 0, 0, 0 };
	DisplayArea.Right = columns; DisplayArea.Bottom = rows;
	SetConsoleWindowInfo(hScreenBuffer, );*/
}

SIZE Win32Console::GetSize()
{
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo = { 0 };
	GetConsoleScreenBufferInfo(hScreenBuffer, &bufferInfo);
	return { bufferInfo.srWindow.Right - bufferInfo.srWindow.Left + 1, bufferInfo.srWindow.Bottom - bufferInfo.srWindow.Top + 1 };
}

void Win32Console::SetCursor(bool enabled, bool blinking)
{
    wchar_t buffer[32];
    swprintf(buffer, 32, L"\u001b[?25%s\u001b[?12%s", enabled ? L"h" : L"l", blinking ? L"h" : L"l");
    WriteConsoleW(hScreenBuffer, buffer, lstrlenW(buffer), NULL, NULL);
}

void Win32Console::Write(const wchar_t* string)
{
    int strlen = lstrlenW(string);
    wchar_t* stringwithcolorescape = (wchar_t*)malloc(sizeof(wchar_t) * (lstrlenW(string) + 40 + 1));

    if (rgbMode) 
    {
        swprintf(stringwithcolorescape, (lstrlenW(string) + 40 + 1), L"\u001b[38;2;%03d;%03d;%03dm\u001b[48;2;%03d;%03d;%03dm%s", foreColor.R, foreColor.G, foreColor.B, backColor.R, backColor.G, backColor.B, string);
        WriteConsoleW(hScreenBuffer, stringwithcolorescape, lstrlenW(stringwithcolorescape), NULL, NULL);
    }
    else 
    {
        const int ansiColorIndecesFg[6] = { 30, 37, 31, 33, 97, 30 };
        const int ansiColorIndecesBg[6] = { 40, 47, 41, 43, 107, 40 };
        swprintf(stringwithcolorescape, (lstrlenW(string) + 40 + 1), L"\u001b[%dm\u001b[%dm%s", ansiColorIndecesFg[foreColorIndex], ansiColorIndecesBg[backColorIndex], string);
        WriteConsoleW(hScreenBuffer, stringwithcolorescape, lstrlenW(stringwithcolorescape), NULL, NULL);
    }
    free(stringwithcolorescape);
}

void Win32Console::WriteLine(const wchar_t* string)
{
	wchar_t* stringwithlinebreak = (wchar_t*)malloc(sizeof(wchar_t) * (lstrlenW(string) + 2));

	memcpy(stringwithlinebreak, string, sizeof(wchar_t) * lstrlenW(string));
    stringwithlinebreak[lstrlenW(string)] = L'\n';
    stringwithlinebreak[lstrlenW(string) + 1] = L'\0';
	Write(stringwithlinebreak);

	free(stringwithlinebreak);
}

KEY_EVENT_RECORD* Win32Console::Read(int count)
{
    KEY_EVENT_RECORD* keys = (KEY_EVENT_RECORD*)malloc(sizeof(KEY_EVENT_RECORD) * count);
    if (!keys)
        return NULL;

    INPUT_RECORD input;
    DWORD eventsRead;
    int i = 0;
    while (i != count)
    {
        ReadConsoleInputW(hInputBuffer, &input, 1, &eventsRead);

        int j = GetLastError();

        if (input.EventType != KEY_EVENT)
            continue;

        if (!input.Event.KeyEvent.bKeyDown)
            continue;

        keys[i] = input.Event.KeyEvent;

        i++;
    }
	return keys;
}

KEY_EVENT_RECORD* Win32Console::ReadLine()
{
	return nullptr;
}

void Win32Console::Update() { }

void Win32Console::Clear()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SMALL_RECT scrollRect;
    COORD scrollTarget;
    CHAR_INFO fill;

    // Write a space to make sure the correct colors are selected
    Write(L" ");

    // Get the number of character cells in the current buffer.
    if (!GetConsoleScreenBufferInfo(hScreenBuffer, &csbi))
    {
        return;
    }

    // Scroll the rectangle of the entire buffer.
    scrollRect.Left = 0;
    scrollRect.Top = 0;
    scrollRect.Right = csbi.dwSize.X;
    scrollRect.Bottom = csbi.dwSize.Y;

    // Scroll it upwards off the top of the buffer with a magnitude of the entire height.
    scrollTarget.X = 0;
    scrollTarget.Y = (SHORT)(0 - csbi.dwSize.Y);

    // Fill with empty spaces with the buffer's default text attribute.
    fill.Char.UnicodeChar = L' ';
    fill.Attributes = 0;

    // Do the scroll
    ScrollConsoleScreenBufferW(hScreenBuffer, &scrollRect, NULL, scrollTarget, &fill);

    // Move the cursor to the top left corner too.
    csbi.dwCursorPosition.X = 0;
    csbi.dwCursorPosition.Y = 0;

    SetConsoleCursorPosition(hScreenBuffer, csbi.dwCursorPosition);
}

void Win32Console::UpdateColorTable()
{
}
