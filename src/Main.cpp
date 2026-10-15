#include "RivaCross.h"
#ifdef RIVACROSS_OCAT
#include "OCATShared.h"
#include <tlhelp32.h>
#endif

#include <commdlg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef RIVACROSS_OCAT
#include <wchar.h>
#endif

enum ControlId {
	IdCrosshair = 101, IdX, IdY, IdScale, IdColor, IdMode,
	IdHotkey, IdMaster, IdLoad, IdSaveAs, IdApply, IdVisible,
	IdClipboard, IdKeep, IdStatus, IdLaunch, IdAttach
};

#ifndef RIVACROSS_OCAT
static const char* const OsdEntry = "RivaCross_crosshair";
#endif

struct AppState {
	HWND window, crosshair, x, y, scale, color, mode;
	HWND hotkey, master, visibleButton, keep, status;
	HWND captureTarget;
	HFONT font;
	WNDPROC buttonProc;
	RTSSCrossConfig config;
	HotkeyState hotkeyState;
	char configDir[MAX_PATH];
	char configPath[MAX_PATH];
};

static AppState app = {};

static HWND control(const char* type, const char* text, DWORD style,
	int x, int y, int width, int height, int id = 0) {
	HWND hwnd = CreateWindowExA(0, type, text, WS_CHILD | WS_VISIBLE | style,
		x, y, width, height, app.window, (HMENU)(INT_PTR)id,
		GetModuleHandleA(NULL), NULL);
	SendMessageA(hwnd, WM_SETFONT, (WPARAM)app.font, TRUE);
	return hwnd;
}

static void status(const char* message) {
	SetWindowTextA(app.status, message);
}

static void formatKey(int vk, int modifiers, char* text, size_t capacity) {
	if (!vk) {
		snprintf(text, capacity, "None (click to set)");
		return;
	}
	char name[40] = "";
	hotkeyVkCodeToName(vk, name, sizeof(name));
	snprintf(text, capacity, "%s%s%s%s", (modifiers & 1) ? "Ctrl+" : "",
		(modifiers & 2) ? "Shift+" : "", (modifiers & 4) ? "Alt+" : "", name);
}

static void refreshHotkeys() {
	char name[96];
	formatKey(app.hotkeyState.keyCode, app.hotkeyState.modifierMask, name, sizeof(name));
	SetWindowTextA(app.hotkey, name);
	formatKey(app.hotkeyState.masterKeyCode, app.hotkeyState.masterModifierMask, name, sizeof(name));
	SetWindowTextA(app.master, name);
}

static void refreshControls() {
	char text[96];
	SetWindowTextA(app.crosshair, app.config.crosshair);
	snprintf(text, sizeof(text), "%d", app.config.position[0]);
	SetWindowTextA(app.x, text);
	snprintf(text, sizeof(text), "%d", app.config.position[1]);
	SetWindowTextA(app.y, text);
	snprintf(text, sizeof(text), "%u", app.config.scale);
	SetWindowTextA(app.scale, text);
	snprintf(text, sizeof(text), "#%02X%02X%02X%02X", app.config.color[0],
		app.config.color[1], app.config.color[2], app.config.color[3]);
	SetWindowTextA(app.color, text);
	SendMessageA(app.mode, CB_SETCURSEL, app.hotkeyState.mode, 0);
	refreshHotkeys();
	SetWindowTextA(app.visibleButton, app.hotkeyState.visible ? "Hide" : "Show");
}

static bool parseNumber(HWND field, int* value) {
	char text[32];
	GetWindowTextA(field, text, sizeof(text));
	char* end;
	unsigned long parsed = strtoul(text, &end, 10);
	if (!text[0] || *end || parsed > INT_MAX || text[0] == '-') return false;
	*value = (int)parsed;
	return true;
}

