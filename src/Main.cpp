#include "RivaCross.h"

#include <commdlg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ControlId {
	IdCrosshair = 101, IdX, IdY, IdScale, IdColor, IdMode,
	IdHotkey, IdMaster, IdLoad, IdSaveAs, IdApply, IdVisible,
	IdClipboard, IdKeep, IdStatus
};

static const char* const OsdEntry = "RivaCross_crosshair";

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

static void updateOverlay() {
	int result = RTSSUpdateOSDVisibility(&app.config, OsdEntry, app.hotkeyState.visible);
	status(result == 0 ? "Sent to RTSS; game display is not verified." :
		result == -4 ? "RTSS is busy. Click Apply + Save again." :
		"RTSS is unavailable. Start RTSS, then click Apply + Save.");
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
			RTSSUpdateOSDVisibility(&app.config, OsdEntry, visible);
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
	app.keep = control("BUTTON", "Leave crosshair visible when closing",
		WS_TABSTOP | BS_AUTOCHECKBOX, 190, 315, 310, 28, IdKeep);
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
		if (SendMessageA(app.keep, BM_GETCHECK, 0, 0) == BST_CHECKED)
			RTSSUpdateOSDVisibility(&app.config, OsdEntry, true);
		else RTSSReleaseOSDEntry(OsdEntry);
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
	HWND window = CreateWindowExA(0, wc.lpszClassName, "RivaCross",
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
