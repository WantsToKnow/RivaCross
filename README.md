RivaCross - Customizable Crosshair Overlay
==========================================

Adds a customizable crosshair to MSI Afterburner /
RivaTuner Statistics Server

The default build uses a feature in RivaTuner Statistics Server (RTSS)
that enables the addition of custom text to the RTSS on screen display (OSD).<br>
The Windows GUI lets you customize the crosshair without opening a console.

Examples
--------
These examples show different crosshair designs made with RivaCross.<br>
Levels of usability may vary.

[RivaCross in Games #1](docs/images/example00_clear_sky.jpg)

[RivaCross in Games #2](docs/images/example01_mankind_divided.jpg)

[RivaCross in Games #3](docs/images/example02_acc.jpg)

Releases
--------

Older console builds are available in the
[Releases section](https://github.com/irseny/RivaCross/releases).
This checkout includes the Windows GUI executable.

Usage
-----

1. Open MSI Afterburner / RivaTuner Statistics Server
1. Execute `RivaCross.exe`
1. Edit the crosshair text, X/Y position, size, and RGBA color
1. Click **Apply + Save** to update RTSS and save the active preset

Use **Load** to switch presets and **Save As** to create one. The default preset
is `config/default.txt` beside the executable. The window also provides
**Show/Hide**, a clipboard clear action, and an option to leave the crosshair
visible after exit. Configure a main hotkey and an optional **Hotkey 2 (OR)**
in the same window. Both use the selected hold or toggle mode: hold either key
to show the crosshair, or press either key to toggle it. **Master toggle** is
the crosshair's main switch: OFF forces it hidden; ON resets it to visible
in toggle mode, even if a hotkey hid it before. Hold mode always requires
either hotkey to be held, including immediately after master ON.
While master is OFF, both hotkeys and
**Show/Hide** cannot show the crosshair. Each binding supports its own
Ctrl/Shift/Alt modifiers.
Click a hotkey field and press a key or mouse button (left, right, middle, or
side); Esc cancels and Backspace clears it.
Keep the window open (it can be minimized) to use hotkeys. Closing it releases
the RTSS overlay unless "Leave crosshair visible when closing" is checked.
The arrow buttons beside X/Y adjust one RTSS position unit per click and save
immediately; the on-screen pixel distance depends on RTSS's OSD settings.

The previous text interface is available as the optional `RivaCrossConsole.exe` build.

Optional OCAT build
-------------------

`RivaCrossOCAT.exe` publishes the same presets and hotkey visibility to a named
shared-memory block for a compatible, modified OCAT game overlay. It does not
use RTSS. Stock OCAT does not read this block or draw the custom crosshair.
With `ocat/bin/gameoverlay64-flip.dll` beside the GUI, click **Watch launch...**
and select the actual 64-bit game EXE (not Steam/WeGame). Keep the GUI open,
then start the game normally from its platform. No per-PID attach is needed.
The selection is saved in `config/ocat-watch.ini`; the next GUI launch restores
the watcher. Choose **Stop watching** to disable it and stop rendering. If the
game was already running, restart it to check the startup path.

Before arming a hook, the GUI runs a separate graphics probe with a 7-second
deadline. It caches DXGI/D3D12 method offsets and module identities. The target
checks the module path, PE timestamp, image size, and executable memory before
using those offsets. It creates no temporary D3D device inside the game. A
different runtime module causes that cached path to be skipped.

The watcher registers an x64 global `WH_CBT` hook for the current desktop through
`ocat/bin/RivaCrossOCATHook64.dll`. This small entry imports only KERNEL32 and
USER32, with its runtime linked statically (about 103 KiB). Windows may map it
into other desktop programs, but the callback checks the selected full executable
path before a worker loads `gameoverlay64-flip.dll`. The worker handles
graphics initialization; the callback does not create graphics devices. The
renderer checks the GUI's heartbeat before each draw and stops on revocation or
after 3 seconds without a heartbeat. Loaded rendering code stays until the game
exits. Only one executable is selected at a time; the GUI tracks one instance.
If a newly found window has no DLL acknowledgement, the watcher also tries a
thread-specific `WH_CALLWNDPROC` hook and one `WM_NULL` message (500 ms timeout).
The startup hook remains installed for later instances. A delivered message
still does not confirm that Windows ran our callback inside that game.
The V2 status protocol separately records callback execution, control-read
errors, renderer-load/start errors, Present, and draw submission. Local D3D12
startup and running-window checks pass with the split entry. In the 2026-10-17
Apex check, TEST remained absent; a kernel image trace recorded 135 image loads
for the game PID, including Steam's overlay, but neither RivaCross DLL. Apex
compatibility remains unresolved.

The optional `ocat/bin/DLLInjector64.exe` supports the older explicit paths.
Click **Launch game...** and select a 64-bit game executable. The GUI
starts it suspended, asks OCAT's injector to load the modified overlay, then
resumes it even if injection fails. Games that require their own launcher may
reject direct EXE launch.

For an already-running, unprotected D3D11 or D3D12 program, click
**Attach running...**. The picker lists visible 64-bit processes with the
required process rights that have loaded `d3d11.dll` or `d3d12.dll`. It also
shows accessible processes whose module scan failed, marked as unverified.
Select one and check for the TEST marker and crosshair in its window. D3D12 late attachment is
experimental; unverified entries require a warning confirmation. This list
does not certify that the DLL can load or render. The injector's success report
confirms only a load or renderer-start request; it does not
prove that the crosshair rendered. The late D3D12 renderer stops drawing when it
observes multiple direct queues for one device or a `ResizeBuffers1` call that
supplies Present queues, because it cannot then identify the right queue.
The bundled renderer does not automatically inject into child processes.
Manual attachment authorizes only the PID you select. Diagnostics are written
under `Documents\OCAT\Logs` in `PresentMonLog.txt` and `GameOverlayLog.txt`.

**Attach window...** installs a `WH_CALLWNDPROC` hook on a selected visible
64-bit window thread. It sends one `WM_NULL` wake message with a 500 ms timeout,
so this path can run the hook without waiting for a new CBT event. It requires
neither remote process-memory writes nor an external graphics-module scan.
Keep the GUI open while using this path; closing it removes the hook. A delivered
message confirms only the wake request; the per-PID status must still report DLL
execution and drawing. **Watch launch...** provides the RTSS-style desktop-wide
CBT entry before the game starts.
The GUI limits each injector attempt to 8 seconds. In a local D3D12 test,
RTSS's loaded `RTSSHooks64.dll` prevented OCAT's late Present hook from running;
the same test initialized OCAT rendering after RTSS was closed. The running-app
picker labels D3D12 processes that have the RTSS hook module loaded, including
apps that also load D3D11. This indicates a possible conflict, not proof that
RTSS hooked that swapchain; **Try attach** remains available. If rendering fails,
exit RTSS and restart the target app to unload its RTSS hook. An injector timeout
leaves the remote load result unknown; the DLL may still load later.
It draws into the game's own flip-model swapchain, without a separate crosshair
window. That avoids adding desktop content over the game, but does not prove
that Windows kept Independent Flip. Compare per-frame `PresentMode` before and
after enabling the crosshair; treat any deviation from your required mode as
a failed compatibility check. This measures past frames and cannot guarantee
zero mode changes. A display's built-in crosshair is outside the Windows
composition path.
Anti-cheat-protected games may block attachment or penalize injected third-party
software; this project does not certify account safety. Keep RivaCrossOCAT open
(it may be minimized) while playing. Closing the GUI hides the crosshair and
releases the shared-memory block. Position uses a 960 x 540 reference grid. The bundled
DLL's launch-time renderer targets Direct3D 11 and 12; Vulkan is not included.

In the OCAT GUI, the four arrow buttons beside X/Y move the crosshair by one
actual game-image pixel per click. The signed Fine X/Y offsets are saved with
the preset; existing presets start with zero offsets.
Restart a game that already loaded an older OCAT DLL before testing the new build.

After launch or attach, the GUI reports progress for the selected PID: DLL worker
started, DXGI Present reached, and draw commands issued. The last receipt
confirms work in the game's rendering path, not that the crosshair was visible
on screen or that Independent Flip was retained. The small `TEST` marker stays
in the top-right corner while rendering is active, even when the crosshair is hidden.
Starting from Steam is supported when **Watch launch...** has armed that EXE.
Select the actual game's executable, not its launcher; the window title shows the
full watched path. If the game is already running, restart it after arming the watcher.
CS2 is excluded from all OCAT loading paths. [Valve says targeted third-party injection into
CS2 Trusted Mode can risk a VAC ban](https://help.steampowered.com/en/faqs/view/09A0-4879-4353-EF95).

`tests/startup_watch_check.cpp` and its PowerShell runner check startup loading and
attachment to an already initialized window (`-WindowHook`), including
actual green crosshair and yellow TEST pixels, exclusion of an unselected EXE, and stopping
after revocation, for both D3D11 and D3D12. Each process has a 7.5-second hard
deadline; the runner adds an 8-second limit. See [OCAT build notes](ocat/BUILD.md).

For bug reports or feature suggestions please use the 
[Issues section](https://github.com/irseny/RivaCross/issues).

Manual Build
------------

From an MSVC developer command prompt, run `nmake` in the repository root.
Alternatively, run `build.bat` from any directory. This builds the Windows GUI
as `RivaCross.exe` using only Windows system libraries; PDCurses is not needed.

Run `nmake ocat` or `build.bat ocat` for the optional `RivaCrossOCAT.exe`.
It also uses only Windows system libraries, but needs the compatible OCAT
renderer described above to display anything in a game.

To build the optional text interface, run `nmake console` or `build.bat console`.
It produces `RivaCrossConsole.exe` and requires `lib/pdcurses.lib`. If the
library is missing, run `git submodule update --init`, then build PDCurses in
`external/pdcurses/wincon` with `nmake -f Makefile.vc WIDE=Y UTF8=Y` and copy
`pdcurses.lib` to `lib`.

With MinGW, run `mingw32-make -f Makefile.mingw` for the GUI. The optional
`ocat` target builds `RivaCrossOCAT.exe`; the `console` target additionally
needs a MinGW-built `lib/pdcurses.a`.

Limitations
-----------
* The crosshair is limited to one line of text
  - any further work in this direction is unlikely
* The optional text interface does not necessarily understand all keys
  - probably a limitation of the technology and not easily fixable
* Availability of colorization and scaling features depend on RTSS rendering mode
  - try out a different rendering mode (Vector2D, Vector3D, Raster)
* The crosshair positioning is not very fine grained
  - this seems to be a limitation of RTSS
* The crosshair cannot be positioned to the left of the MSI Afterburner overlay
  - no workaround so far (negative position values are not interpreted as such by RTSS)
* Running the application multiple times does not yield multiple independent crosshairs
  - this behaviour exists for simplicity reasons and is required for the "hard exit" feature
* Configuration presets are stored in the `config` folder
  - see [`config/HELPME.md`](config/HELPME.md) for formatting information

Credits
-------

The initial plan to create this tool has been elevated by the existing
[RTSS_Crosshair](https://github.com/RecursiveLife/RTSS_Crosshair) project.<br>
Parts of the codebase by
[Slava (RecursiveLife)](https://github.com/RecursiveLife)
have been used as a starting point for RivaCross.

Special thanks to
[Guru3D](https://www.guru3d.com)
for [RivaTuner Statistics Server](https://www.guru3d.com/content-page/rivatuner.html)
and the RTSS SDK.

License
-------

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