static int hexDigit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static bool readControls() {
	RTSSCrossConfig next = app.config;
	GetWindowTextA(app.crosshair, next.crosshair, sizeof(next.crosshair));
	int x, y, scale;
	if (!parseNumber(app.x, &x) || !parseNumber(app.y, &y) ||
		!parseNumber(app.scale, &scale)) {
		MessageBoxA(app.window, "Position and size must be nonnegative whole numbers.",
			"Invalid value", MB_OK | MB_ICONWARNING);
		return false;
	}
	char color[16];
	GetWindowTextA(app.color, color, sizeof(color));
	size_t length = strlen(color);
	if ((length != 7 && length != 9) || color[0] != '#') {
		MessageBoxA(app.window, "Use #RRGGBB or #RRGGBBAA for the color.",
			"Invalid color", MB_OK | MB_ICONWARNING);
		return false;
	}
	for (size_t i = 1; i < length; ++i) {
		if (hexDigit(color[i]) < 0) {
			MessageBoxA(app.window, "Use #RRGGBB or #RRGGBBAA for the color.",
				"Invalid color", MB_OK | MB_ICONWARNING);
			return false;
		}
	}
	for (int i = 0; i < 3; ++i)
		next.color[i] = (unsigned char)((hexDigit(color[1 + i * 2]) << 4) |
			hexDigit(color[2 + i * 2]));
	next.color[3] = length == 9
		? (unsigned char)((hexDigit(color[7]) << 4) | hexDigit(color[8])) : 0xff;
	next.position[0] = x;
	next.position[1] = y;
	next.scale = (unsigned int)scale;
	int mode = (int)SendMessageA(app.mode, CB_GETCURSEL, 0, 0);
	if (mode != HotkeyNone && !app.hotkeyState.keyCode) {
		MessageBoxA(app.window, "Click the hotkey field and press a key, or disable the hotkey mode.",
			"Hotkey missing", MB_OK | MB_ICONWARNING);
		return false;
	}
	app.config = next;
	app.hotkeyState.mode = (HotkeyMode)mode;
	return true;
}

static int sendOverlay() {
#ifdef RIVACROSS_OCAT
	return OCATSharedUpdate(&app.config, app.hotkeyState.visible) ? 0 : -1;
#else
	return RTSSUpdateOSDVisibility(&app.config, OsdEntry, app.hotkeyState.visible);
#endif
}

static void updateOverlay() {
	int result = sendOverlay();
#ifdef RIVACROSS_OCAT
	status(result == 0 ? "Settings ready. Check the crosshair in your game." :
		"Could not create OCAT bridge. Close another RivaCrossOCAT or restart.");
#else
	status(result == 0 ? "Sent to RTSS; game display is not verified." :
		result == -4 ? "RTSS is busy. Click Apply + Save again." :
		"RTSS is unavailable. Start RTSS, then click Apply + Save.");
#endif
}

static void applyAndSave() {
	if (!readControls()) return;
	CreateDirectoryA(app.configDir, NULL);
	bool saved = saveConfigToFile(app.configPath, &app.config, &app.hotkeyState);
	updateOverlay();
	if (!saved) status("Could not save the preset file.");
	refreshControls();
}

