@echo off
setlocal
pushd "%~dp0"

where nmake >nul 2>&1
if errorlevel 1 (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    if errorlevel 1 (
        echo MSVC Build Tools not found. Run from a Visual Studio Developer Command Prompt.
        popd
        exit /b 1
    )
)

if /I "%~1"=="console" (
    nmake console
) else (
    nmake
)
set "build_result=%ERRORLEVEL%"
popd
exit /b %build_result%
