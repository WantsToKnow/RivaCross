/**
 * Source: RivaTuner Statics Server SDK Samples
 * Original author: Unwinder
 * All rights belong to Unwinder
 */

#include "RTSShared.h"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <limits.h>
#include <vector>

static HRGN drawingRegion(const CrosshairDrawing& drawing, int padding) {
	const int grid = crosshairGridScale(drawing);
	const int width = drawing.width == CrosshairHalfWidth ? 1 : drawing.width;
	const int length = drawing.length * grid;
	const int gap = drawing.gap * grid;
	const int low = -width / 2;
	const int high = low + width;
	if (drawing.style == CrosshairDot) {
		const int start = low - padding;
		const int end = high + padding;
		if (end - start <= 2) return CreateRectRgn(start, start, end, end);
		// GDI ellipse regions omit the far row and column.
		return CreateEllipticRgn(start, start, end + 1, end + 1);
	}

	HRGN region = CreateRectRgn(0, 0, 0, 0);
	HRGN arm = CreateRectRgn(0, 0, 0, 0);
	if (!region || !arm) {
		if (arm) DeleteObject(arm);
		if (region) DeleteObject(region);
		return NULL;
	}
	const auto addRect = [&](int left, int top, int right, int bottom) {
		return SetRectRgn(arm, left - padding, top - padding, right + padding, bottom + padding) &&
			CombineRgn(region, region, arm, RGN_OR) != ERROR;
	};

	const bool joined = gap == 0;
	bool ok = true;
	if (drawing.style == CrosshairChevron) {
		// Each arm steps down one unit per column, with an optional shared apex.
		const int height = length + width - (joined ? 0 : 1);
		const int top = -height / 2;
		const int firstX = joined ? 1 : gap;
		const int firstY = joined ? top + 1 : top;
		if (joined) ok = addRect(0, top, 1, top + width);
		for (int step = 0; step < length && ok; ++step) {
			const int x = firstX + step;
			const int y = firstY + step;
			ok = addRect(-x, y, 1 - x, y + width) &&
				addRect(x, y, x + 1, y + width);
		}
	} else {
		const int left = low - gap;
		const int right = high + gap;
		ok = addRect(left - length, low, left, high) &&
			addRect(right, low, right + length, high) &&
			addRect(low, right, high, right + length);
		if (ok && drawing.style == CrosshairCross)
			ok = addRect(low, left - length, high, left);
		if (ok && joined) ok = addRect(low, low, high, high);
	}
	DeleteObject(arm);
	if (ok) return region;
	DeleteObject(region);
	return NULL;
}

bool makeCrosshairRegions(const CrosshairDrawing& drawing, HRGN& fill, HRGN& outline) {
	outline = NULL;
	fill = drawingRegion(drawing, 0);
	if (!fill) return false;
	if (!drawing.outline) return true;
	outline = drawingRegion(drawing, 1);
	// Keep the rim and fill disjoint so translucent colors blend only once.
	if (outline && CombineRgn(outline, outline, fill, RGN_DIFF) != ERROR) return true;
	if (outline) DeleteObject(outline);
	DeleteObject(fill);
	fill = outline = NULL;
	return false;
}

static const RGNDATA* readRegionData(HRGN region, std::vector<BYTE>& bytes) {
	const DWORD size = GetRegionData(region, 0, NULL);
	if (!size) return NULL;
	bytes.resize(size);
	auto* data = reinterpret_cast<RGNDATA*>(bytes.data());
	return GetRegionData(region, size, data) == size ? data : NULL;
}

static bool appendBars(HRGN region, DWORD color, long long x, long long y,
	char* text, size_t capacity, size_t& used) {
	std::vector<BYTE> bytes;
	const RGNDATA* data = readRegionData(region, bytes);
	if (!data) return false;
	const auto* bars = reinterpret_cast<const RECT*>(data->Buffer);
	for (DWORD i = 0; i < data->rdh.nCount; ++i) {
		// Negative <P> coordinates wrap to the opposite edge in RTSS.
		const long long left = (x + bars[i].left < 0) ? 0 : x + bars[i].left;
		const long long top = (y + bars[i].top < 0) ? 0 : y + bars[i].top;
		const long long width = x + bars[i].right - left;
		const long long height = y + bars[i].bottom - top;
		if (width <= 0 || height <= 0 || left > INT_MAX || top > INT_MAX) continue;
		int written = snprintf(text + used, capacity - used,
			"<P=%lld,%lld><C=%08lx><B=%lld,%lld>\r", left, top, color, width, height);
		if (written < 0 || (size_t)written >= capacity - used) return false;
		used += written;
	}
	return true;
}

