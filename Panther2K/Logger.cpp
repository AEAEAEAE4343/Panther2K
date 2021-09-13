#include "Logger.h"
#include <iostream>

bool Logger::isEnabled = true;
bool Logger::isInitialized = false;
HANDLE Logger::handle = 0;

void Logger::Init()
{
	handle = CreateFileW(L"panther.log", (GENERIC_READ | GENERIC_WRITE), NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		MessageBoxW(NULL, L"Could not open log file. Panther2K will continue with logging disabled.", L"Panther2K", MB_OK | MB_ICONERROR);
		isEnabled = false;
		return;
	}

	isInitialized = true;
}

void Logger::WriteLine(const wchar_t* text, const wchar_t* source, const wchar_t* sourceDetailed)
{
	if (!isEnabled)
		return;
	if (!isInitialized)
		Init();

	wchar_t buffer[1024];
	swprintf(buffer, 1024, L"[%8s]", source);
}
