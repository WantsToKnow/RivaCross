#pragma once
#include <windows.h>

#define RIVACROSS_OCAT_WATCH_NAME L"Local\\RivaCrossOCATWatchV1"
#define RIVACROSS_OCAT_WATCH_MAGIC 0x57414352u
#define RIVACROSS_OCAT_WATCH_VERSION 1u
enum OCATWatchMode { OCATWatchLaunch = 1, OCATWatchThread, OCATWatchRemote };

// One selected executable. A zero PID allows future instances of that file.
struct OCATWatchShared {
	volatile LONG sequence;
	DWORD magic, version, controllerPid;
	volatile LONG heartbeat;
	DWORD mode, targetPid;
	wchar_t executable[MAX_PATH];
};
