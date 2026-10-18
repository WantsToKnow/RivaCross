// Build in an x64 VS developer prompt (see startup_watch_check.ps1).
// Runs an unprotected local D3D11/12 target, with a 7.5 second hard deadline.
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include "../src/OCATShared.h"

using Microsoft::WRL::ComPtr;

static DWORD WINAPI deadline(void*) {
    Sleep(7500);
    TerminateProcess(GetCurrentProcess(), 124);
    return 0;
}

static void pump() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM key, LPARAM value) {
    return DefWindowProcW(window, message, key, value);
}

static bool callbackAuthorized = false, callbackRejected = false;

static bool statusExists(LONG* stage) {
    DWORD api = 0, error = 0;
    OCATStatus details = {};
    bool found = OCATSharedReadStatus(GetCurrentProcessId(), stage, &api, &error, &details);
    if (found && details.callbackSeen) {
        if (details.authorizationError == ERROR_SUCCESS && !details.startError)
            callbackAuthorized = true;
        if (details.authorizationError == ERROR_NOT_FOUND) callbackRejected = true;
    }
    return found;
}

static int target(const wchar_t* output) {
    const ULONGLONG started = GetTickCount64();
    WNDCLASSW type = {};
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = windowProc;
    type.lpszClassName = L"RivaCrossBoundedD3D11";
    if (!RegisterClassW(&type)) return 10;
    HWND window = CreateWindowW(type.lpszClassName, L"RivaCross local D3D11 check",
        WS_OVERLAPPEDWINDOW, 20, 20, 976, 579, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 11;
    ShowWindow(window, SW_SHOWNOACTIVATE);

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context))) return 12;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return 13;
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = 960;
    desc.Height = 540;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    // Sequential preserves the submitted buffer for readback after Present.
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    ComPtr<IDXGISwapChain1> chain1;
    ComPtr<IDXGISwapChain3> chain;
    if (FAILED(factory->CreateSwapChainForHwnd(device.Get(), window, &desc,
        nullptr, nullptr, &chain1)) || FAILED(chain1.As(&chain))) return 14;
    ComPtr<ID3D11Texture2D> buffers[2], staging;
    ComPtr<ID3D11RenderTargetView> view;
    for (UINT index = 0; index < 2; ++index) {
        if (FAILED(chain->GetBuffer(index, IID_PPV_ARGS(&buffers[index])))) return 15;
    }
    if (FAILED(device->CreateRenderTargetView(buffers[0].Get(), nullptr, &view))) return 15;
    D3D11_TEXTURE2D_DESC texture = {};
    buffers[0]->GetDesc(&texture);
    texture.Usage = D3D11_USAGE_STAGING;
    texture.BindFlags = texture.MiscFlags = 0;
    texture.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(device->CreateTexture2D(&texture, nullptr, &staging))) return 16;

    unsigned before = 0, after = 0, beforeTest = 0, afterTest = 0, lateSamples = 0;
    LONG stage = 0;
    SetPropW(window, L"RivaCrossTestReady", window);
    while (GetTickCount64() - started < 5500) {
        pump();
        const FLOAT clear[] = { 0.03f, 0.08f, 0.17f, 1.0f };
        // D3D11 rotates the identities behind back-buffer zero; unlike D3D12,
        // it does not require selecting GetCurrentBackBufferIndex for drawing.
        context->ClearRenderTargetView(view.Get(), clear);
        if (FAILED(chain->Present(1, 0))) return 17;
        unsigned green = 0, yellow = 0;
        // Read both preserved buffers so their rotation cannot hide the frame
        // that the overlay just submitted. Both are cleared again after stop.
        for (auto& buffer : buffers) {
            context->CopyResource(staging.Get(), buffer.Get());
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return 18;
            for (UINT y = 0; y < texture.Height; ++y) {
                const BYTE* row = static_cast<const BYTE*>(mapped.pData) + y * mapped.RowPitch;
                for (UINT x = 0; x < texture.Width; ++x) {
                    const BYTE* pixel = row + x * 4;
                    if (pixel[1] > 120 && pixel[0] < 80 && pixel[2] < 80) ++green;
                    if (x >= 848 && y < 56 && pixel[0] > 120 &&
                        pixel[1] > 120 && pixel[2] < 80) ++yellow;
                }
            }
            context->Unmap(staging.Get(), 0);
        }
        const ULONGLONG elapsed = GetTickCount64() - started;
        if (elapsed < 2300 && green > before) before = green;
        if (elapsed < 2300 && yellow > beforeTest) beforeTest = yellow;
        if (elapsed > 4300) { after += green; afterTest += yellow; ++lateSamples; }
        LONG observed = 0;
        if (statusExists(&observed) && observed > stage) stage = observed;
    }
    FILE* file = nullptr;
    if (_wfopen_s(&file, output, L"w") || !file) return 19;
    fprintf(file, "%u %u %u %u %ld %u %u %u\n", before, after,
        beforeTest, afterTest, stage, lateSamples, callbackAuthorized, callbackRejected);
    fclose(file);
    DestroyWindow(window);
    return 0;
}