static bool chooseFile(bool save, char* path) {
	OPENFILENAMEA dialog = {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = app.window;
	dialog.lpstrFilter = "RivaCross presets (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
	dialog.lpstrFile = path;
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrInitialDir = app.configDir;
	dialog.lpstrDefExt = "txt";
	dialog.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
	return save ? GetSaveFileNameA(&dialog) != 0 : GetOpenFileNameA(&dialog) != 0;
}

static void loadPreset() {
	char path[MAX_PATH] = "";
	if (!chooseFile(false, path)) return;
	RTSSCrossConfig config = app.config;
	HotkeyState hotkey = {};
	hotkey.visible = true;
	hotkey.masterEnabled = true;
	if (loadConfig(path, &config, &hotkey) != 0) {
		MessageBoxA(app.window, "The preset file is missing or invalid.", "Load failed",
			MB_OK | MB_ICONWARNING);
		return;
	}
	app.config = config;
	app.hotkeyState = hotkey;
	app.hotkeyState.visible = true;
	app.hotkeyState.masterEnabled = true;
	app.hotkeyState.prevAsyncState = false;
	app.hotkeyState.masterPrevAsyncState = false;
	strcpy(app.configPath, path);
	refreshControls();
	updateOverlay();
}

static void saveAs() {
	if (!readControls()) return;
	char path[MAX_PATH] = "";
	if (!chooseFile(true, path)) return;
	if (!saveConfigToFile(path, &app.config, &app.hotkeyState)) {
		MessageBoxA(app.window, "Could not write the preset file.", "Save failed",
			MB_OK | MB_ICONWARNING);
		return;
	}
	strcpy(app.configPath, path);
	updateOverlay();
	refreshControls();
}

#ifdef RIVACROSS_OCAT
static bool besideExe(wchar_t path[MAX_PATH], const wchar_t* suffix) {
	DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
	if (!length || length >= MAX_PATH) return false;
	wchar_t* slash = wcsrchr(path, L'\\');
	if (!slash) return false;
	size_t prefix = slash + 1 - path;
	if (prefix + wcslen(suffix) >= MAX_PATH) return false;
	wcscpy(path + prefix, suffix);
	return true;
}

static bool regularFile(const wchar_t* path) {
	DWORD attributes = GetFileAttributesW(path);
	return attributes != INVALID_FILE_ATTRIBUTES &&
		!(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static bool isCs2(const wchar_t* path) {
	const wchar_t* slash = wcsrchr(path, L'\\');
	return _wcsicmp(slash ? slash + 1 : path, L"cs2.exe") == 0;
}

static bool prepareOcat(wchar_t dll[MAX_PATH], wchar_t injector[MAX_PATH]) {
	if (!OCATSharedOpen() || !OCATSharedUpdate(&app.config, app.hotkeyState.visible)) {
		status("OCAT bridge unavailable. Close another RivaCrossOCAT.");
		return false;
	}
	if (!besideExe(dll, L"ocat\\bin\\gameoverlay64.dll") ||
		!besideExe(injector, L"ocat\\bin\\DLLInjector64.exe") ||
		!regularFile(dll) || !regularFile(injector)) {
		status("OCAT files missing beside RivaCrossOCAT.exe.");
		return false;
	}
	return true;
}

// 1: helper reported success, 0: timeout, -1: failure. Rendering is not verified.
static int runInjector(const wchar_t* dll, const wchar_t* injector, DWORD pid) {
	wchar_t command[MAX_PATH * 2 + 64];
	if (swprintf(command, MAX_PATH * 2 + 64, L"\"%ls\" -l \"%ls\" -p %lu",
		injector, dll, (unsigned long)pid) < 0) return -1;
	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION helper = {};
	if (!CreateProcessW(injector, command, NULL, NULL, FALSE,
		CREATE_NO_WINDOW, NULL, NULL, &startup, &helper)) return -1;
	DWORD wait = WaitForSingleObject(helper.hProcess, 15000);
	if (wait == WAIT_TIMEOUT) {
		TerminateProcess(helper.hProcess, 1);
		WaitForSingleObject(helper.hProcess, 5000);
	}
	DWORD exitCode = 0;
	int result = wait == WAIT_TIMEOUT ? 0 :
		wait == WAIT_OBJECT_0 && GetExitCodeProcess(helper.hProcess, &exitCode) &&
		exitCode == 1 ? 1 : -1;
	CloseHandle(helper.hThread);
	CloseHandle(helper.hProcess);
	return result;
}

struct AttachCandidate {
	DWORD pid;
	wchar_t path[MAX_PATH];
};

static bool inspectCandidate(DWORD pid, AttachCandidate* candidate) {
	if (pid <= 4 || pid == GetCurrentProcessId()) return false;
	DWORD session = 0, ownSession = 0;
	if (!ProcessIdToSessionId(pid, &session) ||
		!ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) ||
		session != ownSession) return false;
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
	if (!process) return false; // Protected or inaccessible processes are not candidates.
	wchar_t path[MAX_PATH];
	DWORD capacity = MAX_PATH;
	bool valid = QueryFullProcessImageNameW(process, 0, path, &capacity) != 0;
	CloseHandle(process);
	if (!valid || isCs2(path)) return false;
	wchar_t windows[MAX_PATH];
	UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
	if (length && length < MAX_PATH &&
		_wcsnicmp(path, windows, length) == 0 && path[length] == L'\\') return false;
	DWORD binaryType = 0;
	if (!GetBinaryTypeW(path, &binaryType) || binaryType != SCS_64BIT_BINARY)
		return false;
	HANDLE snapshot = INVALID_HANDLE_VALUE;
	for (int attempt = 0; attempt < 3; ++attempt) {
		snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
		if (snapshot != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH) break;
	}
	if (snapshot == INVALID_HANDLE_VALUE) return false;
	MODULEENTRY32W module = {};
	module.dwSize = sizeof(module);
	bool d3d11 = false;
	if (Module32FirstW(snapshot, &module)) do {
		if (_wcsicmp(module.szModule, L"d3d11.dll") == 0) {
			d3d11 = true;
			break;
		}
	} while (Module32NextW(snapshot, &module));
	CloseHandle(snapshot);
	if (!d3d11) return false;
	candidate->pid = pid;
	wcscpy(candidate->path, path);
	return true;
}

enum { IdCandidateList = 600, IdRefreshCandidates };
struct AttachPicker {
	HWND list, attach, hint;
	int count;
	// ponytail: 256 visible candidates are enough for a manual picker; use a vector if this ceiling is hit.
	AttachCandidate candidates[256];
};
static AttachPicker picker = {};

struct CandidateScan {
	DWORD seen[512];
	int count;
};

static BOOL CALLBACK scanCandidateWindow(HWND window, LPARAM parameter) {
	if (!IsWindowVisible(window)) return TRUE;
	CandidateScan* scan = (CandidateScan*)parameter;
	DWORD pid = 0;
	GetWindowThreadProcessId(window, &pid);
	for (int i = 0; i < scan->count; ++i)
		if (scan->seen[i] == pid) return TRUE;
	if (scan->count == 512 || picker.count == 256) return FALSE;
	scan->seen[scan->count++] = pid;
	AttachCandidate candidate = {};
	if (!inspectCandidate(pid, &candidate)) return TRUE;
	picker.candidates[picker.count++] = candidate;
	wchar_t label[MAX_PATH + 40];
	const wchar_t* slash = wcsrchr(candidate.path, L'\\');
	swprintf(label, MAX_PATH + 40, L"%ls   PID %lu",
		slash ? slash + 1 : candidate.path, (unsigned long)candidate.pid);
	SendMessageW(picker.list, LB_ADDSTRING, 0, (LPARAM)label);
	return TRUE;
}

static void refreshCandidates() {
	picker.count = 0;
	SendMessageW(picker.list, LB_RESETCONTENT, 0, 0);
	CandidateScan scan = {};
	EnumWindows(scanCandidateWindow, (LPARAM)&scan);
	if (picker.count) SendMessageW(picker.list, LB_SETCURSEL, 0, 0);
	EnableWindow(picker.attach, picker.count != 0);
	SetWindowTextW(picker.hint, picker.count ?
		L"64-bit D3D11 candidates. CS2 excluded; display is not guaranteed." :
		L"No D3D11 candidates. Start an app, then click Refresh.");
}

static INT_PTR CALLBACK pickerProc(HWND window, UINT message, WPARAM key, LPARAM) {
	if (message == WM_INITDIALOG) {
		SetWindowTextW(window, L"Attach to running D3D11 application");
		RECT rect;
		GetClientRect(window, &rect);
		picker.hint = CreateWindowW(L"STATIC",
			L"64-bit D3D11 candidates. CS2 is excluded. Display is not guaranteed.",
			WS_CHILD | WS_VISIBLE, 10, 9, rect.right - 20, 22,
			window, NULL, GetModuleHandleW(NULL), NULL);
		picker.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY |
			LBS_NOINTEGRALHEIGHT, 10, 34, rect.right - 20, rect.bottom - 78,
			window, (HMENU)IdCandidateList, GetModuleHandleW(NULL), NULL);
		picker.attach = CreateWindowW(L"BUTTON", L"Attach selected", WS_CHILD |
			WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 10, rect.bottom - 36,
			115, 27, window, (HMENU)IDOK, GetModuleHandleW(NULL), NULL);
		HWND refresh = CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD |
			WS_VISIBLE | WS_TABSTOP, 135, rect.bottom - 36, 90, 27,
			window, (HMENU)IdRefreshCandidates, GetModuleHandleW(NULL), NULL);
		HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD |
			WS_VISIBLE | WS_TABSTOP, rect.right - 100, rect.bottom - 36,
			90, 27, window, (HMENU)IDCANCEL, GetModuleHandleW(NULL), NULL);
		HWND controls[] = { picker.hint, picker.list, picker.attach, refresh, cancel };
		for (int i = 0; i < 5; ++i)
			SendMessageW(controls[i], WM_SETFONT, (WPARAM)app.font, TRUE);
		refreshCandidates();
		return TRUE;
	}
	if (message == WM_COMMAND) {
		if (LOWORD(key) == IDCANCEL) { EndDialog(window, 0); return TRUE; }
		if (LOWORD(key) == IdRefreshCandidates) { refreshCandidates(); return TRUE; }
		if (LOWORD(key) == IDOK ||
			(LOWORD(key) == IdCandidateList && HIWORD(key) == LBN_DBLCLK)) {
			int index = (int)SendMessageW(picker.list, LB_GETCURSEL, 0, 0);
			if (index >= 0 && index < picker.count) EndDialog(window, index + 1);
			return TRUE;
		}
	}
	return FALSE;
}

