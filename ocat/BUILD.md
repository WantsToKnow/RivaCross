# Build the optional OCAT renderer

This builds the patched OCAT D3D11/D3D12 renderer without installing OCAT's full frontend, capture service, or installer. OCAT's Vulkan overlay is a separate DLL and is not built here. The script patches the OCAT checkout; it does not load the DLL into any process. The renderer does not automatically inject into child processes. The GUI authorizes either one selected executable for startup watching, or one PID for manual attachment.

The repository also includes optional, prebuilt x64 Release files in [`bin/`](bin/): `gameoverlay64-flip.dll`, `DLLInjector64.exe`, and `RivaCrossOCATHook64.dll`. The first two are built from the pinned OCAT commit below with the ordered patches listed in [`build-ocat.ps1`](build-ocat.ps1), using Visual Studio 2022. The lightweight hook entry is built from this repository's `src/OCATHook.cpp` by `build.bat ocat`. The renderer omits OCAT's VR paths and Vulkan proxy exports. These files are separate from the normal RivaCross executable; the build script does not install or inject them.

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

After `ocat-status.patch`, the remaining patches add startup watching, explicit remote renderer start, factory hooks, and [`ocat-external-probe.patch`](ocat-external-probe.patch). That patch moves graphics address discovery into the GUI's short-lived probe process. The final [`ocat-entry-status.patch`](ocat-entry-status.patch) adds V2 entry/error receipts and preserves the receipt already created by the small entry DLL. All draws require live GUI authorization. The watch, probe, and status structures in this repository and the patched OCAT sources must match.

Run the last command from the RivaCross repository root. The script accepts an existing full Vulkan SDK path for `-VulkanSdk` as well. It applies the eighteen patches in order, then builds x64 Release `libMinHook`, `Commons`, `GameOverlay`, and `DLLInjector`. Outputs remain in the OCAT checkout:

- `x64\Release\Bin\gameoverlay64.dll` — patched D3D renderer with startup watching and late D3D11/D3D12 attachment, without VR paths or Vulkan proxy exports (291,328 bytes in the VS2022 build checked here).
- `x64\Release\Bin\DLLInjector64.exe` — optional launcher helper (about 115 KiB in the checked build).

For the bundled GUI, copy the rebuilt `gameoverlay64.dll` as `ocat\bin\gameoverlay64-flip.dll`. The old `ocat\bin\gameoverlay64.dll` is no longer shipped; a local copy remains loaded by unrelated desktop apps and can be removed after those apps exit. `RivaCrossOCAT.exe` uses only the `-flip` copy.

The rebuilt DLL needs the Microsoft Visual C++ 2015–2022 runtime at execution time. `dumpbin /imports` confirms it has no `vulkan-1.dll` dependency. Vulkan-Headers are only a build input. Both startup watching and running-window attachment use an external graphics cache. The GUI launches itself with an internal probe argument, permits at most 7 seconds, and retains a per-controller read-only cache for target processes. The target verifies each recorded module's full path, PE timestamp, image size, RVA range, and executable memory. It skips unavailable or mismatched addresses and never falls back to creating a temporary D3D device inside the game. Factory creation hooks also remain active for future swapchains.

