// From the repository root (no RTSS, game, or physical key input required):
// g++ -Iinclude tests/hotkey_check.cpp src/Core.cpp src/RTSShared.cpp -o obj/hotkey_check.exe -luser32 -lgdi32 -lcomdlg32
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
static int simulatedRtssUpdate(const RTSSCrossConfig*, const char*, bool visible) {
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
#include "../src/Main.cpp"
#undef GetAsyncKeyState
#undef GetKeyState
#undef RTSSUpdateOSDVisibility
#undef OCATSharedUpdate

static void press(int key, bool down) {
	keys[key] = down ? (SHORT)0x8000 : 0;
	pollHotkeys();
}

int main() {
	// Create the real controls in a hidden test window, without startup hooks.
	app.window = CreateWindowExA(0, "STATIC", "Hotkey check", WS_POPUP,
		0, 0, 520, 495, NULL, NULL, GetModuleHandleA(NULL), NULL);
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
		assert(rect.left >= 0 && rect.right <= 520 && rect.bottom <=
#ifdef RIVACROSS_OCAT
			495
#else
			445
#endif
		);
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
	assert(saveConfigToFile(path, &app.config, &app.hotkeyState));
	RTSSCrossConfig loaded = {};
	HotkeyState hotkey = {};
	assert(loadConfig(path, &loaded, &hotkey) == 0);
	assert(hotkey.keyCode == VK_RBUTTON && hotkey.secondaryKeyCode == VK_F9);
	assert(hotkey.secondaryModifierMask == 5 && hotkey.masterKeyCode == VK_F10);
	assert(loaded.pixelOffset[0] == 4 && loaded.pixelOffset[1] == -13);
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
		assert(hotkey.secondaryKeyCode == 0 && hotkey.secondaryModifierMask == 0);
	}
	const char* invalidTails[] = { "120\n", "255\n0\n", "120\n8\n", "120\n0\nextra\n" };
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
	puts("PASS: master visibility/reset, both hotkeys, hold/toggle, modifiers, capture, layout, and presets");
	return 0;
}
