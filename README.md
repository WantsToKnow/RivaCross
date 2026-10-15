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
visible after exit. Configure a main hotkey in hold or toggle mode and an
optional master toggle hotkey in the same window.
Click a hotkey field and press a key or mouse button (left, right, middle, or
side); Esc cancels and Backspace clears it.
Keep the window open (it can be minimized) to use hotkeys. Closing it releases
the RTSS overlay unless "Leave crosshair visible when closing" is checked.

The previous text interface is available as the optional `RivaCrossConsole.exe` build.

Optional OCAT build
-------------------

`RivaCrossOCAT.exe` publishes the same presets and hotkey visibility to a named
shared-memory block for a compatible, modified OCAT game overlay. It does not
use RTSS. Stock OCAT does not read this block or draw the custom crosshair.
With `ocat/bin/gameoverlay64.dll` and `ocat/bin/DLLInjector64.exe` beside the
GUI, click **Launch game...** and select a 64-bit game executable. The GUI
starts it suspended, asks OCAT's injector to load the modified overlay, then
resumes it even if injection fails. A success report from the injector does not
prove the DLL loaded or the crosshair rendered. Games that require their own
launcher may reject direct EXE launch. Keep RivaCrossOCAT open (it may be
minimized) while playing. Closing the GUI hides the crosshair and releases the
shared-memory block. Position uses a 960 x 540 reference grid. The GUI cannot
verify whether a game actually displays the crosshair. The bundled DLL targets
Direct3D 11 and 12; Vulkan is not included.

The OCAT DLL currently draws a large red/yellow `TEST` label near the crosshair
position (the screen center without the GUI), even if the crosshair is hidden.
This temporary marker confirms only that the DLL reached the game's renderer.
Steam-launched games are not injected by `RivaCrossOCAT.exe`, so the marker will
not appear in Steam-launched CS2. Do not try to force the DLL into CS2 Trusted
Mode: [Valve says targeted third-party injection can risk a VAC ban](https://help.steampowered.com/en/faqs/view/09A0-4879-4353-EF95).

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

