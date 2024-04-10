#include "pch.h"
#include "Console.h"
#include <iostream>

FILE* stream;
Console::Console()
{
#ifdef _DEBUG
    errno_t err = freopen_s(&stream, "libpanther.log", "w", stdout);
    err;
#endif

    if (colorTable == NULL)
    {
        colorTableSize = 6;
        colorTable = (COLOR*)malloc(sizeof(COLOR) * colorTableSize);
        colorTable[CONSOLE_COLOR_BG] =      COLOR{ 0, 0, 170 };
        colorTable[CONSOLE_COLOR_FG] =      COLOR{ 170, 170, 170 };
        colorTable[CONSOLE_COLOR_ERROR] =   COLOR{ 170, 0, 0 };
        colorTable[CONSOLE_COLOR_PROGBAR] = COLOR{ 255, 255, 0 };
        colorTable[CONSOLE_COLOR_LIGHTFG] = COLOR{ 255, 255, 255 };
        colorTable[CONSOLE_COLOR_DARKFG] =  COLOR{ 0, 0, 0 };
    }
}

COLOR ParseColor(const wchar_t* text, bool* success)
{
    unsigned char rgb[3] = { 0 };
    *success = swscanf_s(text, L"%hhu,%hhu,%hhu", &rgb[0], &rgb[1], &rgb[2]) == 3;
    return COLOR{ rgb[0], rgb[1], rgb[2] };
}

bool Console::Init()
{
    wchar_t INIFile[MAX_PATH] = L"";
    wchar_t iniBuffer[MAX_PATH] = L"";
    GetFullPathNameW(L"libpanther.ini", MAX_PATH, INIFile, NULL);
    
    // Console
    GetPrivateProfileStringW(L"Console", L"ColorScheme", L"Windows Setup", iniBuffer, MAX_PATH, INIFile);
    if (!lstrcmpW(iniBuffer, L"Windows Setup"))
    {
        colorTable[CONSOLE_COLOR_BG] = COLOR{ 0, 0, 168 };
        colorTable[CONSOLE_COLOR_FG] = COLOR{ 168, 168, 168 };
        colorTable[CONSOLE_COLOR_ERROR] = COLOR{ 168, 0, 0 };
        colorTable[CONSOLE_COLOR_PROGBAR] = COLOR{ 255, 255, 0 };
        colorTable[CONSOLE_COLOR_LIGHTFG] = COLOR{ 255, 255, 255 };
        colorTable[CONSOLE_COLOR_DARKFG] = COLOR{ 0, 0, 0 };
    }
    else if (!lstrcmpW(iniBuffer, L"BIOS (Blue)"))
    {
        colorTable[CONSOLE_COLOR_BG] = COLOR{ 0, 0, 170 };
        colorTable[CONSOLE_COLOR_FG] = COLOR{ 170, 170, 170 };
        colorTable[CONSOLE_COLOR_ERROR] = COLOR{ 170, 0, 0 };
        colorTable[CONSOLE_COLOR_PROGBAR] = COLOR{ 255, 255, 0 };
        colorTable[CONSOLE_COLOR_LIGHTFG] = COLOR{ 255, 255, 255 };
        colorTable[CONSOLE_COLOR_DARKFG] = COLOR{ 0, 0, 0 };
    }
    else if (!lstrcmpW(iniBuffer, L"BIOS (Black)"))
    {
        colorTable[CONSOLE_COLOR_BG] = COLOR{ 0, 0, 0 };
        colorTable[CONSOLE_COLOR_FG] = COLOR{ 170, 170, 170 };
        colorTable[CONSOLE_COLOR_ERROR] = COLOR{ 170, 0, 0 };
        colorTable[CONSOLE_COLOR_PROGBAR] = COLOR{ 255, 255, 0 };
        colorTable[CONSOLE_COLOR_LIGHTFG] = COLOR{ 255, 255, 255 };
        colorTable[CONSOLE_COLOR_DARKFG] = COLOR{ 0, 0, 0 };
    }
    else if (!lstrcmpW(iniBuffer, L"Custom"))
    {
        bool success = false;
        GetPrivateProfileStringW(L"Console", L"BackgroundColor", L"Fail", iniBuffer, MAX_PATH, INIFile);
        colorTable[CONSOLE_COLOR_BG] = ParseColor(iniBuffer, &success);
        if (!success) goto fail;
        GetPrivateProfileStringW(L"Console", L"ForegroundColor", L"Fail", iniBuffer, MAX_PATH, INIFile);
        colorTable[CONSOLE_COLOR_FG] = ParseColor(iniBuffer, &success);
        if (!success) goto fail;
        GetPrivateProfileStringW(L"Console", L"ErrorColor", L"Fail", iniBuffer, MAX_PATH, INIFile);
        colorTable[CONSOLE_COLOR_ERROR] = ParseColor(iniBuffer, &success);
        if (!success) goto fail;
        GetPrivateProfileStringW(L"Console", L"ProgressBarColor", L"Fail", iniBuffer, MAX_PATH, INIFile);
        colorTable[CONSOLE_COLOR_PROGBAR] = ParseColor(iniBuffer, &success);
        if (!success) goto fail;
        GetPrivateProfileStringW(L"Console", L"LightForegroundColor", L"Fail", iniBuffer, MAX_PATH, INIFile);
        colorTable[CONSOLE_COLOR_LIGHTFG] = ParseColor(iniBuffer, &success);
        if (!success) goto fail;
        GetPrivateProfileStringW(L"Console", L"DarkForegroundColor", L"Fail", iniBuffer, MAX_PATH, INIFile);
        colorTable[CONSOLE_COLOR_DARKFG] = ParseColor(iniBuffer, &success);
        if (!success) goto fail;
    fail:
        MessageBoxW(NULL, L"The color scheme specified in libpanther.ini is invalid. This application will exit.", L"LibPanther console", MB_OK | MB_ICONERROR);
        return false;
    }
    else
    {
        MessageBoxW(NULL, L"The value Console\\ColorScheme specified in libpanther.ini is invalid. This application will exit.", L"LibPanther console", MB_OK | MB_ICONERROR);
        return false;
    }

    UpdateColorTable();
    int columns = GetPrivateProfileIntW(L"Console", L"Columns", 80, INIFile);
    int rows = GetPrivateProfileIntW(L"Console", L"Rows", 25, INIFile);
    SetSize(columns, rows);

    int fontHeight = GetPrivateProfileIntW(L"Console", L"FontHeight", 16, INIFile);
    if (fontHeight == -1)
    {
        MessageBoxW(NULL, L"The value Console\\FontHeight specified in libpanther.ini is invalid. This application will exit.", L"LibPanther console", MB_OK | MB_ICONERROR);
        return false;
    }
}