static void attachRunning() {
	wchar_t dll[MAX_PATH], injector[MAX_PATH];
	if (!prepareOcat(dll, injector)) return;
	static const struct {
		DLGTEMPLATE dialog;
		WORD menu, windowClass, title;
	} pickerTemplate = {
		{ WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME, 0, 0,
			0, 0, 350, 230 }, 0, 0, 0
	};
	INT_PTR selection = DialogBoxIndirectParamW(GetModuleHandleW(NULL),
		&pickerTemplate.dialog, app.window, pickerProc, 0);
	if (selection == -1) {
		status("Could not open the process selection dialog.");
		return;
	}
	if (selection <= 0 || selection > picker.count) return;
	AttachCandidate selected = picker.candidates[selection - 1];
	HANDLE target = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
		FALSE, selected.pid);
	if (!target) {
		status("Process exited or denied access. Refresh the list.");
		return;
	}
	AttachCandidate current = {};
	if (WaitForSingleObject(target, 0) != WAIT_TIMEOUT ||
		!inspectCandidate(selected.pid, &current) ||
		_wcsicmp(selected.path, current.path) != 0) {
		CloseHandle(target);
		status("Process changed or exited. Refresh the list and try again.");
		return;
	}
	int result = runInjector(dll, injector, selected.pid);
	CloseHandle(target);
	status(result == 1 ? "Injector reported success; check the game for TEST." :
		result == 0 ? "OCAT injector timed out." :
		"OCAT injection failed or the process denied access.");
}

