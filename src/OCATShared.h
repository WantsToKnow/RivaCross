#pragma once

#include "RTSShared.h"

bool OCATSharedOpen();
bool OCATSharedUpdate(const RTSSCrossConfig* config, bool visible);
void OCATSharedClose();
