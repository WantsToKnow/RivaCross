// From the repository root (no RTSS, game, or physical key input required):
// g++ -Iinclude tests/hotkey_check.cpp src/Core.cpp src/RTSShared.cpp -o obj/hotkey_check.exe -luser32 -lgdi32 -lcomdlg32 -lcomctl32 -lgdiplus
// g++ -Iinclude -DRIVACROSS_OCAT tests/hotkey_check.cpp src/Core.cpp src/OCATShared.cpp -o obj/hotkey_check_ocat.exe -luser32 -lgdi32 -lcomdlg32
#include "../src/RivaCross.h"
#ifdef RIVACROSS_OCAT
#include "../src/OCATShared.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SHORT keys[256];
static SHORT WINAPI simulatedKeyState(int code) { return keys[code]; }
static bool renderedVisible;
static int rtssUpdates;
static HWND invalidatedWindow;
static BOOL WINAPI trackInvalidation(HWND window, const RECT* rect, BOOL erase) {
	invalidatedWindow = window;
	return InvalidateRect(window, rect, erase);
}
static int simulatedRtssUpdate(const RTSSCrossConfig*, const char*, bool visible) {
	++rtssUpdates;
	renderedVisible = visible;
	return 0;
}
static bool simulatedOcatUpdate(const RTSSCrossConfig*, bool visible) {
	renderedVisible = visible;
	return true;
}
#define GetAsyncKeyState simulatedKeyState
#define GetKeyState simulatedKeyState
#define RTSSUpdateOSDVisibility simulatedRtssUpdate
#define OCATSharedUpdate simulatedOcatUpdate
#define InvalidateRect trackInvalidation
#include "../src/Main.cpp"
#undef GetAsyncKeyState
#undef GetKeyState
#undef RTSSUpdateOSDVisibility
#undef OCATSharedUpdate
#undef InvalidateRect

static void press(int key, bool down) {
	keys[key] = down ? (SHORT)0x8000 : 0;
	pollHotkeys();
}

static LRESULT CALLBACK testWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_COMMAND || message == WM_DRAWITEM || message == WM_HSCROLL)
		return windowProc(window, message, wParam, lParam);
	return DefWindowProcA(window, message, wParam, lParam);
}

#ifndef RIVACROSS_OCAT
static void setSlider(HWND slider, int position) {
	SendMessageA(slider, TBM_SETPOS, TRUE, position);
	SendMessageA(app.window, WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, position), (LPARAM)slider);
}

