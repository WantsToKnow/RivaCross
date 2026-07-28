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

#include "stdafx.h"
#include "RTSShared.h"
#include "RivaCross.h"

const char RivaCrossOSDEntryName[32] = "RivaCross_crosshair";
static char g_activeConfigName[32] = "default";

void helpWelcome(RenderPassState* state) {
	printw("Welcome to RivaCross\n");
	state->linesFilled += 1;
}
void helpCommands(RenderPassState* state) {
	// TODO use lines filled state or remove it entirely
	//move(state->linesFilled, 0);
	printw("Commands:\n");
	printw("Q - quit RivaCross\n");
	printw("CTRL + Q - quit RivaCross but retain a visible crosshair\n");
	printw("CTRL + O - load an existing config\n");
	printw("T - set the crosshair character/text\n");
	printw("NUM- and NUM+ - change the crosshair size\n");
	printw("S - set the crosshair size\n");
	printw("C - set the crosshair color\n");
	printw("Direction keys - move the crosshair on screen\n");
	printw("M - set the crosshair position\n");
	printw("U - retransmit the current config to RTSS\n");
	printw("H - configure global hotkey toggle\n");
	printw("K - configure master toggle (disables hotkey when off)\n");
	state->linesFilled += 13;
}

void helpActiveConfig(RTSSCrossConfig* config, HotkeyState* hs, RenderPassState* state) {
	//move(state->linesFilled, 0);
	printw("Active config:\n");
	printw("Crosshair: %s\n", config->crosshair);
	printw("Position: %d %d\n", config->position[0], config->position[1]);
	printw("Size: %d\n", config->scale);
	printw("Color: #%02x%02x%02x%02x\n", config->color[0], config->color[1], config->color[2], config->color[3]);
	if (hs != NULL && hs->mode != HotkeyNone) {
		const char* modeStr = (hs->mode == HotkeyHold) ? "Hold" : "Toggle";
		char keyName[32];
		hotkeyVkCodeToName(hs->keyCode, keyName, sizeof(keyName));
		char modPrefix[32] = "";
		if (hs->modifierMask & 1) strcat(modPrefix, "Ctrl+");
		if (hs->modifierMask & 2) strcat(modPrefix, "Shift+");
		if (hs->modifierMask & 4) strcat(modPrefix, "Alt+");
		printw("Hotkey: %s%s (%s mode)\n", modPrefix, keyName, modeStr);
	} else {
		printw("Hotkey: disabled\n");
	}
	// Master toggle status
	if (hs != NULL && hs->masterKeyCode != 0) {
		char keyName[32];
		hotkeyVkCodeToName(hs->masterKeyCode, keyName, sizeof(keyName));
		char modPrefix[32] = "";
		if (hs->masterModifierMask & 1) strcat(modPrefix, "Ctrl+");
		if (hs->masterModifierMask & 2) strcat(modPrefix, "Shift+");
		if (hs->masterModifierMask & 4) strcat(modPrefix, "Alt+");
		printw("Master: %s%s (%s)\n", modPrefix, keyName,
			hs->masterEnabled ? "ON" : "OFF");
	} else {
		printw("Master: not set\n");
	}
	state->linesFilled += 7;
}