void Console::SetPosition(long, long) { }
POINT Console::GetPosition() { return { 0 }; }
void Console::SetSize(long, long) { }
SIZE Console::GetSize() { return { 0 }; }

void Console::Write(const wchar_t* string) { }
void Console::WriteLine(const wchar_t* string) { }
KEY_EVENT_RECORD* Console::Read(int count) { return NULL; }
KEY_EVENT_RECORD* Console::ReadLine() { return NULL; }
void Console::Clear() { }

void Console::SetColors(COLOR* colorTable)
{
    this->colorTable = colorTable;
}

void Console::SetBackgroundColor(COLOR color)
{
    backColor = color;
    backColorIndex = -1;
}

void Console::SetBackgroundColor(int index)
{
    SetBackgroundColor(colorTable[min(index, colorTableSize - 1)]);
    backColorIndex = min(index, colorTableSize - 1);
    //wprintf(L"Backcolor index: %d\n", backColorIndex);
}

COLOR Console::GetBackgroundColor()
{
    return backColor;
}

void Console::SetForegroundColor(COLOR color)
{
    foreColor = color;
    foreColorIndex = -1;
}

void Console::SetForegroundColor(int index)
{
    SetForegroundColor(colorTable[min(index, colorTableSize - 1)]);
    foreColorIndex = min(index, colorTableSize - 1);
}

COLOR Console::GetForegroundColor()
{
    return foreColor;
}

void Console::SetColorTable(COLOR* colorTable, int colorTableSize)
{
    if (this->colorTable) free(this->colorTable);
    this->colorTable = colorTable;
    this->colorTableSize = colorTableSize;
    UpdateColorTable();
}

void Console::SetCursor(bool enabled, bool blinking) { }

wchar_t* CleanString(const wchar_t* string)
{
    auto isNonTextChar = [](wchar_t a) {
        return a == L' ' ||
            a == L'\r' ||
            a == L'\n';
    };

    int strLen = lstrlenW(string);
    wchar_t* newString = (wchar_t*)malloc(sizeof(wchar_t) * (strLen + 1));

    int newStrPtr = 0;
    int lastNonWsp = -2;
    for (int oldStrPtr = 0; oldStrPtr < strLen; oldStrPtr++)
    {
        if (isNonTextChar(string[oldStrPtr]) && newStrPtr - lastNonWsp > 1)
            continue;
        else if (!isNonTextChar(string[oldStrPtr]))
            lastNonWsp = newStrPtr;

        newString[newStrPtr] = string[oldStrPtr];
        newStrPtr++;
    }
    newString[lastNonWsp + 1] = '\x00';

    return newString;
}

