#include "OCATShared.h"
#include "OCATCrosshairShared.h"

#include <string.h>

static HANDLE mapping = NULL;
static HANDLE writerMutex = NULL;
static OCATCrosshairShared* shared = NULL;

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
	shared->size = (LONG)config->scale;
	memcpy(shared->rgba, config->color, sizeof(shared->rgba));
	memcpy(shared->text, config->crosshair, sizeof(shared->text));
	shared->text[sizeof(shared->text) - 1] = '\0';
	MemoryBarrier();
	InterlockedIncrement(&shared->sequence); // even: stable snapshot
	return true;
}

void OCATSharedClose() {
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