bool initConfig(RTSSCrossConfig* config) {
	if (config == NULL) {
		return false;
	}
	strcpy(config->crosshair, "O");
	config->position[0] = 477;
	config->position[1] = 265;
	config->color[0] = 0xFF;
	config->color[1] = 0xFF;
	config->color[2] = 0xFF;
	config->color[3] = 0xFF;
	config->scale = 100;
	return true;
}
int loadConfig(char* configPath, RTSSCrossConfig* config, HotkeyState* hs) {
	if (configPath == NULL || config == NULL) {
		return -1;
	}
	// attempt to read from file
	FILE* file = fopen(configPath, "r");
	if (file == NULL) {
		return -2;
	}
	bool complete = false;
	char line[32];
	char crosshair[32];
	unsigned int posX, posY, scale;
	unsigned int red, green, blue, alpha;
	do {
		// read crosshair line
		char* content = fgets(line, sizeof(line), file);
		if (!content) {
			break;
		}
		// exclude everything from the first newline character
		int lineLength = strlen(content);
		int crosshairLength = 0;
		for (; crosshairLength < lineLength; crosshairLength++) {
			if (content[crosshairLength] == '\n' ||
				content[crosshairLength] == '\r') {
				break;
			}
		}
		crosshairLength = imin(crosshairLength, sizeof(crosshair) - 1);
		strncpy(crosshair, content, crosshairLength);
		crosshair[crosshairLength] = '\0';
		
		// read position x
		content = fgets(line, sizeof(line), file);
		if (!content) {
			break;
		}
		if (sscanf(content, "%i", &posX) < 1) {
			break;
		}
		// read position y
		content = fgets(line, sizeof(line), file);
		if (!content) {
			break;
		}
		if (sscanf(content, "%i", &posY) < 1) {
			break;
		}
		// read scale
		content = fgets(line, sizeof(line), file);
		if (!content) {
			break;
		}
		if (sscanf(content, "%i", &scale) < 1) {
			break;
		}
		// read color
		content = fgets(line, sizeof(line), file);
		if (!content) {
			break;
		}
		int colorLength = strlen(content);
		// require red at index 1 and 2, green at 3 and 4 ...
		if (colorLength < 7) {
			break;
		}
		int colorNo = sscanf(content, "#%02x%02x%02x%02x", &red, &green, &blue, &alpha);
		if (colorNo < 3) {
			break;
		}
		else if (colorNo < 4) {
			alpha = 0xFF;
		}
		complete = true;
	} while (false);

	// read optional hotkey configuration (lines 6-7)
	if (hs != NULL && complete) {
		int hotkeyMode = 0;
		int hotkeyCode = 0;
		// line 6: hotkey mode (0=none, 1=hold, 2=toggle)
		char* content = fgets(line, sizeof(line), file);
		if (content && sscanf(content, "%i", &hotkeyMode) >= 1) {
			// line 7: hotkey VK code
			content = fgets(line, sizeof(line), file);
			if (content && sscanf(content, "%i", &hotkeyCode) >= 1) {
				if (hotkeyMode >= HotkeyNone && hotkeyMode <= HotkeyToggle) {
					hs->mode = (HotkeyMode)hotkeyMode;
				}
				hs->keyCode = hotkeyCode;
				hs->visible = true;
				hs->prevAsyncState = false;
				// line 8: modifier mask (optional, defaults to 0)
				content = fgets(line, sizeof(line), file);
				if (content) {
					int modifierMask = 0;
					if (sscanf(content, "%i", &modifierMask) >= 1) {
						hs->modifierMask = modifierMask;
					}
				}
				// line 9: master toggle key code (optional, defaults to 0)
				content = fgets(line, sizeof(line), file);
				if (content && sscanf(content, "%i", &hotkeyCode) >= 1) {
					hs->masterKeyCode = hotkeyCode;
					// line 10: master toggle modifier mask
					content = fgets(line, sizeof(line), file);
					if (content && sscanf(content, "%i", &hotkeyMode) >= 1) {
						hs->masterModifierMask = hotkeyMode;
					}
				}
			}
		}
	}

	fclose(file);
	// apply values after reading successfully
	if (complete) {

		strncpy(config->crosshair, crosshair, imin(sizeof(config->crosshair) - 1, sizeof(crosshair) - 1));
		config->crosshair[sizeof(config->crosshair) - 1] = '\0';
		config->position[0] = posX;
		config->position[1] = posY;
		config->scale = scale;
		config->color[0] = red;
		config->color[1] = green;
		config->color[2] = blue;
		config->color[3] = alpha;
		return 0;
	} else {
		return -3;
	}
}
bool makeConfigPath(char* configName, char* configPath) {
	if (configName == NULL || configPath == NULL) {
		return false;
	}
	char prefix[] = "config";
	char extension[] = "txt";
	sprintf(configPath, "%s/%s.%s", prefix, configName, extension);
	return true;
}
RTSSCrossConfig configureStart(int argc, char** argv, RenderPassState* state, HotkeyState* hs) {
	//move(state->linesFilled, 0);
	char configName[32] = "default";
	// determine config usage
	if (argc < 2) {
		printw("Using default config\n");
		strncpy(g_activeConfigName, "default", sizeof(g_activeConfigName) - 1);
	} else {
		strncpy(configName, argv[1], sizeof(configName) - 1);
		configName[sizeof(configName) - 1] = '\0';
		strncpy(g_activeConfigName, configName, sizeof(g_activeConfigName) - 1);
		printw("Using config %s\n", configName);
	}
	state->linesFilled += 1;
	char configPath[256] = "";
	makeConfigPath(configName, configPath);
	RTSSCrossConfig config;
	if (loadConfig(configPath, &config, hs) != 0) {
		printw("Failed to load config from %s\n", configPath);
		initConfig(&config);
		printw("Continuing with stock config\n");
		state->linesFilled += 2;
	}
	return config;
}
bool pollUserAction(CrossActionArgs* args) {
	if (args == NULL) {
		return false;
	}
	// ctrl+char is observed if bits 6 and 7 are not set
	const int ctrlMod = 0x60;
	// TODO enable after debugging
	noecho();
	int input = getch();
	if (input == ERR) {
		return false; // non-blocking: no input available
	}
	switch (input) {
		case ctrlMod^'c':
		case KEY_EXIT:
		case 'q':
			args->action = CrossActionType::ExitClean;
			break;
		case ctrlMod^'q':
			args->action = CrossActionType::ExitHard;
			break;
		case 't':
			args->action = CrossActionType::Rename;
			break;
		case 'm':
			args->action = CrossActionType::Move;
			break;
		case 's':
			args->action = CrossActionType::Scale;
			break;
		case 'c':
			args->action = CrossActionType::Tint;
			break;
		case ctrlMod^'o':
			args->action = CrossActionType::Load;
			break;
		case 'u':
			args->action = CrossActionType::Update;
			break;
		case 'h':
			args->action = CrossActionType::ConfigHotkey;
			break;
		case 'k':
			args->action = CrossActionType::ConfigMasterHotkey;
			break;
		// move
		case KEY_UP:
			args->deltaY = -1;
			break;
		case CTL_UP:
			args->deltaY = -10;
			break;
		case KEY_DOWN:
			args->deltaY = 1;
			break;
		case CTL_DOWN:
			args->deltaY = 10;
			break;
		case KEY_RIGHT:
			args->deltaX = 1;
			break;
		case CTL_RIGHT:
			args->deltaX = 10;
			break;
		case KEY_LEFT:
			args->deltaX = -1;
			break;
		case CTL_LEFT:
			args->deltaX = -10;
			break;
		// scale
		case PADMINUS:
			args->deltaScale = -1;
			break;
		case CTL_PADMINUS:
			args->deltaScale = -10;
			break;
		case PADPLUS:
			args->deltaScale = 1;
			break;
		case CTL_PADPLUS:
			args->deltaScale = 10;
			break;
		// special cases
		case KEY_RESIZE:
			resize_term(0, 0);
			break;
		default:
			// skip anything else
			args->skippedInput = input;
			break;
	}
	// hint at that the user has input something
	bool toProcess = (args->deltaScale != 0) || (args->deltaX != 0) || (args->deltaY != 0) ||
		(args->action != CrossActionType::None) || (args->skippedInput != 0);
	return toProcess;
}

