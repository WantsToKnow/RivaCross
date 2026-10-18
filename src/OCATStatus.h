#pragma once

#include <windows.h>

#define RIVACROSS_OCAT_STATUS_PREFIX L"Local\\RivaCrossOCATStatusV2-"
#define RIVACROSS_OCAT_STATUS_MAGIC 0x53544352u
#define RIVACROSS_OCAT_STATUS_VERSION 2u

struct OCATStatus {
	DWORD magic;
	DWORD version;
	volatile LONG stage; // 1: DLL code ran, 2: Present hook reached, 3: draw commands issued
	DWORD api;            // 0: unknown, 11: D3D11, 12: D3D12
	volatile LONG callbackSeen;
	DWORD authorizationError;
	DWORD startError;
};

static_assert(sizeof(OCATStatus) == 28, "OCAT status protocol size changed");
