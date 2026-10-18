#include "OCATShared.h"
#include "OCATCrosshairShared.h"
#include "OCATStatus.h"

#include <string.h>
#include <wchar.h>

static HANDLE mapping = NULL;
static HANDLE writerMutex = NULL;
static OCATCrosshairShared* shared = NULL;
static HANDLE watchMapping = NULL;
static OCATWatchShared* watch = NULL;
static HANDLE probeMapping = NULL;
static OCATProbeShared* probe = NULL;

bool OCATProbeOpen() {
	if (probe) return true;
	wchar_t name[80];
	swprintf(name, 80, L"%ls%lu", RIVACROSS_OCAT_PROBE_PREFIX, GetCurrentProcessId());
	probeMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
		0, sizeof(OCATProbeShared), name);
	if (!probeMapping) return false;
	probe = (OCATProbeShared*)MapViewOfFile(probeMapping, FILE_MAP_WRITE, 0, 0, sizeof(OCATProbeShared));
	if (!probe) { CloseHandle(probeMapping); probeMapping = NULL; return false; }
	ZeroMemory(probe, sizeof(*probe));
	probe->magic = RIVACROSS_OCAT_PROBE_MAGIC;
	probe->version = RIVACROSS_OCAT_PROBE_VERSION;
	probe->controllerPid = GetCurrentProcessId();
	return true;
}

bool OCATProbeReady() {
	return probe && probe->ready != 0;
}

bool OCATWatchArm(const wchar_t* executable, DWORD pid, OCATWatchMode mode) {
	if (!executable || !executable[0] || wcslen(executable) >= MAX_PATH ||
		!OCATSharedOpen()) return false;
	if (!watchMapping) watchMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
		PAGE_READWRITE, 0, sizeof(OCATWatchShared), RIVACROSS_OCAT_WATCH_NAME);
	if (!watchMapping) return false;
	if (!watch) watch = (OCATWatchShared*)MapViewOfFile(watchMapping,
		FILE_MAP_WRITE, 0, 0, sizeof(OCATWatchShared));
	if (!watch) return false;
	// The crosshair writer mutex also serializes this control block.
	if (watch->sequence & 1) InterlockedIncrement(&watch->sequence);
	InterlockedIncrement(&watch->sequence);
	watch->magic = RIVACROSS_OCAT_WATCH_MAGIC;
	watch->version = RIVACROSS_OCAT_WATCH_VERSION;
	watch->controllerPid = GetCurrentProcessId();
	watch->mode = mode;
	watch->targetPid = pid;
	wcscpy(watch->executable, executable);
	InterlockedExchange(&watch->heartbeat, (LONG)GetTickCount());
	MemoryBarrier();
	InterlockedIncrement(&watch->sequence);
	return true;
}

void OCATWatchHeartbeat() {
	if (watch) InterlockedExchange(&watch->heartbeat, (LONG)GetTickCount());
}

void OCATWatchStop() {
	if (watch) {
		InterlockedIncrement(&watch->sequence);
		watch->mode = 0;
		watch->executable[0] = 0;
		InterlockedExchange(&watch->heartbeat, 0);
		MemoryBarrier();
		InterlockedIncrement(&watch->sequence);
		UnmapViewOfFile(watch);
		watch = NULL;
	}
	if (watchMapping) { CloseHandle(watchMapping); watchMapping = NULL; }
}