bool processUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	// skip one line
	// it does sometimes contain user input which makes printing buggy
	addch('\n');
	state->linesFilled += 1;
	bool changed = false;
	// debug output for key combinations
	if (false && args->skippedInput != 0) {
		//move(state->linesFilled, 0);
		printw("Skipped input: %i|0x%x (", args->skippedInput, args->skippedInput);
		addch(args->skippedInput>>24);
		addch(' ');
		addch(args->skippedInput>>16);
		addch(' ');
		addch(args->skippedInput>>8);
		addch(' ');
		addch(args->skippedInput);
		addstr(")\n");
		args->skippedInput = 0;
		state->linesFilled += 1;
	}
	switch (args->action) {
	case CrossActionType::None:
		break;
	case CrossActionType::Move:
		changed = processMoveUserAction(args, config, state);
		break;
	case CrossActionType::Scale:
		changed = processScaleUserAction(args, config, state);
		break;
	case CrossActionType::Tint:
		changed = processTintUserAction(args, config, state);
		break;
	case CrossActionType::Load:
		changed = processLoadUserAction(args, config, state);
		break;
	case CrossActionType::Rename:
		changed = processRenameUserAction(args, config, state);
		break;
	case CrossActionType::Update:
		changed = processUpdateUserAction(args, config, state);
		break;
	case CrossActionType::ConfigHotkey:
		changed = processHotkeyUserAction(args, config, state);
		break;
	case CrossActionType::ConfigMasterHotkey:
		changed = processMasterHotkeyUserAction(args, config, state);
		break;
	default:
		break;
	}
	// move 
	if (args->deltaX != 0) {
		config->position[0] += args->deltaX;
		if ((int)config->position[0] < 0) {
			config->position[0] = 0;
		}
		args->deltaX = 0;
		changed = true;
	}
	if (args->deltaY != 0) {
		config->position[1] += args->deltaY;
		if ((int)config->position[1] < 0) {
			config->position[1] = 0;
		}
		args->deltaY = 0;
		changed = true;
	}
	// scale
	if (args->deltaScale != 0) {
		config->scale += args->deltaScale;
		if ((int)config->scale < 0) {
			config->scale = 0;
		}
		args->deltaScale = 0;
		changed = true;
	}
	return changed;
}
bool processUpdateUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	args->action = CrossActionType::None;
	printw("Resending active config to RTSS\n");
	state->linesFilled += 1;
	// indicate that an update of RTSS state is required
	return true;
}
bool processTintUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	bool changed = false;
	//move(state->linesFilled, 0);
	printw("Current color is #%02x%02x%02x%02x\n", config->color[0], 
		config->color[1], config->color[2], config->color[3]);
	printw("Enter new color (r, g, b, a) or (rrggbbaa): #");
	refresh();
	char inputBuffer[256];
	echo();
	getstr(inputBuffer);
	if (!changed) {
		// search for r, g, b pattern
		int r, g, b, a;
		int colorNo = sscanf(inputBuffer, "%03i,%03i,%03i,%03i", &r, &g, &b, &a);
		if (colorNo >= 3) {
			if (r <= 0xFF && g <= 0xFF && b <= 0xFF && a <= 0xFF) {
				config->color[0] = r;
				config->color[1] = g;
				config->color[2] = b;
				if (colorNo > 3) {
					config->color[3] = a;
				} else {
					config->color[3] = 0xFF;
				}
				changed = true;
			}
		}
	}
	
	if (!changed) {
		// search for #rrggbb pattern
		int r, g, b, a;
		int colorNo = sscanf(inputBuffer, "%02x%02x%02x%02x", &r, &g, &b, &a);
		if (colorNo >= 3) {
			config->color[0] = r;
			config->color[1] = g;
			config->color[2] = b;
			if (colorNo > 3) {
				config->color[3] = a;
			} else {
				config->color[3] = 0xFF;
			}
			changed = true;
		}
	}
	if (!changed) {
		printw("Wrong format! Please specify your desired color as ");
		printw("three or four comma seperated values in [0-255] (e.g. 128,234,56) or ");
		printw("a hexadecimal representation (e.g. 70F03AFF)\n");
	}
	// we expect some lines to wrap so we get the actual number of lines written from curses
	int cursorX, cursorY;
	getsyx(cursorY, cursorX);
	state->linesFilled = cursorY;
	args->action = CrossActionType::None;
	// indicate if an update of RTSS memory is required
	return changed;
}
bool processMoveUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	printw("Current position is %i, %i\n", config->position[0], config->position[1]);
	printw("Use the direction keys and exit with ENTER or\n");
	printw("Enter new position (x, y): ");
	refresh();
	
	// get user input
	// first allow crosshair movement with the direction keys
	// but enter number input mode as soon as a number key is pressed
	const int ctrlMod = 0x60; 
	bool numberStarted = false;
	bool moveOrdered = false;
	bool moveAborted = false;
	nodelay(stdscr, true);
	while (!numberStarted && !moveOrdered && !moveAborted) {
		int input = getch();
		// scan for direction keys
		// these also activate if the console window is out of focus
		bool moved = false;
		int modifier = 1;
		if (GetKeyState(VK_CONTROL) & 0x8000) {
			modifier = 10;
		}
		if (GetKeyState(VK_LEFT) & 0x8000) {
			args->deltaX = -modifier;
			moved = true;
		}
		if (GetKeyState(VK_RIGHT) & 0x8000) {
			args->deltaX = modifier;
			moved = true;
		}
		if (GetKeyState(VK_UP) & 0x8000) {
			args->deltaY = -modifier;
			moved = true;
		}
		if (GetKeyState(VK_DOWN) & 0x8000) {
			args->deltaY = modifier;
			moved = true;
		}
		if (GetKeyState(VK_RETURN) & 0x8000) {
			moveAborted = true;
		}
		// process the details outside this function
		// but come back to it (action type is still MOVE)
		if (moved) {
			flushinp(); // throw away repeated direction keys from the input queue
			ungetch('m'); // put something in the queue so that this function is called again
			moveOrdered = true;
		} else {
			switch (input) {
			case 'q':
			case '\n':
			case KEY_ENTER:
			case (ctrlMod^'c'):
				// end position modification with CTRL+C, Q and ENTER
				moveAborted = true;
				break;
			case '0': case '1':
			case '2': case '3':
			case '4': case '5':
			case '6': case '7':
			case '8': case '9':
				// jump to manual specification below
				ungetch(input);
				numberStarted = true;
				break;
			default:
				// ignore everything else
				break;
			}
		}
		Sleep(50); // wait some more so that the user easily do single steps
	}
	nodelay(stdscr, false);
	if (moveAborted) {
		args->action = CrossActionType::None;
		return false;
	}
	if (moveOrdered) {
		return true;
	}
	if (numberStarted) {
		// a number key was pressed
		// continue to read number input until the user hits enter
		bool changed = false;
		char inputBuffer[256];
		echo();
		getnstr(inputBuffer, sizeof(inputBuffer)-1);
		state->linesFilled += 2;
		// process user input
		int posX, posY;
		if (sscanf(inputBuffer, "%i,%i", &posX, &posY) < 2) {
			printw("Wrong format! Please specify your desired position as ");
			printw("two comma separated integers (e.g. 477, 245)\n");
		} else if (posX < 0 || posY < 0) {
			printw("Wrong format! Please use only positive valus\n");
		} else {
			config->position[0] = posX;
			config->position[1] = posY;
			changed = true;
			printw("New position is %i, %i\n", config->position[0], config->position[1]);
		}
		int cursorX, cursorY;
		getsyx(cursorY, cursorX);
		state->linesFilled = cursorY;
		args->action = CrossActionType::None;
		return changed;
	}
	args->action = CrossActionType::None;
	return false;
}
bool processScaleUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	bool changed = false;
	// get user input
	printw("Current size is %i\n", config->scale);
	printw("Enter new size: ");
	refresh();
	char inputBuffer[16];
	echo();
	getnstr(inputBuffer, sizeof(inputBuffer)-1);
	state->linesFilled += 2;
	// process user input
	int scale;
	if (sscanf(inputBuffer, "%i", &scale) < 1) {
		printw("Wrong format! Please specify your desired size as a single integer\n");
	} else {
		if (scale < 0) {
			printw("Wrong format! Cannot have a negative crosshair size\n");
		} else {
			config->scale = scale;
			changed = true;
			printw("New size is %i\n", config->scale);
		}
	}
	state->linesFilled += 1;
	args->action = CrossActionType::None;
	return changed;
}
bool processRenameUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	// get user input
	printw("Current crosshair is %s\n", config->crosshair);
	printw("Enter new crosshair: ");
	refresh();
	char inputBuffer[64];
	echo();
	getnstr(inputBuffer, sizeof(inputBuffer) - 1);
	state->linesFilled += 2;
	// process user input
	strncpy(config->crosshair, inputBuffer, imin(sizeof(config->crosshair) - 1, sizeof(inputBuffer)));
	config->crosshair[sizeof(config->crosshair) - 1] = '\0';
	printw("New crosshair is %s\n", config->crosshair);
	state->linesFilled += 1;
	args->action = CrossActionType::None;
	return true;
}
bool processLoadUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	args->action = CrossActionType::None;
	// get user input
	printw("Enter config to load: ");
	refresh();
	char inputBuffer[128];
	echo();
	getnstr(inputBuffer, sizeof(inputBuffer)-1);
	state->linesFilled += 1;
	// process user input
	char configPath[256];
	if (!makeConfigPath(inputBuffer, configPath)) {
		printw("Internal error\n");
		state->linesFilled += 1;
		return false;
	}
	if (loadConfig(configPath, config, NULL) != 0) {
		printw("Failed to load config from %s\n", configPath);
		printw("Please state your desired config without file extension (.txt) ");
		printw("and make sure it is formatted correctly\n");
		state->linesFilled += 1;
		return false;
	}
	// Remember the loaded config name for auto-save
	strncpy(g_activeConfigName, inputBuffer, sizeof(g_activeConfigName) - 1);
	g_activeConfigName[sizeof(g_activeConfigName) - 1] = '\0';
	return true;
}
bool processHotkeyUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	args->action = CrossActionType::None;
	HotkeyState* hs = args->hotkeyState;
	if (hs == NULL) {
		printw("Internal error: hotkey state not available\n");
		state->linesFilled += 1;
		return false;
	}

	// Show current hotkey state
	if (hs->mode == HotkeyNone) {
		printw("Hotkey is currently disabled\n");
	} else {
		const char* modeStr = (hs->mode == HotkeyHold) ? "Hold" : "Toggle";
		char keyName[32];
		hotkeyVkCodeToName(hs->keyCode, keyName, sizeof(keyName));
		char modPrefix[32] = "";
		if (hs->modifierMask & 1) strcat(modPrefix, "Ctrl+");
		if (hs->modifierMask & 2) strcat(modPrefix, "Shift+");
		if (hs->modifierMask & 4) strcat(modPrefix, "Alt+");
		printw("Current hotkey: %s%s, mode=%s\n", modPrefix, keyName, modeStr);
	}
	state->linesFilled += 1;

	// Step 1: Ask user to press the desired hotkey combination
	printw("Press the hotkey combination now (ESC to cancel, BACKSPACE to disable):\n");
	state->linesFilled += 1;
	refresh();

	// Flush pending GetAsyncKeyState "was pressed" flags (include mouse buttons)
	for (int vk = 0x01; vk <= 0xFE; vk++) {
		GetAsyncKeyState(vk);
	}

	// Poll for key press using GetAsyncKeyState (works globally)
	int pressedKey = 0;
	int pressedModifiers = 0;
	const int VK_MODIFIERS[] = { VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN };
	const int SKIP_VK[] = { VK_XBUTTON1, VK_XBUTTON2 };

	int pollCount = 0;
	while (pressedKey == 0 && pollCount < 500) {
		// Check for Esc to cancel
		if (GetAsyncKeyState(VK_ESCAPE) & 0x0001) {
			printw("Hotkey configuration cancelled\n");
			state->linesFilled += 1;
			return false;
		}
		// Check for Backspace to disable
		if (GetAsyncKeyState(VK_BACK) & 0x0001) {
			hs->mode = HotkeyNone;
			hs->keyCode = 0;
			hs->modifierMask = 0;
			printw("Hotkey disabled\n");
			state->linesFilled += 1;
			return true;
		}

		// Scan VK codes for a newly pressed key (0x01-0x07 = mouse, 0x08-0xFE = keyboard)
		for (int vk = 0x01; vk <= 0xFE; vk++) {
			if (!(GetAsyncKeyState(vk) & 0x0001)) continue;

			// Skip modifier keys
			bool isModifier = false;
			for (int m = 0; m < sizeof(VK_MODIFIERS) / sizeof(VK_MODIFIERS[0]); m++) {
				if (vk == VK_MODIFIERS[m]) { isModifier = true; break; }
			}
			if (isModifier) continue;

			// Skip mouse buttons
			bool isMouse = false;
			for (int m = 0; m < sizeof(SKIP_VK) / sizeof(SKIP_VK[0]); m++) {
				if (vk == SKIP_VK[m]) { isMouse = true; break; }
			}
			if (isMouse) continue;

			pressedKey = vk;
			break;
		}

		// Check modifier states at time of key press
		if (pressedKey != 0) {
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000) pressedModifiers |= 1;
			if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) pressedModifiers |= 2;
			if (GetAsyncKeyState(VK_MENU)    & 0x8000) pressedModifiers |= 4;
		}

		Sleep(20);
		pollCount++;
		refresh(); // keep TUI responsive
	}

	if (pressedKey == 0) {
		printw("No key detected (timeout); use config file for manual setup\n");
		state->linesFilled += 1;
		return false;
	}

	char keyName[32];
	hotkeyVkCodeToName(pressedKey, keyName, sizeof(keyName));
	char modPrefix[32] = "";
	if (pressedModifiers & 1) strcat(modPrefix, "Ctrl+");
	if (pressedModifiers & 2) strcat(modPrefix, "Shift+");
	if (pressedModifiers & 4) strcat(modPrefix, "Alt+");
	printw("Detected: %s%s\n", modPrefix, keyName);
	state->linesFilled += 1;

	// Step 2: Ask for mode
	printw("Select mode: 1=Hold, 2=Toggle, 0=Disable: ");
	refresh();

	echo();
	char modeInput[8];
	getnstr(modeInput, sizeof(modeInput) - 1);
	int modeChoice = 0;
	sscanf(modeInput, "%d", &modeChoice);

	bool changed = false;
	switch (modeChoice) {
		case 0:
			hs->mode = HotkeyNone;
			hs->keyCode = 0;
			hs->modifierMask = 0;
			printw("Hotkey disabled\n");
			changed = true;
			break;
		case 1:
			hs->mode = HotkeyHold;
			hs->keyCode = pressedKey;
			hs->modifierMask = pressedModifiers;
			hs->visible = true;
			hs->prevAsyncState = false;
			printw("Hotkey set: %s%s (Hold)\n", modPrefix, keyName);
			changed = true;
			break;
		case 2:
			hs->mode = HotkeyToggle;
			hs->keyCode = pressedKey;
			hs->modifierMask = pressedModifiers;
			hs->visible = true;
			hs->prevAsyncState = false;
			printw("Hotkey set: %s%s (Toggle)\n", modPrefix, keyName);
			changed = true;
			break;
		default:
			printw("Invalid mode selection\n");
			break;
	}
	state->linesFilled += 1;

	// Step 3: Offer to save to config file
	if (changed) {
		printw("Save hotkey to config file? (y/n): ");
		refresh();
		char saveInput[8];
		getnstr(saveInput, sizeof(saveInput) - 1);
		if (saveInput[0] == 'y' || saveInput[0] == 'Y') {
			char configPath[256];
			makeConfigPath(g_activeConfigName, configPath);
			if (saveConfigToFile(configPath, config, hs)) {
				printw("Hotkey config saved to %s\n", configPath);
			} else {
				printw("Failed to save hotkey config\n");
			}
			state->linesFilled += 1;
		}
	}
	return changed;
}