static void launchGame() {
	wchar_t dll[MAX_PATH], injector[MAX_PATH];
	if (!prepareOcat(dll, injector)) return;

	wchar_t game[MAX_PATH] = L"";
	OPENFILENAMEW dialog = {};
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = app.window;
	dialog.lpstrFilter = L"Games (*.exe)\0*.exe\0All files (*.*)\0*.*\0";
	dialog.lpstrFile = game;
	dialog.nMaxFile = MAX_PATH;
	dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (!GetOpenFileNameW(&dialog)) return;
	if (isCs2(game)) {
		status("CS2 injection is disabled. Choose another D3D11 application.");
		return;
	}
	DWORD binaryType;
	if (!GetBinaryTypeW(game, &binaryType) || binaryType != SCS_64BIT_BINARY) {
		status("Choose a 64-bit game executable.");
		return;
	}

	wchar_t gameDir[MAX_PATH];
	wcscpy(gameDir, game);
	wchar_t* slash = wcsrchr(gameDir, L'\\');
	if (!slash) {
		status("Could not determine the game directory.");
		return;
	}
	*slash = L'\0';
	wchar_t gameCommand[MAX_PATH + 3];
	if (swprintf(gameCommand, MAX_PATH + 3, L"\"%ls\"", game) < 0) {
		status("Game path is too long.");
		return;
	}

	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION target = {};
	if (!CreateProcessW(game, gameCommand, NULL, NULL, FALSE, CREATE_SUSPENDED,
		NULL, gameDir, &startup, &target)) {
		status("Could not start the selected game.");
		return;
	}

	int injection = runInjector(dll, injector, target.dwProcessId);
	DWORD resumed = ResumeThread(target.hThread);
	CloseHandle(target.hThread);
	CloseHandle(target.hProcess);
	status(resumed == (DWORD)-1 ? "Could not resume the selected game." :
		injection == 1 ? "Game resumed; injector reported success. Check game." :
		injection == 0 ? "Game resumed; OCAT injector timed out." :
		"Game resumed; OCAT injection failed.");
}
#endif

