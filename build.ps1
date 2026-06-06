# Windows build helper: loads MSVC (vcvars) then runs CMake.
# Usage:
#   .\build.ps1              # configure (if needed) + build
#   .\build.ps1 -Run         # build + run
#   .\build.ps1 -Reconfigure # force cmake configure

param(
  [switch]$Reconfigure,
  [switch]$Run,
  [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"

function Get-VcVars64Bat {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install 'Desktop development with C++' (Visual Studio Build Tools)."
  }

  $installPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath 2>$null

  if ([string]::IsNullOrWhiteSpace($installPath)) {
    throw "MSVC C++ toolchain not found. Install Visual Studio 2022 Build Tools with C++ workload."
  }

  $vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
  if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at: $vcvars"
  }

  return $vcvars
}

function Invoke-VcBuild([string]$Command) {
  $vcvars = Get-VcVars64Bat
  cmd /c "`"$vcvars`" >nul && $Command"
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

$needsConfigure = $Reconfigure -or -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))
if ($needsConfigure) {
  Write-Host "CMake configure ($Generator) ..."
  Invoke-VcBuild "cmake -S `"$ProjectRoot`" -B `"$BuildDir`" -G $Generator"
}

Write-Host "CMake build ..."
Invoke-VcBuild "cmake --build `"$BuildDir`""

$exe = Join-Path $BuildDir "chrono_rush_demo.exe"
if ($Run) {
  if (-not (Test-Path $exe)) {
    $exe = Join-Path $BuildDir "Debug\chrono_rush_demo.exe"
  }
  if (-not (Test-Path $exe)) {
    throw "Executable not found under build/"
  }
  Write-Host "Run: $exe"
  & $exe
}