bool processMasterHotkeyUserAction(CrossActionArgs* args, RTSSCrossConfig* config, RenderPassState* state) {
	args->action = CrossActionType::None;
	HotkeyState* hs = args->hotkeyState;
	if (hs == NULL) {
		printw("Internal error: hotkey state not available\n");
		state->linesFilled += 1;
		return false;
	}

	// Show current master toggle state
	if (hs->masterKeyCode != 0) {
		char keyName[32];
		hotkeyVkCodeToName(hs->masterKeyCode, keyName, sizeof(keyName));
		char modPrefix[32] = "";
		if (hs->masterModifierMask & 1) strcat(modPrefix, "Ctrl+");
		if (hs->masterModifierMask & 2) strcat(modPrefix, "Shift+");
		if (hs->masterModifierMask & 4) strcat(modPrefix, "Alt+");
		printw("Current master toggle: %s%s (%s)\n", modPrefix, keyName,
			hs->masterEnabled ? "ON" : "OFF");
	} else {
		printw("Master toggle: not set\n");
	}
	state->linesFilled += 1;

	// Step 1: Capture key for master toggle
	printw("Press key for master toggle (ESC cancel, BACKSPACE disable):\n");
	state->linesFilled += 1;
	refresh();

	// Flush pending GetAsyncKeyState flags
	for (int vk = 0x01; vk <= 0xFE; vk++) {
		GetAsyncKeyState(vk);
	}

	const int VK_MODIFIERS[] = { VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN };
	const int SKIP_VK[] = { VK_XBUTTON1, VK_XBUTTON2 };

	int pressedKey = 0;
	int pressedModifiers = 0;
	int pollCount = 0;
	while (pressedKey == 0 && pollCount < 500) {
		if (GetAsyncKeyState(VK_ESCAPE) & 0x0001) {
			printw("Master toggle configuration cancelled\n");
			state->linesFilled += 1;
			return false;
		}
		if (GetAsyncKeyState(VK_BACK) & 0x0001) {
			hs->masterKeyCode = 0;
			hs->masterModifierMask = 0;
			hs->masterEnabled = true;
			printw("Master toggle disabled\n");
			state->linesFilled += 1;
			return true;
		}
		for (int vk = 0x01; vk <= 0xFE; vk++) {
			if (!(GetAsyncKeyState(vk) & 0x0001)) continue;
			bool isModifier = false;
			for (int m = 0; m < sizeof(VK_MODIFIERS) / sizeof(VK_MODIFIERS[0]); m++) {
				if (vk == VK_MODIFIERS[m]) { isModifier = true; break; }
			}
			if (isModifier) continue;
			bool isMouse = false;
			for (int m = 0; m < sizeof(SKIP_VK) / sizeof(SKIP_VK[0]); m++) {
				if (vk == SKIP_VK[m]) { isMouse = true; break; }
			}
			if (isMouse) continue;
			pressedKey = vk;
			break;
		}
		if (pressedKey != 0) {
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000) pressedModifiers |= 1;
			if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) pressedModifiers |= 2;
			if (GetAsyncKeyState(VK_MENU)    & 0x8000) pressedModifiers |= 4;
		}
		Sleep(20);
		pollCount++;
		refresh();
	}

	if (pressedKey == 0) {
		printw("No key detected (timeout); use config file for manual setup\n");
		state->linesFilled += 1;
		return false;
	}

	char keyName[32];
	hotkeyVkCodeToName(pressedKey, keyName, sizeof(keyName));
	char modPrefix[32] = "";
	if (pressedModifiers & 1) strcat(modPrefix, "Ctrl+");
	if (pressedModifiers & 2) strcat(modPrefix, "Shift+");
	if (pressedModifiers & 4) strcat(modPrefix, "Alt+");
	printw("Master toggle set: %s%s\n", modPrefix, keyName);
	state->linesFilled += 1;

	hs->masterKeyCode = pressedKey;
	hs->masterModifierMask = pressedModifiers;
	hs->masterEnabled = true;
	hs->masterPrevAsyncState = false;

	// Save
	printw("Save to config file? (y/n): ");
	refresh();
	echo();
	char saveInput[8];
	getnstr(saveInput, sizeof(saveInput) - 1);
	if (saveInput[0] == 'y' || saveInput[0] == 'Y') {
		char configPath[256];
		makeConfigPath(g_activeConfigName, configPath);
		if (saveConfigToFile(configPath, config, hs)) {
			printw("Saved to %s\n", configPath);
		} else {
			printw("Failed to save\n");
		}
		state->linesFilled += 1;
	}
	return true;
}