static void endCapture(const char* message) {
	HWND button = app.captureTarget;
	app.captureTarget = NULL;
	if (button && GetCapture() == button) ReleaseCapture();
	refreshHotkeys();
	if (message) status(message);
}

static void beginCapture(HWND button) {
	if (app.captureTarget) endCapture(NULL);
	app.captureTarget = button;
	SetWindowTextA(button, "Press key or mouse button");
	status("Press a key or mouse button. Esc cancels; Backspace clears.");
	SetFocus(button);
	SetCapture(button);
}

static void acceptCapture(HWND button, int code) {
	int modifiers = (GetKeyState(VK_CONTROL) & 0x8000 ? 1 : 0) |
		(GetKeyState(VK_SHIFT) & 0x8000 ? 2 : 0) |
		(GetKeyState(VK_MENU) & 0x8000 ? 4 : 0);
	if (button == app.hotkey) {
		app.hotkeyState.keyCode = code;
		app.hotkeyState.modifierMask = code ? modifiers : 0;
		app.hotkeyState.prevAsyncState = true;
		if (code && SendMessageA(app.mode, CB_GETCURSEL, 0, 0) == HotkeyNone)
			SendMessageA(app.mode, CB_SETCURSEL, HotkeyToggle, 0);
		if (!code) SendMessageA(app.mode, CB_SETCURSEL, HotkeyNone, 0);
		app.hotkeyState.mode = (HotkeyMode)SendMessageA(app.mode, CB_GETCURSEL, 0, 0);
	} else {
		app.hotkeyState.masterKeyCode = code;
		app.hotkeyState.masterModifierMask = code ? modifiers : 0;
		app.hotkeyState.masterEnabled = true;
		app.hotkeyState.masterPrevAsyncState = true;
	}
	endCapture(code ? "Hotkey set. Click Apply + Save to keep it." :
		"Hotkey cleared. Click Apply + Save to keep it.");
}

static LRESULT CALLBACK captureProc(HWND hwnd, UINT message, WPARAM key, LPARAM param) {
	if (hwnd != app.captureTarget && (message == WM_RBUTTONDOWN ||
		message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN)) {
		beginCapture(hwnd);
		return message == WM_XBUTTONDOWN ? TRUE : 0;
	}
	if (hwnd == app.captureTarget) {
		if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
		if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
			if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU ||
				key == VK_LWIN || key == VK_RWIN) return 0;
			if (key == VK_ESCAPE) endCapture("Hotkey capture canceled.");
			else acceptCapture(hwnd, key == VK_BACK ? 0 : (int)key);
			return 0;
		}
		if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
			message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN ||
			message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDBLCLK ||
			message == WM_MBUTTONDBLCLK || message == WM_XBUTTONDBLCLK)
			return message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK ? TRUE : 0;
		if (message == WM_LBUTTONUP || message == WM_RBUTTONUP ||
			message == WM_MBUTTONUP || message == WM_XBUTTONUP) {
			int code = message == WM_LBUTTONUP ? VK_LBUTTON :
				message == WM_RBUTTONUP ? VK_RBUTTON :
				message == WM_MBUTTONUP ? VK_MBUTTON :
				(HIWORD(key) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
			acceptCapture(hwnd, code);
			return message == WM_XBUTTONUP ? TRUE : 0;
		}
		if (message == WM_CONTEXTMENU) return 0;
		if (message == WM_KILLFOCUS || message == WM_CAPTURECHANGED)
			endCapture("Hotkey capture canceled.");
	}
	return CallWindowProcA(app.buttonProc, hwnd, message, key, param);
}

static bool keyDown(int key, int modifiers) {
	if (!key || !(GetAsyncKeyState(key) & 0x8000)) return false;
	if ((modifiers & 1) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) return false;
	if ((modifiers & 2) && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) return false;
	if ((modifiers & 4) && !(GetAsyncKeyState(VK_MENU) & 0x8000)) return false;
	return true;
}

