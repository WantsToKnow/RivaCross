// Small, unprotected target for manually checking OCAT's late D3D12 attachment.
// Build from a VS x64 developer prompt: cl /nologo /EHsc tests\manual_d3d12.cpp d3d12.lib dxgi.lib user32.lib
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cwchar>

using Microsoft::WRL::ComPtr;

static DWORD WINAPI deadline(void*) {
	Sleep(7500);
	TerminateProcess(GetCurrentProcess(), 124);
	return 0;
}

static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM key, LPARAM param) {
	if (message == WM_KEYDOWN && key == VK_ESCAPE) { DestroyWindow(window); return 0; }
	if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
	return DefWindowProcW(window, message, key, param);
}

static bool check(HRESULT result, const wchar_t* action) {
	if (SUCCEEDED(result)) return true;
	MessageBoxW(nullptr, action, L"D3D12 smoke test failed", MB_ICONERROR);
	return false;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int) {
	HANDLE guard = CreateThread(nullptr, 0, deadline, nullptr, 0, nullptr);
	if (!guard) return 2;
	CloseHandle(guard);
	const bool twoQueues = arguments && wcsstr(arguments, L"--two-queues");
	const bool resizeRepeatedly = arguments && wcsstr(arguments, L"--resize");
	WNDCLASSW type = {};
	type.hInstance = instance;
	type.lpfnWndProc = windowProc;
	type.lpszClassName = L"RivaCrossD3D12Smoke";
	if (!RegisterClassW(&type)) return 1;
	HWND window = CreateWindowW(type.lpszClassName, L"RivaCross D3D12 smoke test",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 540,
		nullptr, nullptr, instance, nullptr);
	if (!window) return 1;
	ShowWindow(window, SW_SHOW);

	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D12CommandQueue> extraQueue;
	ComPtr<IDXGIFactory4> factory;
	ComPtr<IDXGISwapChain1> swapchain1;
	ComPtr<IDXGISwapChain3> swapchain;
	ComPtr<ID3D12DescriptorHeap> heap;
	ComPtr<ID3D12Resource> buffers[2];
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> commands;
	ComPtr<ID3D12CommandAllocator> extraAllocator;
	ComPtr<ID3D12GraphicsCommandList> extraCommands;
	ComPtr<ID3D12Fence> fence;
	ComPtr<ID3D12Fence> extraFence;
	if (!check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&device)), L"Create device")) return 1;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (!check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)),
		L"Create queue") ||
		!check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), L"Create factory")) return 1;
	if (twoQueues &&
		(!check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&extraQueue)),
			L"Create extra queue") ||
		!check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&extraAllocator)), L"Create extra allocator") ||
		!check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			extraAllocator.Get(), nullptr, IID_PPV_ARGS(&extraCommands)),
			L"Create extra command list") ||
		!check(extraCommands->Close(), L"Close extra command list") ||
		!check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&extraFence)), L"Create extra fence"))) return 1;
	DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
	swapDesc.Width = 960;
	swapDesc.Height = 540;
	swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.BufferCount = 2;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	if (!check(factory->CreateSwapChainForHwnd(queue.Get(), window, &swapDesc,
		nullptr, nullptr, &swapchain1), L"Create swap chain") ||
		!check(swapchain1.As(&swapchain), L"Get swap chain 3")) return 1;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NumDescriptors = 2;
	if (!check(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)),
		L"Create RTV heap")) return 1;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = heap->GetCPUDescriptorHandleForHeapStart();
	UINT stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < 2; ++i) {
		if (!check(swapchain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])),
			L"Get back buffer")) return 1;
		D3D12_CPU_DESCRIPTOR_HANDLE slot = { rtv.ptr + i * stride };
		device->CreateRenderTargetView(buffers[i].Get(), nullptr, slot);
	}
	if (!check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&allocator)), L"Create command allocator") ||
		!check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			allocator.Get(), nullptr, IID_PPV_ARGS(&commands)), L"Create command list") ||
		!check(commands->Close(), L"Close command list") ||
		!check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&fence)), L"Create fence")) return 1;
	HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!event) return 1;

	UINT64 frame = 0;
	MSG message;
	while (true) {
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) { CloseHandle(event); return 0; }
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (resizeRepeatedly && frame > 0 && frame % 180 == 0) {
			buffers[0].Reset();
			buffers[1].Reset();
			if (!check(swapchain->ResizeBuffers(2, 800, 450, DXGI_FORMAT_UNKNOWN, 0),
				L"Resize buffers")) break;
			for (UINT i = 0; i < 2; ++i) {
				if (!check(swapchain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])),
					L"Get resized back buffer")) return 1;
				D3D12_CPU_DESCRIPTOR_HANDLE slot = { rtv.ptr + i * stride };
				device->CreateRenderTargetView(buffers[i].Get(), nullptr, slot);
			}
		}
		UINT index = swapchain->GetCurrentBackBufferIndex();
		if (!check(allocator->Reset(), L"Reset allocator") ||
			!check(commands->Reset(allocator.Get(), nullptr), L"Reset command list")) break;
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = buffers[index].Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		commands->ResourceBarrier(1, &barrier);
		D3D12_CPU_DESCRIPTOR_HANDLE slot = { rtv.ptr + index * stride };
		const FLOAT color[] = { 0.03f, 0.08f, 0.17f, 1.0f };
		commands->ClearRenderTargetView(slot, color, 0, nullptr);
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commands->ResourceBarrier(1, &barrier);
		if (!check(commands->Close(), L"Close command list")) break;
		ID3D12CommandList* list = commands.Get();
		queue->ExecuteCommandLists(1, &list);
		if (twoQueues) {
			ID3D12CommandList* extraList = extraCommands.Get();
			extraQueue->ExecuteCommandLists(1, &extraList);
		}
		if (!check(swapchain->Present(1, 0), L"Present") ||
			!check(queue->Signal(fence.Get(), ++frame), L"Signal fence")) break;
		if (twoQueues && !check(extraQueue->Signal(extraFence.Get(), frame),
			L"Signal extra fence")) break;
		if (fence->GetCompletedValue() < frame) {
			if (!check(fence->SetEventOnCompletion(frame, event), L"Wait for GPU")) break;
			WaitForSingleObject(event, INFINITE);
		}
		if (twoQueues && extraFence->GetCompletedValue() < frame) {
			if (!check(extraFence->SetEventOnCompletion(frame, event),
				L"Wait for extra GPU queue")) break;
			WaitForSingleObject(event, INFINITE);
		}
	}
	CloseHandle(event);
	return 1;
}
