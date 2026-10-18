#pragma once

#include "RTSShared.h"
#include "OCATWatchShared.h"
#include "OCATProbeShared.h"
#include "OCATStatus.h"

bool OCATSharedOpen();
bool OCATSharedUpdate(const RTSSCrossConfig* config, bool visible);
void OCATSharedClose();
bool OCATSharedReadStatus(DWORD pid, LONG* stage, DWORD* api, DWORD* error,
	OCATStatus* details = NULL);
bool OCATWatchArm(const wchar_t* executable, DWORD pid, OCATWatchMode mode);
void OCATWatchHeartbeat();
void OCATWatchStop();
bool OCATProbeOpen();
bool OCATProbeReady();
