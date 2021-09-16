#include "Console.h"
#include <iostream>

#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

bool Console::isWindowClassCreated = false;
Console* Console::lastCreatedConsole = NULL;
HFONT Console::font = NULL;
long Console::fontWidth = 0;
long Console::fontHeight = 0;

void SetBit(char* bitfield, int index)
{
    int bytePosition = index / 8;
    int bitPosition = index % 8;
    char flag = 1;
    flag <<= bitPosition;
    bitfield[bytePosition] |= flag;
}

void ClearBit(char* bitfield, int index)
{
    int bytePosition = index / 8;
    int bitPosition = index % 8;
    char flag = 1;
    flag <<= bitPosition;
    flag = -flag;
    bitfield[bytePosition] &= flag;
}

bool IsSet(char* bitfield, int index)
{
    int bytePosition = index / 8;
    int bitPosition = index % 8;
    char flag = 1;
    flag <<= bitPosition;
    return (bitfield[bytePosition] & flag) != FALSE;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Failure

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);
    return -1;  // Failure
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

    // Remove temporary word list
    free(wordList);

    // Get pointers to all strings
    wchar_t** returnValue = (wchar_t**)malloc((*lineCount) * sizeof(wchar_t*));
    for (int i = 0, j = 0; i < (strLen + 1); i += lstrlenW(endValues + i) + 1, j++)
        returnValue[j] = endValues + i;

    return returnValue;
}

void Console::Init()
{
    font = CreateFontW(16, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bm437 IBM VGA 8x16");
    //font = CreateFontW(12, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Windows Setup");
    fontWidth = 8;
    fontHeight = 16;
}

Console* Console::CreateConsole()
{
    if (!isWindowClassCreated)
        if (!createWindowClass())
            return NULL;

    Console* console = new Console();
    HWND hWnd = CreateWindowW(L"Panther2KConsole", L"Panther2K Windows Setup", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 400, nullptr, nullptr, GetModuleHandle(NULL), console);
    
    if (!hWnd)
        return NULL;

    console->threadId = GetCurrentThreadId();
    console->WindowHandle = hWnd;
    console->OnKeyPress = 0;
    console->Clear();
    
    lastCreatedConsole = console;

    SendMessageW(hWnd, WM_KEYDOWN, VK_HOME, 0);
    UpdateWindow(hWnd);

    return console;
}

void Console::Clear()
{
    RECT rect;

    for (int i = 0; i < (columns * rows); i++)
    {
        screenBuffer[i].character = L' ';
        screenBuffer[i].backColor = backColor;
        screenBuffer[i].foreColor = foreColor;
        SetBit(screenBufferUpdateFlags, i);

        int x = i % columns;
        int y = i / columns;

        /*Color cb(screenBuffer[i].backColor.R, screenBuffer[i].backColor.G, screenBuffer[i].backColor.B);
        SolidBrush bb(cb);
        Color cf(screenBuffer[i].foreColor.R, screenBuffer[i].foreColor.G, screenBuffer[i].foreColor.B);
        SolidBrush bf(cf);
        Rect r(x * fontWidth, y * fontHeight, fontWidth, fontHeight);
        Point p(r.GetLeft(), r.GetTop());
        PointF p2(r.GetLeft(), r.GetTop());*/

        rect.left = x * fontWidth;
        rect.top = y * fontHeight;
        rect.right = rect.left + fontWidth;
        rect.bottom = rect.top + fontHeight;

        HBRUSH bgBr = CreateSolidBrush(screenBuffer[i].backColor.ToColor());
        FillRect(hdcBuf, &rect, bgBr);
        DeleteObject(bgBr);

        //((Graphics*)gBuf)->FillRectangle(&bb, r);
        //((Graphics*)gBuf)->DrawString(&screenBuffer[i].character, 1, (Font*)fBuf, p2, &bf);

        SetTextColor(hdcBuf, screenBuffer[i].foreColor.ToColor());
        DrawText(hdcBuf, &screenBuffer[i].character, 1, &rect, DT_LEFT | DT_TOP);

        ClearBit(screenBufferUpdateFlags, i);
    }

    screenBufferUpdated = true;

    screenPointerX = 0;
    screenPointerY = 0;
}

void Console::Write(const wchar_t* string)
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
        
        int j = screenPointerX + (screenPointerY * columns);
        screenBuffer[j].backColor = characters[i].backColor;
        screenBuffer[j].character = characters[i].character;
        screenBuffer[j].foreColor = characters[i].foreColor;
        //screenBuffer[screenPointerX + (screenPointerY * columns)] = characters[i];
        SetBit(screenBufferUpdateFlags, screenPointerX + (screenPointerY * columns));

        int x = screenPointerX;
        int y = screenPointerY;

        /*Color cb(screenBuffer[i].backColor.R, screenBuffer[i].backColor.G, screenBuffer[i].backColor.B);
        SolidBrush bb(cb);
        Color cf(screenBuffer[i].foreColor.R, screenBuffer[i].foreColor.G, screenBuffer[i].foreColor.B);
        SolidBrush bf(cf);
        Rect r(x * fontWidth, y * fontHeight, fontWidth, fontHeight);
        Point p(r.GetLeft(), r.GetTop());
        PointF p2(r.GetLeft(), r.GetTop());*/

        rect.left = x * fontWidth;
        rect.top = y * fontHeight;
        rect.right = rect.left + fontWidth;
        rect.bottom = rect.top + fontHeight;

        HBRUSH bgBr = CreateSolidBrush(screenBuffer[j].backColor.ToColor());
        FillRect(hdcBuf, &rect, bgBr);
        DeleteObject(bgBr);

        //((Graphics*)gBuf)->FillRectangle(&bb, r);
        //((Graphics*)gBuf)->DrawString(&screenBuffer[i].character, 1, (Font*)fBuf, p2, &bf);

        SetTextColor(hdcBuf, screenBuffer[j].foreColor.ToColor());
        DrawText(hdcBuf, &screenBuffer[j].character, 1, &rect, DT_LEFT | DT_TOP);

        ClearBit(screenBufferUpdateFlags, j);

        incrementX();
    }

    free(characters);

    screenBufferUpdated = true;
}

