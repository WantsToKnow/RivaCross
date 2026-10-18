param(
    [Parameter(Mandatory = $true)][string]$CheckExe,
    [Parameter(Mandatory = $true)][string]$OverlayDll,
    [string]$EntryDll,
    [switch]$D3D12,
    [switch]$WindowHook
)
$ErrorActionPreference = 'Stop'
# Build from an x64 VS developer prompt:
# cl /nologo /EHsc /std:c++14 /utf-8 tests\startup_watch_check.cpp src\OCATShared.cpp /Fe:startup_watch_check.exe d3d11.lib d3d12.lib dxgi.lib user32.lib
# Each controller and target also has an internal 7.5 second hard deadline.
$checkPath = (Resolve-Path -LiteralPath $CheckExe).Path
$dllPath = (Resolve-Path -LiteralPath $OverlayDll).Path
$timer = [Diagnostics.Stopwatch]::StartNew()
$arguments = '"' + $dllPath + '"'
if ($D3D12) { $arguments += ' --d3d12' }
if ($WindowHook) { $arguments += ' --window' }
if ($EntryDll) { $arguments += ' --entry "' + (Resolve-Path -LiteralPath $EntryDll).Path + '"' }
$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName = $checkPath
$start.Arguments = $arguments
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$process = [Diagnostics.Process]::Start($start)
try {
    if (-not $process.WaitForExit(8000)) {
        $process.Kill()
        [void]$process.WaitForExit(500)
        throw 'FAIL: 8 second outer deadline exceeded.'
    }
    $output = $process.StandardOutput.ReadToEnd()
    $code = $process.ExitCode
} finally {
    $process.Dispose()
}
$timer.Stop()
Write-Host $output
Write-Host ('Controller elapsed: {0} ms; exit code: {1}' -f $timer.ElapsedMilliseconds, $code)
if ($code -ne 0) { throw 'FAIL: overlay entry pixel check failed.' }