bool saveConfigToFile(const char* configPath, RTSSCrossConfig* config, HotkeyState* hs) {
	if (configPath == NULL || config == NULL || hs == NULL) {
		return false;
	}
	FILE* file = fopen(configPath, "w");
	if (file == NULL) {
		return false;
	}
	fprintf(file, "%s\n", config->crosshair);
	fprintf(file, "%d\n", config->position[0]);
	fprintf(file, "%d\n", config->position[1]);
	fprintf(file, "%u\n", config->scale);
	fprintf(file, "#%02x%02x%02x%02x\n",
		config->color[0], config->color[1],
		config->color[2], config->color[3]);
	fprintf(file, "%d\n", (int)hs->mode);
	fprintf(file, "%d\n", hs->keyCode);
	fprintf(file, "%d\n", hs->modifierMask);
	// Master toggle
	fprintf(file, "%d\n", hs->masterKeyCode);
	fprintf(file, "%d\n", hs->masterModifierMask);
	fclose(file);
	return true;
}

void hotkeyVkCodeToName(int vkCode, char* outName, int outNameSize) {
	if (outName == NULL || outNameSize <= 0) return;

	// Named keys
	struct { int vk; const char* name; } namedKeys[] = {
		{VK_LBUTTON,"MouseLeft"}, {VK_RBUTTON,"MouseRight"}, {VK_MBUTTON,"MouseMiddle"},
		{VK_XBUTTON1,"MouseX1"},  {VK_XBUTTON2,"MouseX2"},
		{VK_BACK,   "Backspace"}, {VK_TAB,    "Tab"},    {VK_RETURN, "Enter"},
		{VK_ESCAPE, "Esc"},       {VK_SPACE,  "Space"},  {VK_PRIOR,  "PageUp"},
		{VK_NEXT,   "PageDown"},  {VK_END,    "End"},    {VK_HOME,   "Home"},
		{VK_LEFT,   "Left"},      {VK_UP,     "Up"},     {VK_RIGHT,  "Right"},
		{VK_DOWN,   "Down"},      {VK_INSERT, "Insert"}, {VK_DELETE, "Delete"},
		{VK_SNAPSHOT,"PrintScr"}, {VK_PAUSE,  "Pause"},  {VK_CAPITAL,"CapsLock"},
		{VK_NUMLOCK,"NumLock"},   {VK_SCROLL, "ScrollLock"},
		{VK_DIVIDE, "Num/"},      {VK_MULTIPLY,"Num*"},  {VK_SUBTRACT,"Num-"},
		{VK_ADD,    "Num+"},      {VK_DECIMAL,"Num."},    {VK_OEM_PERIOD, "."},
		{VK_OEM_COMMA, ","},      {VK_OEM_MINUS, "-"},    {VK_OEM_PLUS, "="},
		{VK_OEM_1, ";"},         {VK_OEM_2, "/"},        {VK_OEM_3, "`"},
		{VK_OEM_4, "["},         {VK_OEM_5, "\\"},       {VK_OEM_6, "]"},
		{VK_OEM_7, "'"},         {0, NULL}
	};

	for (int i = 0; namedKeys[i].name != NULL; i++) {
		if (vkCode == namedKeys[i].vk) {
			strncpy(outName, namedKeys[i].name, outNameSize - 1);
			outName[outNameSize - 1] = '\0';
			return;
		}
	}

	// Function keys F1-F24
	if (vkCode >= VK_F1 && vkCode <= VK_F24) {
		snprintf(outName, outNameSize, "F%d", vkCode - VK_F1 + 1);
		return;
	}

	// Numpad 0-9
	if (vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9) {
		snprintf(outName, outNameSize, "Num%d", vkCode - VK_NUMPAD0);
		return;
	}

	// Letters A-Z
	if (vkCode >= 'A' && vkCode <= 'Z') {
		snprintf(outName, outNameSize, "%c", (char)vkCode);
		return;
	}

	// Digits 0-9
	if (vkCode >= '0' && vkCode <= '9') {
		snprintf(outName, outNameSize, "%c", (char)vkCode);
		return;
	}

	// Fallback: hex
	snprintf(outName, outNameSize, "0x%02X", vkCode);
}

