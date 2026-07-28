/*
MIT License

Copyright (c) 2020 Irseny

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "RTSShared.h"

extern const char RivaCrossOSDEntryName[32];

enum CrossActionType {
	None,
	ExitClean,
	ExitHard,
	Rename,
	Move,
	Scale,
	Tint,
	Load,
	Update,
	ConfigHotkey,
	ConfigMasterHotkey
};

enum HotkeyMode {
	HotkeyNone,   // hotkey disabled
	HotkeyHold,   // hold to show, release to hide
	HotkeyToggle  // press to toggle show/hide
};

typedef struct HotkeyState {
	HotkeyMode mode;
	int keyCode;           // VK_xxx virtual key code (0 = unset)
	int modifierMask;      // bit0=Ctrl(1), bit1=Shift(2), bit2=Alt(4)
	bool visible;          // current crosshair visibility
	bool prevAsyncState;   // previous frame key state (for Toggle edge detection)
	bool threadRunning;    // signal to hotkey polling thread to stop
	// Master toggle: when disabled, main hotkey is ignored
	int masterKeyCode;           // VK code for master toggle (0 = disabled)
	int masterModifierMask;      // modifiers for master toggle
	bool masterEnabled;          // current master state (true = hotkeys work)
	bool masterPrevAsyncState;   // edge detection for master toggle
} HotkeyState;

typedef struct CrossActionArgs {
	CrossActionType action;
	int deltaX;
	int deltaY;
	int deltaScale;
	int skippedInput;
	HotkeyState* hotkeyState; // pointer to hotkey state for ConfigHotkey action
} CrossActionArgs;

typedef struct RenderPassState {
	int linesFilled;
} RenderPassState;

void helpWelcome(RenderPassState* state);
void helpCommands(RenderPassState* state);
void helpActiveConfig(RTSSCrossConfig* config, HotkeyState* hs, RenderPassState* state);

RTSSCrossConfig configureStart(int argc, char** argv, RenderPassState* state, HotkeyState* hs);
bool makeConfigPath(char* configName, char* configPath);
bool initConfig(RTSSCrossConfig* config);
int loadConfig(char* configName, RTSSCrossConfig* config, HotkeyState* hs);
bool pollUserAction(CrossActionArgs* args);

bool processUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processUpdateUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processRenameUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processLoadUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processTintUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processMoveUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processScaleUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processHotkeyUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
bool processMasterHotkeyUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state);
int imin(int i1, int i2);

CrossActionType runInteractiveProgram(RTSSCrossConfig* config, RenderPassState* state, HotkeyState* hs);

bool saveConfigToFile(const char* configPath, RTSSCrossConfig* config, HotkeyState* hs);
void hotkeyVkCodeToName(int vkCode, char* outName, int outNameSize);
bool isHotkeyActive(HotkeyState* hs);
void startHotkeyThread(HotkeyState* hs, RTSSCrossConfig* config, const char* osdEntryName);
void stopHotkeyThread(HotkeyState* hs);
DWORD WINAPI hotkeyPollThread(LPVOID param);
