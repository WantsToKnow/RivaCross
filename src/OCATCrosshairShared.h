#pragma once

#include <windows.h>

// Shared by the RivaCross GUI and the optional OCAT rendering DLL.
// Positions use the same 960 x 540 reference grid as the RTSS preset UI.
#define RIVACROSS_OCAT_MAPPING_NAME L"Local\\RivaCrossOCATCrosshairV2"
#define RIVACROSS_OCAT_MAGIC 0x54414352u // "RCAT"
#define RIVACROSS_OCAT_VERSION 2u

struct OCATCrosshairShared {
	volatile LONG sequence;
	DWORD magic;
	DWORD version;
	LONG visible;
	LONG x;
	LONG y;
	LONG size;
	BYTE rgba[4];
	char text[64];
	LONG pixelOffsetX;
	LONG pixelOffsetY;
};