static bool waitGpu(ID3D12CommandQueue* queue, ID3D12Fence* fence,
                    HANDLE event, UINT64 value) {
    if (FAILED(queue->Signal(fence, value))) return false;
    if (fence->GetCompletedValue() >= value) return true;
    return SUCCEEDED(fence->SetEventOnCompletion(value, event)) &&
        WaitForSingleObject(event, 100) == WAIT_OBJECT_0;
}

static int targetD3D12(const wchar_t* output) {
    const ULONGLONG started = GetTickCount64();
    WNDCLASSW type = {};
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpfnWndProc = windowProc;
    type.lpszClassName = L"RivaCrossBoundedD3D12";
    if (!RegisterClassW(&type)) return 30;
    HWND window = CreateWindowW(type.lpszClassName, L"RivaCross local D3D12 check",
        WS_OVERLAPPEDWINDOW, 20, 20, 976, 579, nullptr, nullptr, type.hInstance, nullptr);
    if (!window) return 31;
    ShowWindow(window, SW_SHOWNOACTIVATE);

    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))) ||
        FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return 32;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)))) return 33;
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = 960;
    desc.Height = 540;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    ComPtr<IDXGISwapChain1> chain1;
    ComPtr<IDXGISwapChain3> chain;
    if (FAILED(factory->CreateSwapChainForHwnd(queue.Get(), window, &desc,
        nullptr, nullptr, &chain1)) || FAILED(chain1.As(&chain))) return 34;
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 2;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)))) return 35;
    const UINT stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const D3D12_CPU_DESCRIPTOR_HANDLE first = heap->GetCPUDescriptorHandleForHeapStart();
    ComPtr<ID3D12Resource> buffers[2];
    for (UINT index = 0; index < 2; ++index) {
        if (FAILED(chain->GetBuffer(index, IID_PPV_ARGS(&buffers[index])))) return 36;
        D3D12_CPU_DESCRIPTOR_HANDLE slot = { first.ptr + index * stride };
        device->CreateRenderTargetView(buffers[index].Get(), nullptr, slot);
    }
    const D3D12_RESOURCE_DESC texture = buffers[0]->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT64 bytes = 0;
    device->GetCopyableFootprints(&texture, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    D3D12_HEAP_PROPERTIES readHeap = {};
    readHeap.Type = D3D12_HEAP_TYPE_READBACK;
    readHeap.CreationNodeMask = readHeap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC readDesc = {};
    readDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readDesc.Width = bytes;
    readDesc.Height = readDesc.DepthOrArraySize = readDesc.MipLevels = 1;
    readDesc.SampleDesc.Count = 1;
    readDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if (FAILED(device->CreateCommittedResource(&readHeap, D3D12_HEAP_FLAG_NONE,
        &readDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return 37;
    ComPtr<ID3D12CommandAllocator> clearAllocator, copyAllocator;
    ComPtr<ID3D12GraphicsCommandList> clearCommands, copyCommands;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&clearAllocator))) ||
        FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&copyAllocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            clearAllocator.Get(), nullptr, IID_PPV_ARGS(&clearCommands))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            copyAllocator.Get(), nullptr, IID_PPV_ARGS(&copyCommands))) ||
        FAILED(clearCommands->Close()) || FAILED(copyCommands->Close())) return 38;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return 39;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) return 40;

    UINT64 frame = 0;
    unsigned before = 0, after = 0, beforeTest = 0, afterTest = 0, lateSamples = 0;
    LONG stage = 0;
    SetPropW(window, L"RivaCrossTestReady", window);
    while (GetTickCount64() - started < 5500) {
        pump();
        // D3D12 keeps physical buffer identities stable; use the actual index.
        const UINT index = chain->GetCurrentBackBufferIndex();
        if (FAILED(clearAllocator->Reset()) ||
            FAILED(clearCommands->Reset(clearAllocator.Get(), nullptr))) return 41;
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = buffers[index].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        clearCommands->ResourceBarrier(1, &barrier);
        D3D12_CPU_DESCRIPTOR_HANDLE slot = { first.ptr + index * stride };
        const FLOAT clear[] = { 0.03f, 0.08f, 0.17f, 1.0f };
        clearCommands->ClearRenderTargetView(slot, clear, 0, nullptr);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        clearCommands->ResourceBarrier(1, &barrier);
        if (FAILED(clearCommands->Close())) return 42;
        ID3D12CommandList* clearList = clearCommands.Get();
        queue->ExecuteCommandLists(1, &clearList);
        if (FAILED(chain->Present(1, 0))) return 43;

        // OCAT submits its draw to this queue before original Present. The copy
        // is submitted afterwards, so the readback includes those actual pixels.
        if (FAILED(copyAllocator->Reset()) ||
            FAILED(copyCommands->Reset(copyAllocator.Get(), nullptr))) return 44;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        copyCommands->ResourceBarrier(1, &barrier);
        D3D12_TEXTURE_COPY_LOCATION source = {};
        source.pResource = buffers[index].Get();
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION destination = {};
        destination.pResource = readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        copyCommands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        copyCommands->ResourceBarrier(1, &barrier);
        if (FAILED(copyCommands->Close())) return 45;
        ID3D12CommandList* copyList = copyCommands.Get();
        queue->ExecuteCommandLists(1, &copyList);
        if (!waitGpu(queue.Get(), fence.Get(), event, ++frame)) return 46;
        D3D12_RANGE readRange = { 0, static_cast<SIZE_T>(bytes) };
        BYTE* mapped = nullptr;
        if (FAILED(readback->Map(0, &readRange, reinterpret_cast<void**>(&mapped)))) return 47;
        unsigned green = 0, yellow = 0;
        for (UINT y = 0; y < texture.Height; ++y) {
            const BYTE* row = mapped + footprint.Offset + y * footprint.Footprint.RowPitch;
            for (UINT x = 0; x < texture.Width; ++x) {
                const BYTE* pixel = row + x * 4;
                if (pixel[1] > 120 && pixel[0] < 80 && pixel[2] < 80) ++green;
                if (x >= 848 && y < 56 && pixel[0] > 120 &&
                    pixel[1] > 120 && pixel[2] < 80) ++yellow;
            }
        }
        const D3D12_RANGE noWrite = { 0, 0 };
        readback->Unmap(0, &noWrite);
        const ULONGLONG elapsed = GetTickCount64() - started;
        if (elapsed < 2300 && green > before) before = green;
        if (elapsed < 2300 && yellow > beforeTest) beforeTest = yellow;
        if (elapsed > 4300) { after += green; afterTest += yellow; ++lateSamples; }
        LONG observed = 0;
        if (statusExists(&observed) && observed > stage) stage = observed;
    }
    CloseHandle(event);
    FILE* file = nullptr;
    if (_wfopen_s(&file, output, L"w") || !file) return 48;
    fprintf(file, "%u %u %u %u %ld %u %u %u\n", before, after,
        beforeTest, afterTest, stage, lateSamples, callbackAuthorized, callbackRejected);
    fclose(file);
    DestroyWindow(window);
    return 0;
}

