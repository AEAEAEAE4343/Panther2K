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
        colorTable[CONSOLE_COLOR_BG] = COLOR{ 0, 0, 170 };
        colorTable[CONSOLE_COLOR_FG] = COLOR{ 170, 170, 170 };
        colorTable[CONSOLE_COLOR_ERROR] = COLOR{ 170, 0, 0 };
        colorTable[CONSOLE_COLOR_PROGBAR] = COLOR{ 255, 255, 0 };
        colorTable[CONSOLE_COLOR_LIGHTFG] = COLOR{ 255, 255, 255 };
        colorTable[CONSOLE_COLOR_DARKFG] = COLOR{ 0, 0, 0 };
    }
}

void Console::Init()
{ 
    
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
