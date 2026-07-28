@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Failed to set up MSVC environment
    exit /b 1
)

echo === Building PDCurses ===
cd /d "d:\aaa_file\study\RvCross\RivaCross-0.5.2\external\pdcurses\wincon"
nmake -f Makefile.vc clean WIDE=Y UTF8=Y
nmake -f Makefile.vc WIDE=Y UTF8=Y
if %ERRORLEVEL% NEQ 0 (
    echo PDCurses build failed
    exit /b 1
)
echo PDCurses build succeeded

echo === Copying pdcurses.lib ===
if not exist "d:\aaa_file\study\RvCross\RivaCross-0.5.2\lib" mkdir "d:\aaa_file\study\RvCross\RivaCross-0.5.2\lib"
copy /Y pdcurses.lib "d:\aaa_file\study\RvCross\RivaCross-0.5.2\lib\"

echo === Building RivaCross (clean + rebuild) ===
cd /d "d:\aaa_file\study\RvCross\RivaCross-0.5.2"
nmake clean
nmake
if %ERRORLEVEL% NEQ 0 (
    echo RivaCross build failed
    exit /b 1
)
echo === Build complete: RivaCross.exe ===