static bool spawn(const wchar_t* executable, const std::wstring& output,
                  PROCESS_INFORMATION* child, bool d3d12) {
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --target \"" + output + L"\"";
    if (d3d12) command += L" --d3d12";
    STARTUPINFOW startup = { sizeof(startup) };
    return CreateProcessW(executable, &command[0], nullptr, nullptr, FALSE, 0,
        nullptr, nullptr, &startup, child) != FALSE;
}

static bool results(const std::wstring& path, bool matched, bool windowMode) {
    FILE* file = nullptr;
    unsigned before = 0, after = 0, beforeTest = 0, afterTest = 0, samples = 0;
    long stage = 0;
    unsigned authorized = 0, rejected = 0;
    if (_wfopen_s(&file, path.c_str(), L"r") || !file) return false;
    const bool read = fscanf_s(file, "%u %u %u %u %ld %u %u %u", &before, &after,
        &beforeTest, &afterTest, &stage, &samples, &authorized, &rejected) == 8;
    fclose(file);
    printf("%s: before_green=%u after_green=%u before_TEST=%u after_TEST=%u stage=%ld late_samples=%u\n",
        matched ? "selected" : "unselected", before, after,
        beforeTest, afterTest, stage, samples);
    printf("callback_authorized=%u callback_rejected=%u\n", authorized, rejected);
    return read && samples > 0 && after == 0 && afterTest == 0 &&
        (matched ? before > 5 && beforeTest > 5 && stage == 3 && authorized == 1 :
            before == 0 && beforeTest == 0 && stage == 0 && authorized == 0 &&
            (windowMode || rejected == 1));
}