bool OCATSharedOpen() {
	if (shared) return true;
	writerMutex = CreateMutexW(NULL, FALSE, L"Local\\RivaCrossOCATWriterV1");
	if (!writerMutex) return false;
	DWORD lockResult = WaitForSingleObject(writerMutex, 0);
	if (lockResult != WAIT_OBJECT_0 && lockResult != WAIT_ABANDONED) {
		CloseHandle(writerMutex);
		writerMutex = NULL;
		return false;
	}
	mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		sizeof(OCATCrosshairShared), RIVACROSS_OCAT_MAPPING_NAME);
	if (!mapping) {
		OCATSharedClose();
		return false;
	}
	bool isNew = GetLastError() != ERROR_ALREADY_EXISTS;
	shared = (OCATCrosshairShared*)MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0,
		sizeof(OCATCrosshairShared));
	if (!shared) {
		OCATSharedClose();
		return false;
	}
	if (isNew) {
		ZeroMemory(shared, sizeof(*shared));
		shared->magic = RIVACROSS_OCAT_MAGIC;
		shared->version = RIVACROSS_OCAT_VERSION;
	} else {
		if (shared->magic != RIVACROSS_OCAT_MAGIC ||
			shared->version != RIVACROSS_OCAT_VERSION) {
			OCATSharedClose();
			return false;
		}
		// A previous writer may have exited during an update.
		if (shared->sequence & 1) InterlockedIncrement(&shared->sequence);
	}
	return true;
}

bool OCATSharedUpdate(const RTSSCrossConfig* config, bool visible) {
	if (!shared || !config) return false;
	InterlockedIncrement(&shared->sequence); // odd: writer in progress
	shared->visible = visible ? 1 : 0;
	shared->x = config->position[0];
	shared->y = config->position[1];
	shared->pixelOffsetX = config->pixelOffset[0];
	shared->pixelOffsetY = config->pixelOffset[1];
	shared->size = (LONG)config->scale;
	memcpy(shared->rgba, config->color, sizeof(shared->rgba));
	memcpy(shared->text, config->crosshair, sizeof(shared->text));
	shared->text[sizeof(shared->text) - 1] = '\0';
	MemoryBarrier();
	InterlockedIncrement(&shared->sequence); // even: stable snapshot
	return true;
}

void OCATSharedClose() {
	OCATWatchStop();
	if (probe) { UnmapViewOfFile(probe); probe = NULL; }
	if (probeMapping) { CloseHandle(probeMapping); probeMapping = NULL; }
	if (shared) {
		UnmapViewOfFile(shared);
		shared = NULL;
	}
	if (mapping) {
		CloseHandle(mapping);
		mapping = NULL;
	}
	if (writerMutex) {
		ReleaseMutex(writerMutex);
		CloseHandle(writerMutex);
		writerMutex = NULL;
	}
}

bool OCATSharedReadStatus(DWORD pid, LONG* stage, DWORD* api, DWORD* error, OCATStatus* details) {
	if (!pid || !stage || !api || !error) return false;
	*error = ERROR_INVALID_DATA;
	wchar_t name[64];
	if (swprintf(name, sizeof(name) / sizeof(name[0]), L"%ls%lu",
		RIVACROSS_OCAT_STATUS_PREFIX, (unsigned long)pid) < 0) return false;
	HANDLE statusMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
	if (!statusMapping) {
		*error = GetLastError();
		return false;
	}
	const OCATStatus* view = (const OCATStatus*)MapViewOfFile(
		statusMapping, FILE_MAP_READ, 0, 0, sizeof(OCATStatus));
	if (!view) *error = GetLastError();
	bool valid = false;
	if (view) {
		const LONG observedStage = view->stage;
		MemoryBarrier();
		const DWORD observedApi = view->api;
		valid = view->magic == RIVACROSS_OCAT_STATUS_MAGIC &&
			view->version == RIVACROSS_OCAT_STATUS_VERSION &&
			observedStage >= 0 && observedStage <= 3 &&
			(observedApi == 0 || observedApi == 11 || observedApi == 12) &&
			(view->callbackSeen == 0 || view->callbackSeen == 1);
		if (valid) {
			*stage = observedStage;
			*api = observedApi;
			*error = ERROR_SUCCESS;
			if (details) memcpy(details, view, sizeof(*details));
		}
		UnmapViewOfFile(view);
	}
	CloseHandle(statusMapping);
	return valid;
}
