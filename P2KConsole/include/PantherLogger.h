#pragma once
#include <windows.h>

// Shows only basic program flow and errors
#define PANTHER_LL_BASIC 0
// Shows warnings and progress reports
#define PANTHER_LL_NORMAL 1
// Shows program flow and progress reports
#define PANTHER_LL_DETAILED 2
// Shows verbose progress informations
#define PANTHER_LL_VERBOSE 3

namespace LibPanther
{
	class Logger
	{
	public:
		// Initialized the logger with the desired log level
		Logger(const wchar_t* fileName, int logLevel);

		// Writes to the log file with formatted time and date information
		void Write(int level, const wchar_t* message);

		// Writes directly to the log file (uses as little memory as possible)
		void WriteDirect(int level, const wchar_t* message);
	private:
		wchar_t timeBuffer[100];
		wchar_t messageBuffer[512];
		HANDLE hLogFile;
		const wchar_t* szLogFile;
		int dwLogLevel;

		void formatTime();
	};
}

void* __cdecl safeMalloc(LibPanther::Logger* logger, size_t size);