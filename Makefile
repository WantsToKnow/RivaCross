CXX = cl.exe
CXXFLAGS = /nologo /utf-8 /I include
LINK = link.exe
WIN_LIBS = kernel32.lib user32.lib gdi32.lib comdlg32.lib
GUI_OBJS = obj\GuiMain.obj obj\GuiCore.obj obj\GuiRTSShared.obj
OCAT_OBJS = obj\OCATMain.obj obj\GuiCore.obj obj\OCATShared.obj
CONSOLE_OBJS = obj\ConsoleMain.obj obj\ConsoleRivaCross.obj obj\GuiCore.obj obj\GuiRTSShared.obj

all: RivaCross.exe

RivaCross.exe: $(GUI_OBJS) Makefile
	$(LINK) /NOLOGO $(GUI_OBJS) /OUT:$@ /SUBSYSTEM:WINDOWS /MANIFESTUAC:level='requireAdministrator' $(WIN_LIBS) comctl32.lib gdiplus.lib

ocat: RivaCrossOCAT.exe ocat\bin\RivaCrossOCATHook64.dll

ocat\bin\RivaCrossOCATHook64.dll: src\OCATHook.cpp src\OCATWatchShared.h src\OCATStatus.h
	$(CXX) /nologo /utf-8 /MT /O1 /GS /GR- /LD /Foobj\OCATHook.obj src\OCATHook.cpp /link /OUT:$@ /IMPLIB:obj\OCATHook.lib /OPT:REF /OPT:ICF kernel32.lib user32.lib

RivaCrossOCAT.exe: $(OCAT_OBJS) Makefile
	$(LINK) /NOLOGO $(OCAT_OBJS) /OUT:$@ /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTUAC:level='requireAdministrator' $(WIN_LIBS)

console: RivaCrossConsole.exe

RivaCrossConsole.exe: $(CONSOLE_OBJS)
	$(LINK) /NOLOGO $(CONSOLE_OBJS) /OUT:$@ /SUBSYSTEM:CONSOLE /MANIFESTUAC:level='requireAdministrator' $(WIN_LIBS) advapi32.lib gdiplus.lib lib\pdcurses.lib

obj\GuiMain.obj: src\Main.cpp src\RivaCross.h src\RTSShared.h
	$(CXX) $(CXXFLAGS) /c /Fo$@ src\Main.cpp

obj\OCATMain.obj: src\Main.cpp src\RivaCross.h src\RTSShared.h src\OCATShared.h src\OCATCrosshairShared.h src\OCATStatus.h src\OCATWatchShared.h src\OCATProbeShared.h
	$(CXX) $(CXXFLAGS) /DRIVACROSS_OCAT /c /Fo$@ src\Main.cpp

obj\OCATShared.obj: src\OCATShared.cpp src\OCATShared.h src\RTSShared.h src\OCATCrosshairShared.h src\OCATStatus.h src\OCATWatchShared.h src\OCATProbeShared.h
	$(CXX) $(CXXFLAGS) /c /Fo$@ src\OCATShared.cpp

obj\ConsoleMain.obj: src\ConsoleMain.cpp src\RivaCross.h src\RTSShared.h
	$(CXX) $(CXXFLAGS) /c /Fo$@ src\ConsoleMain.cpp

obj\GuiCore.obj: src\Core.cpp src\RivaCross.h src\RTSShared.h
	$(CXX) $(CXXFLAGS) /c /Fo$@ src\Core.cpp

obj\ConsoleRivaCross.obj: src\RivaCross.cpp src\RivaCross.h src\RTSShared.h
	$(CXX) $(CXXFLAGS) /c /Fo$@ src\RivaCross.cpp

obj\GuiRTSShared.obj: src\RTSShared.cpp src\RTSShared.h
	$(CXX) $(CXXFLAGS) /c /Fo$@ src\RTSShared.cpp

clean:
	-if exist RivaCross.exe del RivaCross.exe
	-if exist RivaCrossOCAT.exe del RivaCrossOCAT.exe
	-if exist RivaCrossConsole.exe del RivaCrossConsole.exe
	-if exist obj\Gui*.obj del obj\Gui*.obj
	-if exist obj\OCAT*.obj del obj\OCAT*.obj
	-if exist obj\Console*.obj del obj\Console*.obj