static bool rasterizeRegion(HRGN region, const RECT& bounds, DWORD color, std::vector<DWORD>& pixels) {
	std::vector<BYTE> bytes;
	const RGNDATA* data = readRegionData(region, bytes);
	if (!data) return false;
	const auto* rects = reinterpret_cast<const RECT*>(data->Buffer);
	const int stride = bounds.right - bounds.left;
	for (DWORD i = 0; i < data->rdh.nCount; ++i) {
		const RECT& rect = rects[i];
		for (LONG y = rect.top; y < rect.bottom; ++y) {
			auto row = pixels.begin() + (y - bounds.top) * stride + (rect.left - bounds.left);
			std::fill_n(row, rect.right - rect.left, color);
		}
	}
	return true;
}

static bool writeShapePng(const char* file, int width, int height, std::vector<DWORD>& pixels) {
	wchar_t path[MAX_PATH];
	if (!MultiByteToWideChar(CP_ACP, 0, file, -1, path, MAX_PATH)) return false;
	Gdiplus::GdiplusStartupInput input;
	ULONG_PTR token;
	if (Gdiplus::GdiplusStartup(&token, &input, NULL) != Gdiplus::Ok) return false;
	// Built-in Windows PNG encoder.
	const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
	bool ok;
	{
		Gdiplus::Bitmap bitmap(width, height, width * 4, PixelFormat32bppARGB,
			reinterpret_cast<BYTE*>(pixels.data()));
		ok = bitmap.Save(path, &png, NULL) == Gdiplus::Ok;
	}
	Gdiplus::GdiplusShutdown(token);
	return ok;
}

