#pragma once
#include <windows.h>

#define RIVACROSS_OCAT_PROBE_PREFIX L"Local\\RivaCrossOCATProbeV1-"
#define RIVACROSS_OCAT_PROBE_MAGIC 0x50414352u
#define RIVACROSS_OCAT_PROBE_VERSION 1u

struct OCATProbeAddress {
	DWORD rva, timestamp, imageSize;
	wchar_t module[MAX_PATH];
};

struct OCATProbeTargets {
	OCATProbeAddress present, present1, resize, resize1, release;
};

struct OCATProbeShared {
	DWORD magic, version, controllerPid;
	volatile LONG ready; // bit 0: D3D11, bit 1: D3D12; published last
	OCATProbeTargets d3d11, d3d12;
	OCATProbeAddress execute;
};
