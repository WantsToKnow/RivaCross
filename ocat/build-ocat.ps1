param(
  [Parameter(Mandatory = $true)][string]$OcatSource,
  [Parameter(Mandatory = $true)][string]$VulkanSdk
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $OcatSource).Path
$sdk = (Resolve-Path -LiteralPath $VulkanSdk).Path
$expectedCommit = 'a7652f6ecb313342a7be05b6ba8d030452aa0600' # OCAT v1.6.3

if (-not [Environment]::Is64BitProcess) { throw 'Run this script in 64-bit PowerShell' }
if (-not (Test-Path -LiteralPath (Join-Path $source '.git'))) { throw 'OcatSource must be a Git checkout' }
$head = & git -C $source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or -not $head -or $head.Trim() -ne $expectedCommit) {
  throw "Expected OCAT v1.6.3 ($expectedCommit), found $head"
}
if (-not (Test-Path -LiteralPath (Join-Path $sdk 'Include\vulkan\vulkan.h'))) {
  throw "Vulkan headers not found under $sdk\Include"
}

$patchNames = @('ocat-crosshair.patch', 'ocat-no-vr.patch', 'ocat-late-d3d11.patch')
# The last patch changes context used by the earlier ones; recognize a fully patched tree first.
$ErrorActionPreference = 'Continue'
& git -C $source apply --reverse --check (Join-Path $PSScriptRoot $patchNames[-1]) 2>$null
$fullyPatched = $LASTEXITCODE -eq 0
$ErrorActionPreference = 'Stop'
if (-not $fullyPatched) {
  foreach ($patchName in $patchNames) {
    $patch = Join-Path $PSScriptRoot $patchName
    $ErrorActionPreference = 'Continue' # Windows PowerShell treats expected git stderr as an error.
    & git -C $source apply --check $patch 2>$null
    $canApply = $LASTEXITCODE -eq 0
    $alreadyApplied = $false
    if (-not $canApply) {
      & git -C $source apply --reverse --check $patch 2>$null
      $alreadyApplied = $LASTEXITCODE -eq 0
    }
    $ErrorActionPreference = 'Stop'
    if ($canApply) {
      & git -C $source apply $patch
      if ($LASTEXITCODE -ne 0) { throw "Could not apply $patchName" }
    } elseif (-not $alreadyApplied) {
      throw "$patchName does not apply cleanly"
    }
  }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio 2022 Build Tools not found' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'Visual C++ build tools not found' }
$vs = $vs.Trim()
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'

# OCAT does not reliably rebuild GameOverlay objects when Commons changes layout.
$buildArgs = @('/t:Rebuild', '/p:Configuration=Release', '/p:Platform=x64',
  '/p:WindowsTargetPlatformVersion=10.0', "/p:SolutionDir=$source\",
  "/p:VULKAN_SDK=$sdk", '/verbosity:minimal', '/m')
foreach ($project in @('MinHook\build\VC15\libMinHook.vcxproj', 'Commons\Commons.vcxproj',
    'GameOverlay\d3d\gameoverlay.vcxproj', 'DLLInjector\DLLInjector.vcxproj')) {
  & $msbuild (Join-Path $source $project) @buildArgs
  if ($LASTEXITCODE -ne 0) { throw "Build failed: $project" }
}

$output = Join-Path $source 'x64\Release\Bin'
Get-Item (Join-Path $output 'gameoverlay64.dll'), (Join-Path $output 'DLLInjector64.exe') |
  Select-Object FullName, Length