static void checkLivePreview() {
	const int updates = rtssUpdates;
	SendMessageA(app.shape, CB_SETCURSEL, CrosshairChevron, 0);
	SendMessageA(app.window, WM_COMMAND, MAKEWPARAM(IdShape, CBN_SELCHANGE), (LPARAM)app.shape);
	assert(IsWindowEnabled(app.length) && IsWindowEnabled(app.gap));
	setSlider(app.length, 6);
	setSlider(app.width, 2);
	setSlider(app.gap, 0);
	SetWindowTextA(app.color, "#00FF00FF");
	HDC dc = CreateCompatibleDC(NULL);
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = 224;
	info.bmiHeader.biHeight = -248;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	void* pixels;
	HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
	assert(dc && bitmap);
	HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
	DRAWITEMSTRUCT draw = {};
	draw.CtlID = IdPreview;
	draw.hDC = dc;
	draw.rcItem = {0, 0, 224, 248};
	auto paint = [&] {
		assert(SendMessageA(app.window, WM_DRAWITEM, IdPreview, (LPARAM)&draw) == TRUE);
	};
	paint();
	assert(GetPixel(dc, 112, 98) == RGB(0, 255, 0)); // Apex.
	assert(GetPixel(dc, 88, 122) == RGB(0, 255, 0) && GetPixel(dc, 136, 122) == RGB(0, 255, 0));
	assert(GetPixel(dc, 112, 94) == PreviewBackground);
	invalidatedWindow = NULL;
	SendMessageA(app.outline, BM_SETCHECK, BST_CHECKED, 0);
	SendMessageA(app.window, WM_COMMAND, MAKEWPARAM(IdOutline, BN_CLICKED), (LPARAM)app.outline);
	assert(invalidatedWindow == app.preview);
	paint();
	assert(GetPixel(dc, 112, 94) == RGB(0, 0, 0));
	invalidatedWindow = NULL;
	SetWindowTextA(app.color, "#FF000080");
	assert(invalidatedWindow == app.preview); // Real EN_CHANGE from the edit.
	paint();
	assert(GetPixel(dc, 112, 98) == RGB(182, 58, 62));
	invalidatedWindow = NULL;
	setSlider(app.width, CrosshairHalfWidth);
	assert(invalidatedWindow == app.preview);
	char value[16];
	GetWindowTextA(app.widthValue, value, sizeof(value));
	assert(strcmp(value, "0.5") == 0);
	SetWindowTextA(app.color, "#00FF00FF");
	paint();
	assert(GetPixel(dc, 112, 99) == RGB(0, 255, 0));
	assert(GetPixel(dc, 113, 99) == RGB(0, 0, 0));
	SetWindowTextA(app.color, "#");
	paint();
	assert(GetPixel(dc, 112, 98) == PreviewBackground);
	assert(rtssUpdates == updates && app.config.drawing.style == CrosshairText);
	SetWindowTextA(app.color, "#FFFFFFFF");
	SelectObject(dc, oldBitmap);
	DeleteObject(bitmap);
	DeleteDC(dc);
}
#endif