static void pollHotkeys() {
	HotkeyState* hotkey = &app.hotkeyState;
	bool master = keyDown(hotkey->masterKeyCode, hotkey->masterModifierMask);
	if (master && !hotkey->masterPrevAsyncState) {
		hotkey->masterEnabled = !hotkey->masterEnabled;
		status(hotkey->masterEnabled ? "Hotkeys enabled." : "Hotkeys paused.");
	}
	hotkey->masterPrevAsyncState = master;
	bool pressed = keyDown(hotkey->keyCode, hotkey->modifierMask);
	if (hotkey->masterEnabled && hotkey->mode != HotkeyNone && hotkey->keyCode) {
		bool visible = hotkey->visible;
		if (hotkey->mode == HotkeyHold) visible = pressed;
		else if (pressed && !hotkey->prevAsyncState) visible = !visible;
		if (visible != hotkey->visible) {
			hotkey->visible = visible;
			if (sendOverlay() != 0) {
#ifdef RIVACROSS_OCAT
				status("OCAT bridge unavailable. Restart RivaCrossOCAT.");
#endif
			}
			SetWindowTextA(app.visibleButton, visible ? "Hide" : "Show");
		}
	}
	hotkey->prevAsyncState = pressed;
}

static void createControls() {
	HDC dc = GetDC(app.window);
	int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
	if (dc) ReleaseDC(app.window, dc);
	app.font = CreateFontA(-MulDiv(11, dpi, 72), 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
	if (!app.font) app.font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	control("STATIC", "Crosshair text", 0, 20, 25, 110, 22);
	app.crosshair = control("EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
		140, 22, 360, 25, IdCrosshair);
	SendMessageA(app.crosshair, EM_SETLIMITTEXT, 63, 0);
	control("STATIC", "Position X / Y", 0, 20, 65, 110, 22);
	app.x = control("EDIT", "", WS_BORDER | WS_TABSTOP | ES_NUMBER,
		140, 62, 100, 25, IdX);
	app.y = control("EDIT", "", WS_BORDER | WS_TABSTOP | ES_NUMBER,
		255, 62, 100, 25, IdY);
	control("STATIC", "Size", 0, 20, 105, 110, 22);
	app.scale = control("EDIT", "", WS_BORDER | WS_TABSTOP | ES_NUMBER,
		140, 102, 100, 25, IdScale);
	control("STATIC", "Color", 0, 20, 145, 110, 22);
	app.color = control("EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
		140, 142, 170, 25, IdColor);
	control("STATIC", "#RRGGBB or #RRGGBBAA", 0, 320, 145, 190, 22);
	control("STATIC", "Hotkey mode", 0, 20, 185, 110, 22);
	app.mode = control("COMBOBOX", "", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
		140, 182, 115, 110, IdMode);
	SendMessageA(app.mode, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendMessageA(app.mode, CB_ADDSTRING, 0, (LPARAM)"Hold");
	SendMessageA(app.mode, CB_ADDSTRING, 0, (LPARAM)"Toggle");
	app.hotkey = control("BUTTON", "", WS_TABSTOP | BS_PUSHBUTTON,
		270, 182, 230, 27, IdHotkey);
	control("STATIC", "Master toggle", 0, 20, 225, 110, 22);
	app.master = control("BUTTON", "", WS_TABSTOP | BS_PUSHBUTTON,
		140, 222, 360, 27, IdMaster);
	app.buttonProc = (WNDPROC)SetWindowLongPtrA(app.hotkey, GWLP_WNDPROC, (LONG_PTR)captureProc);
	SetWindowLongPtrA(app.master, GWLP_WNDPROC, (LONG_PTR)captureProc);
	control("BUTTON", "Load...", WS_TABSTOP, 20, 270, 95, 30, IdLoad);
	control("BUTTON", "Save As...", WS_TABSTOP, 125, 270, 105, 30, IdSaveAs);
	control("BUTTON", "Apply + Save", WS_TABSTOP | BS_DEFPUSHBUTTON,
		240, 270, 125, 30, IdApply);
	app.visibleButton = control("BUTTON", "Hide", WS_TABSTOP,
		375, 270, 125, 30, IdVisible);
	control("BUTTON", "Clear clipboard...", WS_TABSTOP,
		20, 315, 150, 28, IdClipboard);
#ifdef RIVACROSS_OCAT
	control("BUTTON", "Launch game...", WS_TABSTOP,
		190, 315, 150, 28, IdLaunch);
	control("BUTTON", "Attach running...", WS_TABSTOP,
		350, 315, 150, 28, IdAttach);
#else
	app.keep = control("BUTTON", "Leave crosshair visible when closing",
		WS_TABSTOP | BS_AUTOCHECKBOX, 190, 315, 310, 28, IdKeep);
#endif
	app.status = control("STATIC", "", 0, 20, 365, 490, 22, IdStatus);
}

static void defaultConfigPath() {
	char exe[MAX_PATH];
	GetModuleFileNameA(NULL, exe, sizeof(exe));
	char* slash = strrchr(exe, '\\');
	if (slash) *slash = 0;
	snprintf(app.configDir, sizeof(app.configDir), "%s\\config", exe);
	snprintf(app.configPath, sizeof(app.configPath), "%s\\default.txt", app.configDir);
}

static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
		app.window = hwnd;
		createControls();
#ifdef RIVACROSS_OCAT
		OCATSharedOpen();
#endif
		initConfig(&app.config);
		app.hotkeyState.visible = true;
		app.hotkeyState.masterEnabled = true;
		defaultConfigPath();
		loadConfig(app.configPath, &app.config, &app.hotkeyState);
		refreshControls();
		updateOverlay();
		SetTimer(hwnd, 1, 10, NULL);
		return 0;
	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IdHotkey:
			case IdMaster:
				PostMessageA(hwnd, WM_APP + 1, LOWORD(wParam), 0);
				return 0;
			case IdLoad: loadPreset(); return 0;
			case IdSaveAs: saveAs(); return 0;
			case IdApply: applyAndSave(); return 0;
#ifdef RIVACROSS_OCAT
			case IdLaunch: launchGame(); return 0;
			case IdAttach: attachRunning(); return 0;
#endif
			case IdVisible:
				app.hotkeyState.visible = !app.hotkeyState.visible;
				updateOverlay();
				SetWindowTextA(app.visibleButton, app.hotkeyState.visible ? "Hide" : "Show");
				return 0;
			case IdClipboard:
				if (MessageBoxA(hwnd, "Clear the Windows clipboard?", "Confirm",
					MB_YESNO | MB_ICONWARNING) == IDYES) {
					if (OpenClipboard(hwnd)) {
						EmptyClipboard();
						CloseClipboard();
						status("Clipboard cleared.");
					} else status("Could not open the clipboard.");
				}
				return 0;
			}
		}
		if (LOWORD(wParam) == IdMode && HIWORD(wParam) == CBN_SELCHANGE) {
			app.hotkeyState.mode = (HotkeyMode)SendMessageA(app.mode, CB_GETCURSEL, 0, 0);
			return 0;
		}
		break;
	case WM_APP + 1:
		beginCapture(wParam == IdHotkey ? app.hotkey : app.master);
		return 0;
	case WM_TIMER:
		pollHotkeys();
		return 0;
	case WM_DESTROY:
		KillTimer(hwnd, 1);
