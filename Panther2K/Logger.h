#pragma once

#include <Windows.h>

static class Logger
{
public:
	static void Init();
	static void WriteLine(const wchar_t* text, const wchar_t* source, const wchar_t* sourceDetailed);
private:
	static bool isEnabled;
	static bool isInitialized;
	static HANDLE handle;
};