int main() {
	// Create the real controls in a hidden test window, without startup hooks.
	WNDCLASSA wc = {};
	wc.hInstance = GetModuleHandleA(NULL);
	wc.lpfnWndProc = testWindowProc;
	wc.lpszClassName = "RivaCrossControlCheck";
	assert(RegisterClassA(&wc));
	app.window = CreateWindowExA(0, wc.lpszClassName, "Hotkey check", WS_POPUP,
		0, 0, ClientWidth, ClientHeight, NULL, NULL, wc.hInstance, NULL);
	assert(app.window);
	createControls();
	assert(app.secondaryHotkey && app.master && app.status);
	initConfig(&app.config);
	app.hotkeyState.visible = app.hotkeyState.masterEnabled = true;
	refreshControls();
	for (HWND child = GetWindow(app.window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
		RECT rect;
		assert(GetWindowRect(child, &rect));
		MapWindowPoints(NULL, app.window, (POINT*)&rect, 2);
		assert(rect.left >= 0 && rect.right <= ClientWidth && rect.bottom <= ClientHeight);
	}
	beginCapture(app.hotkey);
	captureProc(app.hotkey, WM_KEYDOWN, VK_F8, 0);
	assert(app.hotkeyState.keyCode == VK_F8);
	beginCapture(app.secondaryHotkey);
	keys[VK_CONTROL] = (SHORT)0x8000;
	captureProc(app.secondaryHotkey, WM_KEYDOWN, VK_F9, 0);
	keys[VK_CONTROL] = 0;
	assert(app.hotkeyState.secondaryKeyCode == VK_F9 && app.hotkeyState.secondaryModifierMask == 1);
	char label[96];
	GetWindowTextA(app.secondaryHotkey, label, sizeof(label));
	assert(strcmp(label, "Ctrl+F9") == 0);
	pollHotkeys();
	assert(app.hotkeyState.visible); // Setting a key must not trigger it.
	press(VK_F8, true);
	assert(!app.hotkeyState.visible);
	pollHotkeys();
	assert(!app.hotkeyState.visible); // Holding a toggle key must not repeat.
	press(VK_F9, true);
	assert(!app.hotkeyState.visible); // The second binding requires its modifier.
	press(VK_CONTROL, true);
	assert(app.hotkeyState.visible); // Second key still triggers while the first is held.
	press(VK_F9, false);
	press(VK_F8, false);
	press(VK_CONTROL, false);

	app.hotkeyState.mode = HotkeyHold;
	pollHotkeys();
	assert(!app.hotkeyState.visible);
	press(VK_F8, true);
	assert(app.hotkeyState.visible);
	press(VK_CONTROL, true);
	press(VK_F9, true);
	press(VK_F8, false);
	assert(app.hotkeyState.visible); // Releasing one binding keeps the other active.
	press(VK_F9, false);
	assert(!app.hotkeyState.visible);
	press(VK_CONTROL, false);

	app.hotkeyState.mode = HotkeyToggle;
	app.hotkeyState.masterKeyCode = VK_F10;
	press(VK_F10, true);
	press(VK_F10, false);
	assert(!app.hotkeyState.masterEnabled);
	assert(!crosshairVisible() && !renderedVisible && !IsWindowEnabled(app.visibleButton));
	GetWindowTextA(app.visibleButton, label, sizeof(label));
	assert(strcmp(label, "Master off") == 0);
	press(VK_F8, true);
	press(VK_F8, false);
	press(VK_CONTROL, true);
	press(VK_F9, true);
	press(VK_F9, false);
	press(VK_CONTROL, false);
	assert(!app.hotkeyState.visible);
	press(VK_F10, true);
	press(VK_F10, false);
	assert(app.hotkeyState.masterEnabled && crosshairVisible() && renderedVisible);
	assert(IsWindowEnabled(app.visibleButton));
	press(VK_CONTROL, true);
	press(VK_F9, true);
	assert(!crosshairVisible() && !renderedVisible); // Hotkeys work again after master reset.
	press(VK_F9, false);
	press(VK_CONTROL, false);
	// Hidden -> master OFF -> master ON always resets the crosshair to visible.
	press(VK_F10, true);
	press(VK_F10, false);
	assert(!crosshairVisible());
	press(VK_F10, true);
	press(VK_F10, false);
	assert(crosshairVisible() && renderedVisible);
	// Visible -> master OFF must immediately hide, even during settings updates.
	press(VK_F10, true);
	pollHotkeys();
	assert(!app.hotkeyState.masterEnabled && !renderedVisible);
	press(VK_F10, false);
	windowProc(app.window, WM_COMMAND, MAKEWPARAM(IdVisible, BN_CLICKED), 0);
	updateOverlay();
	refreshControls();
	assert(!crosshairVisible() && !renderedVisible && !IsWindowEnabled(app.visibleButton));
	press(VK_F8, true);
	press(VK_F8, false);
	press(VK_CONTROL, true);
	press(VK_F9, true);
	press(VK_F9, false);
	press(VK_CONTROL, false);
	assert(!crosshairVisible() && !renderedVisible);
	// Master works independently of the main hotkey mode.
	app.hotkeyState.mode = HotkeyNone;
	press(VK_F10, true);
	press(VK_F10, false);
	assert(crosshairVisible() && renderedVisible);
	press(VK_F10, true);
	press(VK_F10, false);
	assert(!crosshairVisible() && !renderedVisible);
	press(VK_F10, true);
	press(VK_F10, false);
	// Hold mode stays hidden with no binding held, even on the master ON tick.
	app.hotkeyState.mode = HotkeyHold;
	pollHotkeys();
	assert(!crosshairVisible());
	press(VK_F10, true);
	press(VK_F10, false);
	press(VK_F10, true);
	assert(app.hotkeyState.masterEnabled && !crosshairVisible() && !renderedVisible);
	press(VK_F10, false);
	pollHotkeys();
	pollHotkeys();
	assert(!crosshairVisible() && !renderedVisible);
	press(VK_F9, true); // Missing Ctrl must not show the crosshair.
	assert(!crosshairVisible() && !renderedVisible);
	press(VK_CONTROL, true);
	assert(crosshairVisible() && renderedVisible);
	press(VK_F9, false);
	press(VK_CONTROL, false);
	assert(!crosshairVisible() && !renderedVisible);
	// A hotkey held during master ON stays visible until released.
	press(VK_F10, true);
	press(VK_F10, false);
	press(VK_F8, true);
	assert(!crosshairVisible());
	press(VK_F10, true);
	assert(crosshairVisible() && renderedVisible);
	press(VK_F10, false);
	assert(crosshairVisible());
	press(VK_F8, false);
	assert(!crosshairVisible());
	// Rebinding master in hold mode also cannot briefly show an unheld crosshair.
	press(VK_F10, true);
	press(VK_F10, false);
	beginCapture(app.master);
	captureProc(app.master, WM_KEYDOWN, VK_F10, 0);
	assert(app.hotkeyState.masterEnabled && !crosshairVisible() && !renderedVisible);
	pollHotkeys();
	app.hotkeyState.mode = HotkeyToggle;
	// Rebinding or clearing master while OFF must reopen the output immediately.
	press(VK_F10, true);
	press(VK_F10, false);
	beginCapture(app.master);
	captureProc(app.master, WM_KEYDOWN, VK_F11, 0);
	assert(crosshairVisible() && renderedVisible && IsWindowEnabled(app.visibleButton));
	press(VK_F11, false);
	press(VK_F11, true);
	press(VK_F11, false);
	assert(!crosshairVisible());
	beginCapture(app.master);
	captureProc(app.master, WM_KEYDOWN, VK_BACK, 0);
	assert(app.hotkeyState.masterKeyCode == 0 && crosshairVisible() && renderedVisible);
	app.hotkeyState.masterKeyCode = VK_F10;
	pollHotkeys();

	// Capture suppresses both hotkeys, Escape keeps a binding, and mouse input works.
	beginCapture(app.secondaryHotkey);
	press(VK_F8, true);
	assert(app.hotkeyState.visible);
	press(VK_F8, false);
	captureProc(app.secondaryHotkey, WM_KEYDOWN, VK_ESCAPE, 0);
	assert(app.hotkeyState.secondaryKeyCode == VK_F9);
	beginCapture(app.secondaryHotkey);
	captureProc(app.secondaryHotkey, WM_XBUTTONUP, MAKEWPARAM(0, XBUTTON1), 0);
	assert(app.hotkeyState.secondaryKeyCode == VK_XBUTTON1);
	beginCapture(app.hotkey);
	captureProc(app.hotkey, WM_KEYDOWN, VK_BACK, 0);
	assert(app.hotkeyState.keyCode == 0 && app.hotkeyState.mode == HotkeyToggle);
	assert(readControls()); // The second binding can be used on its own.
	press(VK_XBUTTON1, false);
	press(VK_XBUTTON1, true);
	assert(!app.hotkeyState.visible);
	press(VK_XBUTTON1, false);
	beginCapture(app.secondaryHotkey);
	captureProc(app.secondaryHotkey, WM_KEYDOWN, VK_BACK, 0);
	assert(app.hotkeyState.secondaryKeyCode == 0 && app.hotkeyState.mode == HotkeyNone);

	// Round-trip both bindings and offsets, then load old and malformed presets.
	char tempDir[MAX_PATH], path[MAX_PATH];
	assert(GetTempPathA(MAX_PATH, tempDir));
	assert(GetTempFileNameA(tempDir, "rhc", 0, path));
	app.hotkeyState.mode = HotkeyToggle;
	app.hotkeyState.keyCode = VK_RBUTTON;
	app.hotkeyState.secondaryKeyCode = VK_F9;
	app.hotkeyState.secondaryModifierMask = 5;
	app.config.pixelOffset[0] = 4;
	app.config.pixelOffset[1] = -13;
#ifndef RIVACROSS_OCAT
	assert(!IsWindowEnabled(app.width) && IsWindowEnabled(app.crosshair));
	SendMessageA(app.shape, CB_SETCURSEL, CrosshairDot, 0);
	windowProc(app.window, WM_COMMAND, MAKEWPARAM(IdShape, CBN_SELCHANGE), (LPARAM)app.shape);
	assert(IsWindowEnabled(app.width) && !IsWindowEnabled(app.length));
	assert(!IsWindowEnabled(app.scale) && !IsWindowEnabled(app.crosshair));
	checkLivePreview();
	setSlider(app.length, 12);
	setSlider(app.width, CrosshairHalfWidth);
	setSlider(app.gap, 4);
	SendMessageA(app.outline, BM_SETCHECK, BST_CHECKED, 0);
	assert(readControls());
#endif
	assert(saveConfigToFile(path, &app.config, &app.hotkeyState));
	RTSSCrossConfig loaded = {};
	HotkeyState hotkey = {};
	assert(loadConfig(path, &loaded, &hotkey) == 0);
	assert(hotkey.keyCode == VK_RBUTTON && hotkey.secondaryKeyCode == VK_F9);
	assert(hotkey.secondaryModifierMask == 5 && hotkey.masterKeyCode == VK_F10);
	assert(loaded.pixelOffset[0] == 4 && loaded.pixelOffset[1] == -13);
#ifndef RIVACROSS_OCAT
	assert(loaded.drawing.style == CrosshairChevron && loaded.drawing.length == 12 &&
		loaded.drawing.width == CrosshairHalfWidth && loaded.drawing.gap == 4 && loaded.drawing.outline);
	assert(strcmp(loaded.crosshair, "O") == 0 && loaded.scale == 100);
	app.config = loaded;
	refreshControls();
	assert(SendMessageA(app.shape, CB_GETCURSEL, 0, 0) == CrosshairChevron);
	assert(sliderValue(app.width) == CrosshairHalfWidth);
	assert(SendMessageA(app.outline, BM_GETCHECK, 0, 0) == BST_CHECKED);
#endif
	const char* oldPresets[] = {
		"O\n477\n265\n100\n#ffffffff\n",
		"O\n477\n265\n100\n#ffffffff\n2\n2\n",
		"O\n477\n265\n100\n#ffffffff\n2\n2\n0\n4\n0\n",
		"O\n477\n265\n100\n#ffffffff\n2\n2\n0\n4\n0\n4\n-13\n"
	};
	for (const char* preset : oldPresets) {
		FILE* file = fopen(path, "w");
		assert(file);
		fputs(preset, file);
		assert(fclose(file) == 0);
		hotkey.secondaryKeyCode = VK_F9;
		assert(loadConfig(path, &loaded, &hotkey) == 0);
		assert(loaded.drawing.style == CrosshairText);
		assert(hotkey.secondaryKeyCode == 0 && hotkey.secondaryModifierMask == 0);
	}
	const char* invalidTails[] = { "120\n", "255\n0\n", "120\n8\n", "120\n0\nextra\n",
		"120\n0\n1\n6\n2\n2\n", "120\n0\n1\n6\n17\n2\n1\n" };
	for (const char* tail : invalidTails) {
		FILE* file = fopen(path, "w");
		assert(file);
		fputs(oldPresets[3], file);
		fputs(tail, file);
		assert(fclose(file) == 0);
		hotkey.secondaryKeyCode = VK_F9;
		assert(loadConfig(path, &loaded, &hotkey) != 0);
		assert(hotkey.secondaryKeyCode == VK_F9); // Failed reads leave state intact.
	}
	assert(DeleteFileA(path));
	DestroyWindow(app.window);
	DeleteObject(app.font);
	puts("PASS: hotkeys, visibility, sliders, live preview, and presets");
	return 0;
}