struct ReadyWindow { DWORD pid; HWND window; };

static BOOL CALLBACK findReadyWindow(HWND window, LPARAM value) {
    ReadyWindow* selected = reinterpret_cast<ReadyWindow*>(value);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != selected->pid || !GetPropW(window, L"RivaCrossTestReady")) return TRUE;
    selected->window = window;
    return FALSE;
}

static int controller(const wchar_t* dll, bool d3d12, bool windowMode, const wchar_t* entry) {
    const ULONGLONG started = GetTickCount64();
    wchar_t executable[MAX_PATH], temporary[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, executable, MAX_PATH) ||
        !GetTempPathW(MAX_PATH, temporary)) return 20;
    const std::wstring prefix = std::wstring(temporary) + L"RivaCrossWatchCheck-" +
        std::to_wstring(GetCurrentProcessId());
    const std::wstring other = prefix + L"-unselected.exe";
    const std::wstring selectedResult = prefix + L"-selected.txt";
    const std::wstring otherResult = prefix + L"-unselected.txt";
    if (!CopyFileW(executable, other.c_str(), TRUE)) return 21;
    RTSSCrossConfig config = {};
    strcpy_s(config.crosshair, "+");
    config.position[0] = 480;
    config.position[1] = 270;
    config.scale = 50;
    config.color[1] = config.color[3] = 255;
    if (!OCATWatchArm(executable, 0, windowMode ? OCATWatchThread : OCATWatchLaunch) ||
        !OCATSharedUpdate(&config, true)) {
        printf("Cannot own OCAT shared controls; close the real GUI before running this check.\n");
        return 22;
    }
    HMODULE library = LoadLibraryW(dll);
    typedef DWORD(WINAPI* BuildProbe)(LPVOID);
    BuildProbe build = library ? (BuildProbe)GetProcAddress(library, "BuildRivaCrossHookCache") : nullptr;
    if (!OCATProbeOpen() || !build || !build(reinterpret_cast<LPVOID>(
        static_cast<ULONG_PTR>(GetCurrentProcessId()))) || !OCATProbeReady()) {
        printf("External graphics cache creation failed\n"); return 24;
    }
    HMODULE entryLibrary = entry ? LoadLibraryW(entry) : library;
    HOOKPROC callback = entryLibrary ? (HOOKPROC)GetProcAddress(entryLibrary, "GlobalHookProc") : nullptr;
    HHOOK hook = !windowMode && callback ?
        SetWindowsHookExW(WH_CBT, callback, entryLibrary, 0) : nullptr;
    if (!callback || (!windowMode && !hook)) {
        printf("Hook setup failed: %lu\n", GetLastError()); return 23;
    }
    PROCESS_INFORMATION children[2] = {};
    printf("Target API: D3D%u; entry: %s\n", d3d12 ? 12 : 11,
        windowMode ? "running window message" : "startup CBT");
    const bool created = spawn(executable, selectedResult, &children[0], d3d12) &&
        spawn(other.c_str(), otherResult, &children[1], d3d12);
    bool attached = !windowMode;
    if (created && windowMode) {
        ReadyWindow selected = { children[0].dwProcessId, nullptr };
        while (!selected.window && GetTickCount64() - started < 1500) {
            EnumWindows(findReadyWindow, reinterpret_cast<LPARAM>(&selected));
            pump();
            OCATWatchHeartbeat();
            Sleep(5);
        }
        if (selected.window && OCATWatchArm(executable, selected.pid, OCATWatchThread)) {
            hook = SetWindowsHookExW(WH_CALLWNDPROC, callback, entryLibrary,
                GetWindowThreadProcessId(selected.window, nullptr));
            DWORD_PTR ignored = 0;
            attached = hook && SendMessageTimeoutW(selected.window, WM_NULL, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &ignored);
        }
        if (!attached) printf("Window hook/wake failed: %lu\n", GetLastError());
    }
    bool stopped = false, ended = false;
    while (created && attached && GetTickCount64() - started < 6800) {
        pump();
        if (!stopped && GetTickCount64() - started >= 3000) {
            OCATWatchStop();
            stopped = true;
        }
        if (!stopped) OCATWatchHeartbeat();
        ended = WaitForSingleObject(children[0].hProcess, 0) == WAIT_OBJECT_0 &&
            WaitForSingleObject(children[1].hProcess, 0) == WAIT_OBJECT_0;
        if (ended) break;
        Sleep(5);
    }
    OCATWatchStop();
    if (hook) UnhookWindowsHookEx(hook);
    bool exited = created && attached && ended;
    for (auto& child : children) {
        if (child.hProcess) {
            DWORD exitCode = STILL_ACTIVE;
            GetExitCodeProcess(child.hProcess, &exitCode);
            if (exitCode == STILL_ACTIVE) TerminateProcess(child.hProcess, 124);
            if (exitCode != 0) {
                printf("Target PID %lu failed with exit code %lu\n", child.dwProcessId, exitCode);
                exited = false;
            }
            CloseHandle(child.hProcess);
        }
        if (child.hThread) CloseHandle(child.hThread);
    }
    const bool selected = exited && results(selectedResult, true, windowMode);
    const bool unselected = exited && results(otherResult, false, windowMode);
    OCATSharedClose();
    if (entry && entryLibrary) FreeLibrary(entryLibrary);
    if (library) FreeLibrary(library);
    DeleteFileW(other.c_str());
    printf("%s elapsed_ms=%llu\n", selected && unselected ? "PASS" : "FAIL",
        GetTickCount64() - started);
    return selected && unselected ? 0 : 1;
}

int wmain(int count, wchar_t** arguments) {
    HANDLE guard = CreateThread(nullptr, 0, deadline, nullptr, 0, nullptr);
    if (!guard) return 2;
    CloseHandle(guard);
    if (count == 4 && wcscmp(arguments[1], L"--target") == 0 &&
        wcscmp(arguments[3], L"--d3d12") == 0) return targetD3D12(arguments[2]);
    if (count == 3 && wcscmp(arguments[1], L"--target") == 0)
        return target(arguments[2]);
    if (count < 2) {
        printf("Usage: startup_watch_check.exe <absolute DLL path> [--d3d12] [--window] [--entry <DLL path>]\n");
        return 2;
    }
    bool d3d12 = false, windowMode = false;
    const wchar_t* entry = nullptr;
    for (int i = 2; i < count; ++i) {
        if (wcscmp(arguments[i], L"--d3d12") == 0) d3d12 = true;
        else if (wcscmp(arguments[i], L"--window") == 0) windowMode = true;
        else if (wcscmp(arguments[i], L"--entry") == 0 && i + 1 < count) entry = arguments[++i];
        else return 2;
    }
    return controller(arguments[1], d3d12, windowMode, entry);
}
