#include "pch.h"
#include "Logger.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

const wchar_t* levelNames[4] = {
	L"  BASIC",
	L" NORMAL",
	L" DETAIL",
	L"VERBOSE"
};

namespace LibPanther 
{
	Logger::Logger(const wchar_t* fileName, int outputLevel)
	{
		szLogFile = fileName;
		dwLogLevel = outputLevel;

		hLogFile = CreateFileW(fileName, GENERIC_WRITE | FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (!hLogFile)
		{
			const wchar_t* output = L"Logger failed to initialize.";
			WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), output, lstrlenW(output), NULL, NULL);
		}
		// Write LE UTF-16 byte order mark
		WriteFile(hLogFile, "ÿþ", 2, NULL, NULL);
	}

	void Logger::Write(int level, const wchar_t* message)
	{
		if (dwLogLevel < level)
			return;
		formatTime();
		swprintf(messageBuffer, 512, L"%s %s %s\r\n", timeBuffer, levelNames[level], message);
		WriteDirect(level, messageBuffer);
	}
	
	void Logger::WriteDirect(int level, const wchar_t* message)
	{
		if (dwLogLevel < level)
			return;
		WriteFile(hLogFile, message, lstrlenW(message) * sizeof(wchar_t), NULL, NULL);
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), message, lstrlenW(message), NULL, NULL);
	}

	void Logger::formatTime()
	{
		time_t tTime = time(NULL);
		tm lTime;
		localtime_s(&lTime, &tTime);
		wcsftime(timeBuffer, 100, L"", &lTime);
	}
}

void *safeMalloc(LibPanther::Logger* logger, size_t size)
{
	void *returnValue = malloc(size);
	if (!returnValue) 
	{
		logger->WriteDirect(PANTHER_LL_BASIC, L"FATAL: Failed to allocate memory.\r\n");
		exit(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ERROR_NOT_ENOUGH_MEMORY));
	}
	return returnValue;
}