#ifdef RIVACROSS_OCAT
		OCATSharedUpdate(&app.config, false);
		OCATSharedClose();
#else
		if (SendMessageA(app.keep, BM_GETCHECK, 0, 0) == BST_CHECKED)
			RTSSUpdateOSDVisibility(&app.config, OsdEntry, true);
		else RTSSReleaseOSDEntry(OsdEntry);
#endif
		PostQuitMessage(0);
		return 0;
	case WM_NCDESTROY:
		if (app.font && app.font != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(app.font);
		return DefWindowProcA(hwnd, message, wParam, lParam);
	}
	return DefWindowProcA(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
	WNDCLASSA wc = {};
	wc.lpfnWndProc = windowProc;
	wc.hInstance = instance;
	wc.lpszClassName = "RivaCrossWindow";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	if (!RegisterClassA(&wc)) return 1;
	RECT size = { 0, 0, 520, 405 };
	AdjustWindowRect(&size, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
	HWND window = CreateWindowExA(0, wc.lpszClassName,
#ifdef RIVACROSS_OCAT
		"RivaCross (OCAT)",
#else
		"RivaCross",
#endif
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left, size.bottom - size.top,
		NULL, NULL, instance, NULL);
	if (!window) return 1;
	ShowWindow(window, show);
	UpdateWindow(window);
	MSG message;
	while (GetMessageA(&message, NULL, 0, 0) > 0) {
		if (!IsDialogMessageA(window, &message)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
	}
	return (int)message.wParam;
}