void Console::WriteLine(const wchar_t* string)
{
    Write(string);
    Write(L"\n");
}

int Console::WriteLinesLeft(const wchar_t* string, int maxWidth)
{
    int lineCount = 0;
    wchar_t** strings = SplitStringToLines(string, maxWidth, &lineCount);

    int x = (columns / 2) - (maxWidth / 2), y = screenPointerY;
    for (int i = 0; i < lineCount; i++, y++)
    {
        SetPosition(x, y);
        Write(strings[i]);
    }
    SetPosition(x, y + 1);

    return lineCount;
}

int Console::WriteLinesCentered(const wchar_t* string, int maxWidth)
{
    int lineCount = 0;
    wchar_t** strings = SplitStringToLines(string, maxWidth, &lineCount);

    int y = screenPointerY;
    for (int i = 0; i < lineCount; i++, y++)
    {
        int x = (columns / 2) - (lstrlenW(strings[i]) / 2);
        SetPosition(x, y);
        Write(strings[i]);
    }
    SetPosition((columns / 2) - (maxWidth / 2), y + 1);

    return lineCount;
}

void Console::RedrawImmediately()
{
    if (GetCurrentThreadId() != threadId)
        RedrawWindow(WindowHandle, NULL, NULL, RDW_INVALIDATE);
    else
        RedrawWindow(WindowHandle, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

void Console::SetPosition(long x, long y)
{
    screenPointerX = x;
    screenPointerY = y;
}

POINT Console::GetPosition()
{
    return POINT{ screenPointerX, screenPointerY };
}

void Console::SetSize(long rows, long columns)
{
    this->rows = rows;
    this->columns = columns;
}

SIZE Console::GetSize()
{
    return SIZE{ columns, rows };
}

void Console::SetBackgroundColor(COLOR color)
{
    backColor = color;
}

COLOR Console::GetBackgroundColor()
{
    return backColor;
}

void Console::SetForegroundColor(COLOR color)
{
    foreColor = color;
}

COLOR Console::GetForegroundColor()
{
    return foreColor;
}

long Console::GetThreadId()
{
    return threadId;
}

void Console::WriteToConhost()
{
    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < columns; x++)
        {
            std::cout << &screenBuffer[y * columns + x];
        }
        std::cout << std::endl;
    }
}