wchar_t** SplitStringToLines(const wchar_t* string, int maxWidth, int* lineCount)
{
    int strLen = lstrlenW(string);
    if (strLen <= 0)
        return 0;

    wchar_t* wordList = (wchar_t*)malloc(sizeof(wchar_t) * (strLen + 1));
    wchar_t* endValues = (wchar_t*)malloc(sizeof(wchar_t) * (strLen + 1));
    if (!wordList || !endValues)
        return 0;
    wordList[strLen] = L'\x0';
    endValues[strLen] = L'\x0';

    // Split by words
    for (int i = 0; i < (strLen + 1); i++)
    {
        wordList[i] = string[i];
        if (wordList[i] == L' ')
            wordList[i] = L'\0';
    }

    // Determine where to put a space, a null character or nothing
    int lineWidth = -1;
    (*lineCount) = 1;
    for (int i = 0; i < (strLen + 1); i += lstrlenW(wordList + i) + 1)
    {
        int wordSize = lstrlenW(wordList + i);
        if (lineWidth == -1)
        {
            memcpy(endValues + i, wordList + i, sizeof(wchar_t) * wordSize);
        }
        else
        {
            if (lineWidth + wordSize + 1 <= maxWidth)
                endValues[i - 1] = L' ';
            else
            {
                endValues[i - 1] = L'\x0';
                lineWidth = -1;
                (*lineCount)++;
            }
            memcpy(endValues + i, wordList + i, sizeof(wchar_t) * wordSize);
        }
        lineWidth += wordSize + 1;
    }


    // Get pointers to all strings
    wchar_t** returnValue = (wchar_t**)malloc((*lineCount) * sizeof(wchar_t*));
    for (int i = 0, j = 0; i < (strLen + 1); i += lstrlenW(endValues + i) + 1, j++)
        returnValue[j] = endValues + i;

    // Remove temporary word list
    free(wordList);

    return returnValue;
}

void Console::DrawBox(int boxX, int boxY, int boxWidth, int boxHeight, bool useDouble)
{
    for (int i = 0; i < boxHeight; i++)
    {
        SetPosition(boxX, boxY + i);
        for (int j = 0; j < boxWidth; j++)
            Write(L" ");
    }

    SetPosition(boxX, boxY);
    Write(useDouble ? L"\x2554" : L"\x250C");
    SetPosition(boxX + (boxWidth - 1), boxY);
    Write(useDouble ? L"\x2557" : L"\x2510");
    SetPosition(boxX, boxY + (boxHeight - 1));
    Write(useDouble ? L"\x255A" : L"\x2514");
    SetPosition(boxX + (boxWidth - 1), boxY + (boxHeight - 1));
    Write(useDouble ? L"\x255D" : L"\x2518");

    SetPosition(boxX + 1, boxY);
    for (int i = 0; i < boxWidth - 2; i++)
        Write(useDouble ? L"\x2550" : L"\x2500");

    SetPosition(boxX + 1, boxY + boxHeight - 1);
    for (int i = 0; i < boxWidth - 2; i++)
        Write(useDouble ? L"\x2550" : L"\x2500");

    for (int i = 0; i < boxHeight - 2; i++)
    {
        SetPosition(boxX, boxY + 1 + i);
        Write(useDouble ? L"\x2551" : L"\x2502");
    }

    for (int i = 0; i < boxHeight - 2; i++)
    {
        SetPosition(boxX + boxWidth - 1, boxY + 1 + i);
        Write(useDouble ? L"\x2551" : L"\x2502");
    }
}

void Console::DrawTextLeft(const wchar_t* string, int cx, int y)
{
    int lineCount = 0;
    wchar_t** lines = SplitStringToLines(string, cx, &lineCount);

    int x = (GetSize().cx - cx) / 2;
    for (int i = 0; i < lineCount; i++, y++)
    {
        SetPosition(x, y);
        Write(lines[i]);
    }

    free(lines);
}

void Console::Update() { }

void Console::DrawTextRight(const wchar_t* string, int cx, int y)
{
    int lineCount = 0;
    wchar_t** lines = SplitStringToLines(string, cx, &lineCount);

    int paragraphX = (GetSize().cx - cx) / 2;
    for (int i = 0; i < lineCount; i++, y++)
    {
        int x = paragraphX + (cx - lstrlenW(lines[i]));
        SetPosition(x, y);
        Write(lines[i]);
    }

    free(lines);
}

void Console::DrawTextCenter(const wchar_t* string, int cx, int y)
{
    int lineCount = 0;
    wchar_t** lines = SplitStringToLines(string, cx, &lineCount);

    int paragraphX = (GetSize().cx - cx) / 2;
    for (int i = 0; i < lineCount; i++, y++)
    {
        int x = paragraphX + ((cx - lstrlenW(lines[i])) / 2);
        SetPosition(x, y);
        Write(lines[i]);
    }
    
    free(lines);
}

void Console::UpdateColorTable()
{
}
