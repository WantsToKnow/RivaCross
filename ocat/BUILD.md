# Build the optional OCAT renderer

This builds the patched OCAT D3D11/D3D12 renderer without installing OCAT's full frontend, capture service, or installer. OCAT's Vulkan overlay is a separate DLL and is not built here. The script patches the OCAT checkout; it does not load the DLL into any process.

The repository also includes optional, prebuilt x64 Release files in [`bin/`](bin/): `gameoverlay64.dll` (301,056 bytes) and `DLLInjector64.exe` (117,248 bytes). They were built from the pinned OCAT commit below with [`ocat-crosshair.patch`](ocat-crosshair.patch), [`ocat-no-vr.patch`](ocat-no-vr.patch), [`ocat-late-d3d11.patch`](ocat-late-d3d11.patch), [`ocat-test-corner.patch`](ocat-test-corner.patch), [`ocat-pixel-nudge.patch`](ocat-pixel-nudge.patch), and [`ocat-late-d3d12.patch`](ocat-late-d3d12.patch), using Visual Studio 2022. The DLL omits OCAT's VR paths and Vulkan proxy exports. The patched injector accepts Unicode paths and reports a failed remote load. These files are separate from the normal RivaCross executable; the build script does not install or inject them. `RivaCrossOCAT.exe` can launch a selected x64 executable or attach to a running D3D11/D3D12 process with the bundled injector.

## Requirements

- Windows x64, Git, Visual Studio 2022 Build Tools with C++ tools and a Windows 10/11 SDK.
- OCAT [v1.6.3](https://github.com/GPUOpen-Tools/ocat/releases/tag/v1.6.3), commit `a7652f6ecb313342a7be05b6ba8d030452aa0600`.
- [Vulkan-Headers v1.2.182](https://github.com/KhronosGroup/Vulkan-Headers/releases/tag/v1.2.182), or a full Vulkan SDK with those headers. OCAT's shared headers use Vulkan types, but this D3D build needs neither `vulkan-1.lib` nor a system Vulkan loader.

For a small source checkout, from PowerShell:

```powershell
$ocatSource = Join-Path $env:TEMP 'RivaCross-OCAT-v1.6.3'
$vulkanHeaders = Join-Path $env:TEMP 'RivaCross-Vulkan-Headers-1.2.182'
git clone --depth 1 --branch v1.6.3 --filter=blob:none --sparse https://github.com/GPUOpen-Tools/OCAT.git $ocatSource
git -C $ocatSource sparse-checkout set Commons GameOverlay/d3d DLLInjector MinHook
git clone --depth 1 --branch v1.2.182 --filter=blob:none --sparse https://github.com/KhronosGroup/Vulkan-Headers.git $vulkanHeaders
git -C $vulkanHeaders sparse-checkout set include
powershell -NoProfile -ExecutionPolicy Bypass -File .\ocat\build-ocat.ps1 -OcatSource $ocatSource -VulkanSdk $vulkanHeaders
```

Run the last command from the RivaCross repository root. The script accepts an existing full Vulkan SDK path for `-VulkanSdk` as well. It applies the six patches listed above once each, then builds x64 Release `libMinHook`, `Commons`, `GameOverlay`, and `DLLInjector`. Outputs remain in the OCAT checkout:

- `x64\Release\Bin\gameoverlay64.dll` — patched D3D renderer with late D3D11/D3D12 attachment, without VR paths or Vulkan proxy exports (301,056 bytes in the VS2022 build checked here).
- `x64\Release\Bin\DLLInjector64.exe` — optional launcher helper (about 115 KiB in the checked build).

The rebuilt DLL needs the Microsoft Visual C++ 2015–2022 runtime at execution time. `dumpbin /imports` confirms it has no `vulkan-1.dll` dependency. Vulkan-Headers are only a build input.

For a local, unprotected x64 D3D test process, `DLLInjector64.exe` accepts `-l <absolute DLL path> -p <process ID>`. Exit code 1 reports that remote `LoadLibraryW` returned a nonzero result; 0 reports failure. The experimental late D3D12 path observes `ExecuteCommandLists` to find a direct command queue for the swapchain's device and only draws to swapchains marked for render-target output. A swapchain does not expose its original queue, so rendering stops if two distinct direct queues appear for one device. If ResizeBuffers1 supplies new Present queues, rendering stops for that swapchain until it is released, including after later resizes. The late renderer waits for GPU completion before releasing resources; if completion cannot be confirmed, it retains them and fails a resize with `DXGI_ERROR_INVALID_CALL`. A local D3D12 smoke test displayed both the TEST marker and crosshair after injection. This does not establish compatibility with any particular game, including Apex.

## Source notices

The bundled [`licenses/`](licenses/) folder has notices for OCAT's AMD MIT code (`OCAT-MIT.txt`), crosire gameoverlay (`gameoverlay-LICENSE.txt`), MinHook and HDE (`MinHook-LICENSE.txt`), [JSON for Modern C++](https://github.com/GPUOpen-Tools/ocat/blob/v1.6.3/Commons/Config/json.hpp) (`nlohmann-json-MIT.txt`), Microsoft's [`d3dx12.h`](https://github.com/GPUOpen-Tools/ocat/blob/v1.6.3/GameOverlay/d3d/deps/d3dx12/d3dx12.h) (`d3dx12-MIT.txt`), and [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/blob/v1.2.182/LICENSE.txt) (`Vulkan-Headers-Apache-2.0.txt`). These cover the third-party sources identified in the bundled D3D renderer. See OCAT's [upstream third-party notices](https://github.com/GPUOpen-Tools/ocat/blob/v1.6.3/docs/source/NOTICES.TXT) for additional source provenance.