DISPLAYCHAR* Console::WcharPointerToDisplayCharPointer(const wchar_t* string)
{
    int length = lstrlenW(string);
    if (length <= 0)
        return NULL;

    DISPLAYCHAR* displayCharPointer = (DISPLAYCHAR*)malloc(sizeof(DISPLAYCHAR) * length);

    if (displayCharPointer == 0)
        return NULL;

    for (int i = 0; i < length; i++)
    {
        if (string[i] == 0)
            continue;

        displayCharPointer[i].backColor = backColor;
        displayCharPointer[i].foreColor = foreColor;
        displayCharPointer[i].character = string[i];
    }

    return displayCharPointer;
}

Console::Console()
{
    /*bBuf = 0;
    gBuf = 0;
    fBuf = 0;*/
    hBuf = 0;
    hdcBuf = 0;
    threadId = 0;
    fullScreen = false;
    columns = 80;
    rows = 25;
    backColor = COLOR{ 0, 0, 170 };
    foreColor = COLOR{ 170, 170, 170 };
    screenPointerX = 0;
    screenPointerY = 0;
    screenBuffer = (DISPLAYCHAR*)malloc(sizeof(DISPLAYCHAR) * columns * rows);
    screenBufferUpdateFlags = (char*)malloc((columns * rows) / sizeof(char));
    screenBufferUpdated = false;
    OnKeyPress = 0;

    lastCreatedConsole = 0;
    WindowHandle = 0;
}

void Console::ReloadSettings(long c, long r, HFONT fontt)
{
    DISPLAYCHAR* oldScreenBuffer = screenBuffer;
    screenBuffer = (DISPLAYCHAR*)malloc(sizeof(DISPLAYCHAR) * c * r);
    for (int x = 0; x < min(columns, c); x++)
        for (int y = 0; y < min(rows, r); y++)
            screenBuffer[y * c + x] = oldScreenBuffer[y * columns + x];

    free(oldScreenBuffer);

    free(screenBufferUpdateFlags);
    screenBufferUpdateFlags = (char*)malloc((c * r) / sizeof(char));
    screenBufferUpdated = false;

    columns = c;
    rows = r;
    backColor = COLOR{ 0, 0, 170 };
    foreColor = COLOR{ 170, 170, 170 };
    screenPointerX = 0;
    screenPointerY = 0;

    if (font)
        DeleteObject(font);

    font = fontt;

    DeleteObject(hBuf);
    DeleteDC(hdcBuf);
    hBuf = 0;
    hdcBuf = 0;
    /*if (bBuf) bBuf->~Bitmap();
    if (gBuf) gBuf->~Graphics();
    if (fBuf) fBuf->~Font();*/

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(WindowHandle, &ps);

    HGDIOBJ old = SelectObject(hdc, font);
    /*TEXTMETRICW tm = { 0 };
    GetTextMetricsW(hdc, &tm);
    fontWidth = tm.tmAveCharWidth;
    fontHeight = tm.tmHeight;*/
    SIZE fSize = { 0 };
    GetTextExtentPoint32W(hdc, L"", 1, &fSize);
    fontWidth = fSize.cx;
    fontHeight = fSize.cy;
    SelectObject(hdc, old);
    
    /*BITMAPINFO bi = { 0 };
    BITMAPINFOHEADER& bih = bi.bmiHeader;
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = columns * fontWidth, bih.biHeight = -(rows * fontHeight);
    bih.biPlanes = 1, bih.biBitCount = 24;
    bih.biCompression = BI_RGB;
    void* bits;*/
    //hBuf = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    //hBuf = CreateBitmap(columns * fontWidth, rows * fontHeight, 1, 24, NULL);
    hBuf = CreateCompatibleBitmap(hdc, columns * fontWidth, rows * fontHeight);
    hdcBuf = CreateCompatibleDC(hdc);

    //bBuf = new Bitmap(columns * fontWidth, rows * fontHeight, PixelFormat32bppPARGB);
    //fBuf = new Font(hdc, font);
    //gBuf = Graphics::FromImage(bBuf);

    SetBkMode(hdcBuf, TRANSPARENT);
    SelectObject(hdcBuf, font);
    SelectObject(hdcBuf, hBuf);
    EndPaint(WindowHandle, &ps);

    SendMessage(WindowHandle, WM_KEYDOWN, VK_HOME, 0);
}

