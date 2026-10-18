// Lightweight Windows hook entry. Graphics and C++ objects live in the renderer.
#include <windows.h>
#include "OCATWatchShared.h"
#include "OCATStatus.h"

static HMODULE hookModule;
static INIT_ONCE watchOnce = INIT_ONCE_STATIC_INIT, statusOnce = INIT_ONCE_STATIC_INIT;
static HANDLE watchMapping, statusMapping;
static const OCATWatchShared* watch;
static OCATStatus* receipt;
static wchar_t executable[MAX_PATH];
static DWORD watchError;
static volatile LONG workerStarted;

static BOOL CALLBACK openWatch(PINIT_ONCE, PVOID, PVOID*) {
	DWORD length = GetModuleFileNameW(NULL, executable, MAX_PATH);
	if (!length || length >= MAX_PATH) { watchError = ERROR_BAD_PATHNAME; return FALSE; }
	watchMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, RIVACROSS_OCAT_WATCH_NAME);
	if (!watchMapping) { watchError = GetLastError(); return FALSE; }
	watch = (const OCATWatchShared*)MapViewOfFile(watchMapping, FILE_MAP_READ, 0, 0, sizeof(*watch));
	if (!watch) {
		watchError = GetLastError();
		CloseHandle(watchMapping);
		watchMapping = NULL;
		return FALSE;
	}
	return TRUE;
}

static DWORD selectionError() {
	if (!InitOnceExecuteOnce(&watchOnce, openWatch, NULL, NULL)) return watchError;
	LONG sequence = watch->sequence;
	if (sequence & 1) return ERROR_BUSY;
	MemoryBarrier();
	OCATWatchShared selected;
	// A volatile copy also works without CRT memory functions in small toolchains.
	for (SIZE_T i = 0; i < sizeof(selected); ++i)
		((BYTE*)&selected)[i] = ((const volatile BYTE*)watch)[i];
	MemoryBarrier();
	if (sequence != watch->sequence) return ERROR_BUSY;
	if (selected.magic != RIVACROSS_OCAT_WATCH_MAGIC ||
		selected.version != RIVACROSS_OCAT_WATCH_VERSION ||
		selected.mode < OCATWatchLaunch || selected.mode > OCATWatchRemote ||
		!selected.executable[0] || selected.executable[MAX_PATH - 1]) return ERROR_INVALID_DATA;
	if (!selected.heartbeat || (DWORD)(GetTickCount() - (DWORD)selected.heartbeat) >= 3000)
		return ERROR_TIMEOUT;
	DWORD pid = GetCurrentProcessId();
	if (selected.controllerPid == pid || (selected.targetPid && selected.targetPid != pid) ||
		CompareStringOrdinal(executable, -1, selected.executable, -1, TRUE) != CSTR_EQUAL)
		return ERROR_NOT_FOUND;
	return ERROR_SUCCESS;
}

static BOOL CALLBACK openStatus(PINIT_ONCE, PVOID, PVOID*) {
	wchar_t name[80];
	// wsprintfW's bounded inputs are a fixed prefix and at most ten PID digits.
	wsprintfW(name, L"%ls%lu", RIVACROSS_OCAT_STATUS_PREFIX, GetCurrentProcessId());
	statusMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
		0, sizeof(OCATStatus), name);
	if (!statusMapping) return FALSE;
	bool existing = GetLastError() == ERROR_ALREADY_EXISTS;
	receipt = (OCATStatus*)MapViewOfFile(statusMapping, FILE_MAP_WRITE, 0, 0, sizeof(OCATStatus));
	if (!receipt || (existing && (receipt->magic != RIVACROSS_OCAT_STATUS_MAGIC ||
		receipt->version != RIVACROSS_OCAT_STATUS_VERSION))) {
		if (receipt) UnmapViewOfFile(receipt);
		receipt = NULL;
		CloseHandle(statusMapping);
		statusMapping = NULL;
		return FALSE;
	}
	if (!existing) {
		receipt->version = RIVACROSS_OCAT_STATUS_VERSION;
		MemoryBarrier();
		receipt->magic = RIVACROSS_OCAT_STATUS_MAGIC;
	}
	return TRUE;
}

static void startError(DWORD error) {
	if (InitOnceExecuteOnce(&statusOnce, openStatus, NULL, NULL)) receipt->startError = error;
}

static DWORD WINAPI loadRenderer(void*) {
	DWORD error = selectionError();
	if (error) { startError(error); return 0; }
	wchar_t library[MAX_PATH];
	DWORD length = GetModuleFileNameW(hookModule, library, MAX_PATH);
	if (!length || length >= MAX_PATH) { startError(ERROR_BAD_PATHNAME); return 0; }
	while (length && library[length - 1] != L'\\') --length;
	const wchar_t renderer[] = L"gameoverlay64-flip.dll";
	if (!length || length + ARRAYSIZE(renderer) > MAX_PATH) {
		startError(ERROR_BAD_PATHNAME); return 0;
	}
	for (SIZE_T i = 0; i < ARRAYSIZE(renderer); ++i) library[length + i] = renderer[i];
	HMODULE module = LoadLibraryExW(library, NULL,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if (!module) { startError(GetLastError()); return 0; }
	LPTHREAD_START_ROUTINE start = (LPTHREAD_START_ROUTINE)GetProcAddress(module, "StartRemoteRenderer");
	if (!start) { startError(GetLastError()); FreeLibrary(module); return 0; }
	if (!start(NULL) && InitOnceExecuteOnce(&statusOnce, openStatus, NULL, NULL) &&
		!receipt->startError) receipt->startError = ERROR_DLL_INIT_FAILED;
	// The renderer and this entry retain their code while callbacks can be in flight.
	return 0;
}

extern "C" __declspec(dllexport) LRESULT CALLBACK
GlobalHookProc(int code, WPARAM key, LPARAM value) {
	if (code >= 0) {
		DWORD error = selectionError();
		if (InitOnceExecuteOnce(&statusOnce, openStatus, NULL, NULL)) {
			receipt->authorizationError = error;
			MemoryBarrier();
			InterlockedExchange(&receipt->callbackSeen, 1);
		}
		if (!error && InterlockedCompareExchange(&workerStarted, 1, 0) == 0) {
			HMODULE pinned;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				(LPCWSTR)&GlobalHookProc, &pinned)) startError(GetLastError());
			else {
				HANDLE thread = CreateThread(NULL, 0, loadRenderer, NULL, 0, NULL);
				if (thread) CloseHandle(thread);
				else startError(GetLastError());
			}
		}
	}
	return CallNextHookEx(NULL, code, key, value);
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		hookModule = module;
	} else if (reason == DLL_PROCESS_DETACH) {
		if (watch) UnmapViewOfFile(watch);
		if (watchMapping) CloseHandle(watchMapping);
		if (receipt) UnmapViewOfFile(receipt);
		if (statusMapping) CloseHandle(statusMapping);
	}
	return TRUE;
}
