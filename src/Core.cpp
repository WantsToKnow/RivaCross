#include "stdafx.h"
#include "RivaCross.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

const char RivaCrossOSDEntryName[32] = "RivaCross_crosshair";

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
// Return 1 for a line, 0 for EOF, -1 for a line that cannot fit the field.
static int readConfigLine(FILE* file, char* line, size_t capacity) {
	size_t length = 0;
	int ch;
	while ((ch = fgetc(file)) != EOF && ch != '\n') {
		if (ch == '\0' || length + 1 >= capacity) return -1;
		line[length++] = (char)ch;
	}
	if (ch == EOF && length == 0) return ferror(file) ? -1 : 0;
	if (length > 0 && line[length - 1] == '\r') --length;
	line[length] = '\0';
	return 1;
}

static bool parseConfigInt(const char* line, int minValue, int maxValue, int* value) {
	char* end;
	errno = 0;
	long parsed = strtol(line, &end, 0);
	if (end == line || errno == ERANGE || parsed < minValue || parsed > maxValue) return false;
	while (isspace((unsigned char)*end)) ++end;
	if (*end != '\0') return false;
	*value = (int)parsed;
	return true;
}

static bool parseConfigScale(const char* line, unsigned int* value) {
	const char* start = line;
	while (isspace((unsigned char)*start)) ++start;
	if (*start == '-') return false;
	char* end;
	errno = 0;
	unsigned long parsed = strtoul(line, &end, 0);
	if (end == line || errno == ERANGE || parsed > INT_MAX) return false;
	while (isspace((unsigned char)*end)) ++end;
	if (*end != '\0') return false;
	*value = (unsigned int)parsed;
	return true;
}

static int hexDigit(char ch) {
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
	return -1;
}

static bool parseConfigColor(char* line, unsigned char color[4]) {
	size_t length = strlen(line);
	while (length > 0 && isspace((unsigned char)line[length - 1])) --length;
	if ((length != 7 && length != 9) || line[0] != '#') return false;
	color[3] = 0xFF;
	for (size_t i = 0; i < (length - 1) / 2; ++i) {
		int high = hexDigit(line[1 + i * 2]);
		int low = hexDigit(line[2 + i * 2]);
		if (high < 0 || low < 0) return false;
		color[i] = (unsigned char)(high * 16 + low);
	}
	return true;
}

int loadConfig(char* configPath, RTSSCrossConfig* config, HotkeyState* hs) {
	if (configPath == NULL || config == NULL) return -1;
	FILE* file = fopen(configPath, "r");
	if (file == NULL) return -2;

	RTSSCrossConfig parsed = {};
	char line[128];
	int hotkey[5] = {};
	int hotkeyLines = 0;
	int result = -3;
	do {
		if (readConfigLine(file, line, sizeof(line)) != 1 ||
			strlen(line) >= sizeof(parsed.crosshair)) break;
		strcpy(parsed.crosshair, line);
		if (readConfigLine(file, line, sizeof(line)) != 1 ||
			!parseConfigInt(line, 0, INT_MAX, &parsed.position[0])) break;
		if (readConfigLine(file, line, sizeof(line)) != 1 ||
			!parseConfigInt(line, 0, INT_MAX, &parsed.position[1])) break;
		if (readConfigLine(file, line, sizeof(line)) != 1 ||
			!parseConfigScale(line, &parsed.scale)) break;
		if (readConfigLine(file, line, sizeof(line)) != 1 ||
			!parseConfigColor(line, parsed.color)) break;

		const int maxValues[5] = { HotkeyToggle, 0xFE, 7, 0xFE, 7 };
		int status = 0;
		while (hotkeyLines < 5 && (status = readConfigLine(file, line, sizeof(line))) == 1) {
			if (!parseConfigInt(line, 0, maxValues[hotkeyLines], &hotkey[hotkeyLines])) {
				status = -1;
				break;
			}
			++hotkeyLines;
		}
		if (status < 0 || hotkeyLines == 1) break;
		if (hotkeyLines == 5 && readConfigLine(file, line, sizeof(line)) != 0) break;
		result = 0;
	} while (false);
	fclose(file);
	if (result != 0) return result;

	*config = parsed;
	if (hs != NULL && hotkeyLines >= 2) {
		hs->mode = (HotkeyMode)hotkey[0];
		hs->keyCode = hotkey[1];
		hs->modifierMask = hotkeyLines >= 3 ? hotkey[2] : 0;
		hs->masterKeyCode = hotkeyLines >= 4 ? hotkey[3] : 0;
		hs->masterModifierMask = hotkeyLines >= 5 ? hotkey[4] : 0;
		hs->visible = true;
		hs->prevAsyncState = false;
	}
	return 0;
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
	bool success = ferror(file) == 0;
	if (fclose(file) != 0) success = false;
	return success;
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

int imin(int i1, int i2) {
	return i1 < i2 ? i1 : i2;
}