void Console::incrementX()
{
    screenPointerX++;
    if (screenPointerX == columns)
    {
        screenPointerX = 0;
        incrementY();
    }
}

void Console::incrementY()
{
    screenPointerY++;
    if (screenPointerY == rows)
    {
        screenPointerY = 0;
    }
}

bool Console::createWindowClass()
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

    return true;
}

LRESULT CALLBACK Console::WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    HANDLE hOld;

    RECT rect;
    HFONT oldFont;
    HBRUSH oldBrush;
    HPEN oldPen;
    Rect rectGdi;

    switch (Msg)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
    case WM_CREATE:
        hdc = BeginPaint(hWnd, &ps);
        hBuf = CreateCompatibleBitmap(hdc, columns * fontWidth, rows * fontHeight);
        hdcBuf = CreateCompatibleDC(hdc);

        /*bBuf = new Bitmap(columns * fontWidth, rows * fontHeight, PixelFormat32bppPARGB);
        fBuf = new Font(hdc, font);
        gBuf = Graphics::FromImage(bBuf);*/

        SetBkMode(hdcBuf, TRANSPARENT);
        SelectObject(hdcBuf, font);
        SelectObject(hdcBuf, hBuf);
        EndPaint(hWnd, &ps);
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
        case VK_HOME:
            rect = { 0 };
            GetWindowRect(hWnd, &rect);
            rect.right = rect.left + (columns * fontWidth);
            rect.bottom = rect.top + (rows * fontHeight);
            style = GetWindowLongPtrW(hWnd, GWL_STYLE);
            exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
            AdjustWindowRectEx(&rect, style, false, exStyle);
            SetWindowPos(hWnd, HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top, NULL);
            break;
        default:
            if (OnKeyPress != 0)
                OnKeyPress(wParam);
            break;
        }
        return 0;
    case WM_TIMER:
        PostQuitMessage(0);
    case WM_PAINT:
        if (screenBufferUpdated)
        {
            hdc = BeginPaint(hWnd, &ps);

            GetClientRect(hWnd, &rect);

            /*SetStretchBltMode(hdc, COLORONCOLOR);
            StretchBlt(hdc, rect.left, rect.top, rect.right, rect.bottom, hdcBuf, 0, 0, columns * fontWidth, rows * fontHeight, SRCCOPY);*/

            Graphics g(hdc);
            
            g.SetCompositingMode(CompositingModeSourceCopy);
            Image* z = (Image*)Bitmap::FromHBITMAP(hBuf, NULL);

            /*float scaleX = static_cast<float>(z->GetWidth()) / static_cast<float>(rect.right - rect.left);
            float scaleY = static_cast<float>(z->GetHeight()) / static_cast<float>(rect.bottom - rect.top);
            Gdiplus::Matrix m(scaleX, 0, 0, scaleY, 0, 0);
            
            g.SetTransform(&m);*/
            g.DrawImage(z, 0, 0, rect.right - rect.left, rect.bottom - rect.top);
            //g.ResetTransform();

            delete z;
            //m.~Matrix();
            g.~Graphics();

            EndPaint(hWnd, &ps);
        }
        return 0;
    case WM_SHOWWINDOW:
        if (wParam)
            SendMessageW(hWnd, WM_KEYDOWN, VK_HOME, 0);
        break;
    case WM_SETCURSOR:
        if (fullScreen)
        {
            SetCursor(LoadCursorW(NULL, NULL));
            return TRUE;
        }
    default:
        return DefWindowProc(hWnd, Msg, wParam, lParam);
    }
}

LRESULT CALLBACK Console::StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    Console* classPointer;

    if (Msg == WM_NCCREATE)
    {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        classPointer = static_cast<Console*>(lpcs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(classPointer));
    }
    else
    {
        classPointer = reinterpret_cast<Console*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    return classPointer->WndProc(hWnd, Msg, wParam, lParam);
}