For a local, unprotected x64 D3D test process, `DLLInjector64.exe` accepts `-l <absolute DLL path> -p <process ID>`. Exit code 1 reports a successful load or renderer-start request, but does not verify drawing; 2 reports Win32 access denied during injection; 0 reports another failure. The experimental late D3D12 path observes `ExecuteCommandLists` to find a direct command queue for the swapchain's device and only draws to flip-model swapchains marked for render-target output. A swapchain does not expose its original queue, so rendering stops if two distinct direct queues appear for one device. If ResizeBuffers1 supplies new Present queues, rendering stops for that swapchain until it is released, including after later resizes. The late renderer waits at most 5 ms on Present for its prior GPU work and skips the overlay frame when completion is unverified. During resize it waits for GPU completion before releasing resources; if completion cannot be confirmed, it retains them and fails the resize with `DXGI_ERROR_INVALID_CALL`. An earlier D3D12 smoke test displayed both the diagnostic TEST marker and crosshair after injection. The current build retains the small top-right TEST marker; a manual D3D12 smoke test showed the crosshair and reported stage 3 (API 12). A separate late-attach GPU readback found 0 yellow TEST pixels before injection and 185 by frame 150; earlier post-injection samples at frames 90 and 120 were still 0, so drawing was not immediate. In another borderless D3D12 late-attach test with RTSS exited, PresentMon recorded 271/274 baseline frames and 343/346 injected frames as `Hardware Composed: Independent Flip`; the other three frames in each run were startup `Composed: Flip`, before attachment. The injected run remained in Independent Flip during and after attachment. These are local tests, not a guarantee for another game or machine; confirm the actual per-frame present mode with [PresentMon](https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md).

The entry and renderer publish a per-PID `Local\RivaCrossOCATStatusV2-<pid>` mapping for the GUI.
Stage 0 can include a confirmed callback with a control-read or renderer-start error.
Stage 1 means the rendering worker was requested, stage 2 means a DXGI Present path was reached,
and stage 3 means D3D11/D3D12 draw commands were issued. A stage 3 receipt
does not prove that the crosshair pixels reached the screen.

The GUI also offers a target-thread `WH_CALLWNDPROC` attach path using the small entry's
`GlobalHookProc` export. It sends `WM_NULL` with a 500 ms timeout to invoke the
callback on an already-running window, without process-memory writes. The
callback signature is shared with the startup hook; it does not inspect or
change the message. **Watch launch...** separately installs a global CBT hook,
authorizing only the selected full EXE path. Neither receipt is a
protected-game compatibility claim. All rendering now requires GUI authorization;
loading the DLL with the standalone injector alone does not enable it.

## Bounded local checks

From an x64 developer prompt at the repository root, build the check:

```bat
cl /nologo /EHsc /std:c++14 /utf-8 tests\startup_watch_check.cpp src\OCATShared.cpp /Fe:startup_watch_check.exe d3d11.lib d3d12.lib dxgi.lib user32.lib
```

Close RivaCrossOCAT, then run either local check:

```powershell
.\tests\startup_watch_check.ps1 -CheckExe .\startup_watch_check.exe -OverlayDll .\ocat\bin\gameoverlay64-flip.dll
.\tests\startup_watch_check.ps1 -CheckExe .\startup_watch_check.exe -OverlayDll .\ocat\bin\gameoverlay64-flip.dll -D3D12
```

Add `-WindowHook` to either command to create the target's swapchain first,
then attach through the window-message hook. With the external cache, this path passed on
2026-10-17 in 6.01 seconds on D3D11 (260 green, 478 TEST pixels) and 6.04 seconds
on D3D12 (130 green, 239 TEST pixels), with zero pixels after revocation.

The selected target must report stage 3, green crosshair pixels, and yellow TEST
pixels; the renamed, unselected target must report no renderer and neither pixel
group. After revocation, both pixel groups must be absent. Each executable has
a 7.5-second watchdog; the runner fails and terminates the controller after
8 seconds. The checks do not launch games or change RTSS. On 2026-10-17, the
new cached startup path passed in 6.03 seconds on D3D12 (130 green, 239 TEST pixels).
The earlier factory-only path passed on both APIs. These were ordinary
local targets; Apex, Delta Force, and Independent Flip remain unverified with
this build.

The 2026-10-17 Apex attempt did not confirm TEST. A direct launch's initial PID
93364 ran OCAT code through the start of module registration, then exited during
the platform launch chain. The later game PIDs 91296 and 96816 produced no OCAT
initialization log. The controller ran at high integrity (RID 12288). A Steam
launch still showed the EA Javelin bootstrap screen at the 8-second cutoff;
the actual game process appeared later. The first cleanup missed a delayed PID,
which was subsequently terminated. The user subsequently allowed launcher
waiting, followed by inspection and closure within 5 seconds of the actual
game window appearing. A separate process guard now closes the game about
4.2 seconds after window detection, independently of screenshot analysis.