bool isHotkeyActive(HotkeyState* hs) {
	if (hs == NULL || hs->mode == HotkeyNone || hs->keyCode == 0) {
		return false;
	}

	SHORT state = GetAsyncKeyState(hs->keyCode);
	bool keyDown = (state & 0x8000) != 0;
	if (!keyDown) return false;

	if (hs->modifierMask & 1) { // Ctrl
		if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) return false;
	}
	if (hs->modifierMask & 2) { // Shift
		if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)) return false;
	}
	if (hs->modifierMask & 4) { // Alt required
		if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) return false;
	}

	return true;
}

// --- Global hotkey via background polling thread ---
// Uses GetAsyncKeyState which reads physical key state from the kernel directly.
// Works in fullscreen games when the process has sufficient privilege (run as admin).

// Thread parameter: bundles everything the polling thread needs
typedef struct HotkeyThreadParam {
	HotkeyState* hs;
	RTSSCrossConfig* config;
	const char* osdEntryName;
} HotkeyThreadParam;

// Background thread: polls GetAsyncKeyState and updates RTSS directly
DWORD WINAPI hotkeyPollThread(LPVOID param) {
	HotkeyThreadParam* tp = (HotkeyThreadParam*)param;

	// Create message queue for this thread — required for accurate GetAsyncKeyState
	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	while (tp->hs->threadRunning) {
		// Process any pending messages to keep input state synced
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// --- Master toggle (always active, controls whether main hotkey works) ---
		if (tp->hs->masterKeyCode != 0) {
			SHORT mState = GetAsyncKeyState(tp->hs->masterKeyCode);
			// Toggle keys (Caps/Num/Scroll Lock): use "was pressed" flag (0x0001)
			// Regular keys: use "is currently down" flag (0x8000)
			bool mKeyDown = (tp->hs->masterKeyCode == VK_CAPITAL ||
			                 tp->hs->masterKeyCode == VK_NUMLOCK ||
			                 tp->hs->masterKeyCode == VK_SCROLL)
				? (mState & 0x0001) != 0
				: (mState & 0x8000) != 0;
			// Check master modifiers
			if (mKeyDown) {
				if (tp->hs->masterModifierMask & 1) {
					if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) mKeyDown = false;
				}
				if (tp->hs->masterModifierMask & 2) {
					if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)) mKeyDown = false;
				}
				if (tp->hs->masterModifierMask & 4) {
					if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) mKeyDown = false;
				}
			}
			// Toggle on rising edge
			if (mKeyDown && !tp->hs->masterPrevAsyncState) {
				tp->hs->masterEnabled = !tp->hs->masterEnabled;
			}
			tp->hs->masterPrevAsyncState = mKeyDown;
		}

		// --- Main hotkey (only active when master is enabled) ---
		if (tp->hs->masterEnabled && tp->hs->mode != HotkeyNone && tp->hs->keyCode != 0) {
			SHORT state = GetAsyncKeyState(tp->hs->keyCode);
			// Toggle keys (Caps/Num/Scroll Lock): use "was pressed" flag (0x0001)
			bool keyDown = (tp->hs->keyCode == VK_CAPITAL ||
			                tp->hs->keyCode == VK_NUMLOCK ||
			                tp->hs->keyCode == VK_SCROLL)
				? (state & 0x0001) != 0
				: (state & 0x8000) != 0;

			// Check required modifiers (extra modifiers don't block)
			if (keyDown) {
				if (tp->hs->modifierMask & 1) {
					if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) keyDown = false;
				}
				if (tp->hs->modifierMask & 2) {
					if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)) keyDown = false;
				}
				if (tp->hs->modifierMask & 4) {
					if (!(GetAsyncKeyState(VK_MENU) & 0x8000)) keyDown = false;
				}
			}

			if (tp->hs->mode == HotkeyHold) {
				if (keyDown != tp->hs->visible) {
					tp->hs->visible = keyDown;
					RTSSUpdateOSDVisibility(tp->config, tp->osdEntryName, keyDown);
				}
			} else if (tp->hs->mode == HotkeyToggle) {
				if (keyDown && !tp->hs->prevAsyncState) {
					tp->hs->visible = !tp->hs->visible;
					RTSSUpdateOSDVisibility(tp->config, tp->osdEntryName, tp->hs->visible);
				}
			}
			tp->hs->prevAsyncState = keyDown;
		}

		Sleep(5); // ~200 polls/sec
	}
	return 0;
}