static bool getThinTexture(const CrosshairDrawing& drawing, HRGN fill, HRGN outline,
	const RECT& bounds, char (&path)[MAX_PATH]) {
	char temp[MAX_PATH], directory[MAX_PATH];
	const DWORD tempLength = GetTempPathA(MAX_PATH, temp);
	if (!tempLength || tempLength >= MAX_PATH) return false;
	const int directoryLength = snprintf(directory, sizeof(directory), "%sRivaCross", temp);
	if (directoryLength < 0 || directoryLength >= MAX_PATH) return false;
	if (!CreateDirectoryA(directory, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
	const int length = snprintf(path, sizeof(path), "%s\\thin1-%d-%d-%d-%d.png",
		directory, drawing.style, drawing.length, drawing.gap, drawing.outline ? 1 : 0);
	if (length < 0 || length >= MAX_PATH) return false;
	// RTSS can still load the cached mask after RivaCross exits with the overlay left on.
	if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return true;

	const int width = bounds.right - bounds.left;
	const int height = bounds.bottom - bounds.top;
	std::vector<DWORD> pixels(width * height);
	if ((outline && !rasterizeRegion(outline, bounds, 0xff000000, pixels)) ||
		!rasterizeRegion(fill, bounds, 0xffffffff, pixels)) return false;

	char staged[MAX_PATH];
	if (!GetTempFileNameA(directory, "rcs", 0, staged)) return false;
	if (writeShapePng(staged, width, height, pixels) && MoveFileA(staged, path)) return true;
	DeleteFileA(staged);
	// Another publisher may have created the same mask while we were encoding it.
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static bool formatThinDrawing(const CrosshairDrawing& drawing, HRGN fill, HRGN outline,
	DWORD color, long long x, long long y, char* text, size_t capacity) {
	RECT bounds;
	if (GetRgnBox(outline ? outline : fill, &bounds) <= NULLREGION) return false;
	// Each OSD unit covers two source texels. Round the image bounds to whole units.
	bounds.left &= ~1;
	bounds.top &= ~1;
	bounds.right = (bounds.right + 1) & ~1;
	bounds.bottom = (bounds.bottom + 1) & ~1;
	const int width = (bounds.right - bounds.left) / 2;
	const int height = (bounds.bottom - bounds.top) / 2;
	const long long left = x + bounds.left / 2;
	const long long top = y + bounds.top / 2;
	if (left + width <= 0 || top + height <= 0 || left > INT_MAX || top > INT_MAX) return true;
	const int skipX = left < 0 ? (int)-left : 0;
	const int skipY = top < 0 ? (int)-top : 0;
	const int shownWidth = width - skipX;
	const int shownHeight = height - skipY;
	char path[MAX_PATH];
	if (!getThinTexture(drawing, fill, outline, bounds, path)) return false;
	// RTSS tints white pixels with the current RGBA color and leaves the black rim black.
	const int written = snprintf(text, capacity, "<LI=\"%s\"><P=%lld,%lld><C=%08lx><I=%d,%d,%d,%d,%d,%d>",
		path, left + skipX, top + skipY, color, shownWidth, shownHeight,
		skipX * 2, skipY * 2, shownWidth * 2, shownHeight * 2);
	return written >= 0 && (size_t)written < capacity;
}

static bool formatDrawing(const RTSSCrossConfig* config, long long x, long long y,
	char* text, size_t capacity) {
	const DWORD alpha = (DWORD)config->color[3] << 24;
	if (!alpha) return true;
	const DWORD color = alpha | ((DWORD)config->color[0] << 16) |
		((DWORD)config->color[1] << 8) | config->color[2];
	HRGN fill, outline;
	if (!makeCrosshairRegions(config->drawing, fill, outline)) return false;
	size_t used = 0;
	bool ok;
	if (config->drawing.width == CrosshairHalfWidth)
		ok = formatThinDrawing(config->drawing, fill, outline, color, x, y, text, capacity);
	else
		ok = (!outline || appendBars(outline, alpha, x, y, text, capacity, used)) &&
			appendBars(fill, color, x, y, text, capacity, used);
	if (outline) DeleteObject(outline);
	DeleteObject(fill);
	return ok;
}

int RTSSUpdateOSDVisibility(const RTSSCrossConfig* config, const char* osdEntry, bool visible) {
	using OsdSlot = RTSSharedMemory::RTSSharedMemoryOSDEntry;
	// initially not updated successfully 
	int result = 1;
	char targetText[sizeof(OsdSlot::szOSDEx2)] = " ";
	const bool drawing = visible && config->drawing.style != CrosshairText;
	const long long x = (long long)config->position[0] + config->pixelOffset[0];
	const long long y = (long long)config->position[1] + config->pixelOffset[1];
	if (drawing) {
		if (!formatDrawing(config, x, y, targetText, sizeof(targetText))) return RTSSDrawingFailed;
	} else {
		snprintf(targetText, sizeof(targetText), "<P=%lld,%lld><S=%u><C0=%02x%02x%02x%02x><C0>%s",
			x, y, config->scale, config->color[3], config->color[0], config->color[1],
			config->color[2], visible ? config->crosshair : " ");
	}
	
	// write config to shared RTSS memory
	HANDLE sharedFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "RTSSSharedMemoryV2");
	if (sharedFile == NULL) {
		// file not available
		result = -1;
	} else {
		LPVOID sharedMap = MapViewOfFile(sharedFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		RTSSharedMemory* sharedMem = (RTSSharedMemory*)sharedMap;
		
		if (sharedMem == NULL) {
			// mapping not successful
			result = -2;
		} else {
			const bool longSlot = sharedMem->dwVersion >= 0x00020014 &&
				sharedMem->dwOSDEntrySize >= sizeof(OsdSlot);
			if (sharedMem->dwSignature == 'RTSS' && drawing && !longSlot) {
				result = RTSSDrawingUnsupported;
			} else if ((sharedMem->dwSignature == 'RTSS') &&
				(sharedMem->dwVersion >= 0x00020000)) {
				bool locked = sharedMem->dwVersion < 0x0002000e;
				if (!locked) {
					for (int attempt = 0; attempt < 5; ++attempt) {
						if (!_interlockedbittestandset(&sharedMem->dwBusy, 0)) {
							locked = true;
							break;
						}
						Sleep(1);
					}
				}
				if (!locked) result = -4;
				bool found = false;
				// write in two passes
				//1st pass : find previously captured OSD slot
				//2nd pass : otherwise find the first unused OSD slot and capture it
				for (int pass = 0; locked && pass < 2; pass++) {
					for (int entry = 1; entry < sharedMem->dwOSDArrSize; entry++) {
						//allow primary OSD clients (i.e. EVGA Precision / MSI Afterburner) to use the first slot exclusively, so third party
						//applications start scanning the slots from the second one
						RTSSharedMemory::RTSSharedMemoryOSDEntry* pEntry = 
							(RTSSharedMemory::RTSSharedMemoryOSDEntry*)
							((LPBYTE)sharedMem + sharedMem->dwOSDArrOffset + entry * sharedMem->dwOSDEntrySize);

						if (pass > 0) {
							if (lstrlenA(pEntry->szOSDOwner) == 0) {
								lstrcpyA(pEntry->szOSDOwner, osdEntry);
							}
						}
						if (lstrcmpA(pEntry->szOSDOwner, osdEntry) == 0) {
							if (longSlot) {
								// Diagonal arms can exceed the older 4 KB slot.
								lstrcpynA(pEntry->szOSDEx2, targetText, sizeof(pEntry->szOSDEx2));
								pEntry->szOSDEx[0] = '\0';
							} else if (sharedMem->dwVersion >= 0x00020007) {
								//use extended text slot for v2.7 and higher shared memory, it allows displaying 4096 symbols
								//instead of 256 for regular text slot
								lstrcpynA(pEntry->szOSDEx, targetText, sizeof(pEntry->szOSDEx));
							} else {
								lstrcpynA(pEntry->szOSD, targetText, sizeof(pEntry->szOSD) - 1);
							}
							sharedMem->dwOSDFrame++;
							found = true;
							result = 0;
							break;
						}
					}
					if (found) {
						break;
					}
				}
				if (locked && sharedMem->dwVersion >= 0x0002000e)
					_interlockedbittestandreset(&sharedMem->dwBusy, 0);
			} else {
				// not the correct signature
				result = -3;
			}
			UnmapViewOfFile(sharedMap);
		}
		CloseHandle(sharedFile);
	}
	return result;
}

int RTSSUpdateOSDEntry(const RTSSCrossConfig* config, const char* osdEntry) {
	return RTSSUpdateOSDVisibility(config, osdEntry, true);
}

int RTSSReleaseOSDEntry(const char* osdEntry) {
	// initially not released successfully
	int result = 1;
	// reset shared RTSS memory
	HANDLE sharedFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "RTSSSharedMemoryV2");
	if (sharedFile == NULL) {
		// file not available
		result = -1;
	} else {
		LPVOID sharedMap = MapViewOfFile(sharedFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		RTSSharedMemory* sharedMem= (RTSSharedMemory*)sharedMap;
		if (sharedMem == NULL) {
			// mapping failed
			result = -2;
		} else {
			if ((sharedMem->dwSignature == 'RTSS') &&
				(sharedMem->dwVersion >= 0x00020000)) {
				for (int dwEntry = 1; dwEntry < sharedMem->dwOSDArrSize; dwEntry++) {
					RTSSharedMemory::RTSSharedMemoryOSDEntry* entry = 
						(RTSSharedMemory::RTSSharedMemoryOSDEntry*)
						((LPBYTE)sharedMem + sharedMem->dwOSDArrOffset + dwEntry * sharedMem->dwOSDEntrySize);
					// zero out claimed entry
					if (lstrcmpA(entry->szOSDOwner, osdEntry) == 0) {
						SecureZeroMemory(entry, sharedMem->dwOSDEntrySize);
						sharedMem->dwOSDFrame++;
						result = 0;
					}
				}
			} else {
				// wrong signature
				result = -3;
			}
			UnmapViewOfFile(sharedMem);
		}
		CloseHandle(sharedFile);
	}
	return result;
}