At 19:40, Apex PID 96672 displayed its actual loading screen without TEST and
had no per-PID renderer status mapping. The next GUI build added one automatic
`WH_CALLWNDPROC` hook and a bounded `WM_NULL` wake when a watched window has no
DLL acknowledgement. At 19:42, PID 87460 displayed the game intro without TEST;
the GUI reported that the wake message was delivered, but that PID still had
no status mapping or OCAT initialization log. The process guard closed it at
4,252 ms after window detection. These results locate the unresolved failure
before confirmed DLL execution; they do not by themselves establish why the
callback did not acknowledge. No RTSS process was running during these checks.
The local D3D11/D3D12 pixel results above are not an Apex compatibility result.

## Split entry results (2026-10-17)

`RivaCrossOCATHook64.dll` is 105,472 bytes, imports only KERNEL32/USER32, and
has a static runtime. Its callback validates the selected executable and GUI
heartbeat before a worker loads the renderer from the same directory. It
records callback, authorization, and load/start errors before graphics setup.
The renderer preserves that existing V2 receipt. No separate overlay window is
created by this change; Independent Flip still requires measurement in the
actual target.

Use `-EntryDll .\ocat\bin\RivaCrossOCATHook64.dll` with the local check runner.
Startup D3D12 passed in 6,056 ms and running-window D3D12 passed in 6,036 ms.
Both found 130 green and 239 yellow TEST pixels in the selected target, zero
pixels after revocation, and no rendering in the unselected process. Callback
receipts also distinguished authorization from rejection. Each executable
retains the 7.5-second internal deadline and 8-second outer limit.

Apex PIDs 87796 and 95652 did not show TEST with the split entry and closed
at 4,124 ms and 4,132 ms after window detection. PID 95652 and the controller
both had integrity RID 12288; its ExtensionPoint, Signature, and DynamicCode
policy flags each read 0. These results do not identify a blocking component.

A final read-only `Microsoft-Windows-Kernel-Process` trace captured 714 image-load
events, including 135 for Apex PID 91372. That PID loaded D3D12, DXGI, and Steam's
`GameOverlayRenderer64.dll`, but had no load event for either RivaCross DLL and
no V2 receipt. The small hook did produce load events in three other PIDs.
The game was closed at 4,169 ms after window detection. The owned trace session
was stopped after the first stop request failed; its subsequent query confirmed
the session no longer existed. The ETL is retained locally at
`%TEMP%\RivaCross-apex-images-20261017-l.etl`. This is an unresolved game-loading
compatibility issue; copying the public hook sequence has not yielded TEST in Apex.

## Source notices

The bundled [`licenses/`](licenses/) folder has notices for OCAT's AMD MIT code (`OCAT-MIT.txt`), crosire gameoverlay (`gameoverlay-LICENSE.txt`), MinHook and HDE (`MinHook-LICENSE.txt`), [JSON for Modern C++](https://github.com/GPUOpen-Tools/ocat/blob/v1.6.3/Commons/Config/json.hpp) (`nlohmann-json-MIT.txt`), Microsoft's [`d3dx12.h`](https://github.com/GPUOpen-Tools/ocat/blob/v1.6.3/GameOverlay/d3d/deps/d3dx12/d3dx12.h) (`d3dx12-MIT.txt`), and [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers/blob/v1.2.182/LICENSE.txt) (`Vulkan-Headers-Apache-2.0.txt`). These cover the third-party sources identified in the bundled D3D renderer. See OCAT's [upstream third-party notices](https://github.com/GPUOpen-Tools/ocat/blob/v1.6.3/docs/source/NOTICES.TXT) for additional source provenance.