void startHotkeyThread(HotkeyState* hs, RTSSCrossConfig* config, const char* osdEntryName) {
	if (hs == NULL || hs->threadRunning) return;
	hs->threadRunning = true;
	HotkeyThreadParam* tp = (HotkeyThreadParam*)malloc(sizeof(HotkeyThreadParam));
	tp->hs = hs;
	tp->config = config;
	tp->osdEntryName = osdEntryName;
	HANDLE hThread = CreateThread(NULL, 0, hotkeyPollThread, tp, 0, NULL);
	if (hThread != NULL) {
		CloseHandle(hThread);
	}
}

void stopHotkeyThread(HotkeyState* hs) {
	if (hs == NULL) return;
	hs->threadRunning = false;
	Sleep(30);
}

int imin(int i1, int i2) {
	return i1 < i2 ? i1 : i2;
}

CrossActionType runInteractiveProgram(RTSSCrossConfig* config, RenderPassState* state, HotkeyState* hs) {
	// wait and process user input
	char entryName[32] = "RivaCross_Crosshair";
	bool updatePending = false;
	bool exit = false;
	bool lastVisible = true; // track RTSS state for change detection
	CrossActionArgs actionArgs =  {
		CrossActionType::None,
		0, 0, 0, 0, NULL
	};

	actionArgs.hotkeyState = hs;

	// Launch background hotkey hook thread (works even when console is unfocused)
	if (hs != NULL) {
		startHotkeyThread(hs, config, RivaCrossOSDEntryName);
		lastVisible = hs->visible;
	}

	// Use non-blocking input so the main loop can sync hotkey visibility
	timeout(20);

	do {
		// --- Sync hotkey visibility (set by background thread) to RTSS ---
		if (hs != NULL && hs->mode != HotkeyNone && hs->visible != lastVisible) {
			lastVisible = hs->visible;
			RTSSUpdateOSDVisibility(config, RivaCrossOSDEntryName, hs->visible);
		}

		// --- Console TUI input handling ---
		if (pollUserAction(&actionArgs)) {
			// Switch to blocking mode for interactive process functions
			timeout(-1);

			// even if there is log output visible
			// the cursor sits below commands and active config output currently
			// so we can clear to bottom in order to remove log output
			int cursorX, cursorY;
			getsyx(cursorY, cursorX);
			clrtobot();
			// and restore the cursor position
			// to have new log output correctly placed
			setsyx(cursorY, 0);

			if (processUserAction(&actionArgs, config, state)) {
				updatePending = true;
			}
			// update the displayed information
			clrtobot();
			move(0, 0);
			state->linesFilled = 0;
			helpCommands(state);
			addch('\n');
			state->linesFilled += 1;
			helpActiveConfig(config, hs, state);
			// and stop here in order to not override
			// what processUserAction() has printed

			// Return to non-blocking polling mode
			timeout(20);
		}
		if (updatePending) {
			addch(' ');
			state->linesFilled += 1;
			// Respect current visibility when updating
			bool curVisible = (hs != NULL) ? hs->visible : true;
			if (RTSSUpdateOSDVisibility(config, RivaCrossOSDEntryName, curVisible) != 0) {
				printw("Failed to send active config to RTSS\n");
				state->linesFilled += 1;
			}
			updatePending = false;
			// After a manual update, ensure crosshair is visible
			if (hs != NULL) {
				hs->visible = true;
			}
			// Auto-save config so settings persist across restarts
			char configPath[256];
			makeConfigPath(g_activeConfigName, configPath);
			saveConfigToFile(configPath, config, hs);
			lastVisible = (hs != NULL) ? hs->visible : true;
		}
		refresh();
		// keep the original polling delay for stable RTSS interaction
		Sleep(20);
		exit = actionArgs.action == CrossActionType::ExitClean ||
			actionArgs.action == CrossActionType::ExitHard;
	} while (!exit);

	// Stop the background hotkey thread
	if (hs != NULL) {
		stopHotkeyThread(hs);
	}

	return actionArgs.action;
}