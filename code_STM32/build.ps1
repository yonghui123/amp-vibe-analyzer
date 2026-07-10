# Build code_STM32 Debug via CubeIDE toolchain (bypasses CubeIDE sh.exe Error 87)
# Usage:
#   .\build.ps1          - incremental build
#   .\build.ps1 -Clean   - clean + full rebuild

param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$DebugDir    = Join-Path $ProjectRoot "Debug"
$ElfPath     = Join-Path $DebugDir "code.elf"

function Find-CubeTool {
    param([string]$Name)
    $root = "E:\workoffice\stm32cubeide"
    if (-not (Test-Path $root)) {
        throw "STM32CubeIDE not found at $root"
    }
    $hit = Get-ChildItem $root -Recurse -Filter $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $hit) { throw "$Name not found under $root" }
    return $hit.FullName
}

$Gcc  = Find-CubeTool "arm-none-eabi-gcc.exe"
$Make = Find-CubeTool "make.exe"
$ToolBin = Split-Path $Gcc -Parent
$env:Path = "$ToolBin;$env:Path"

Push-Location $DebugDir
try {
    if ($Clean) {
        Write-Host "Cleaning..."
        & $Make clean 2>$null
    }
    Write-Host "Building (make all -j1)..."
    & $Make all -j1
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
}
finally {
    Pop-Location
}

if (-not (Test-Path $ElfPath)) {
    throw "Missing $ElfPath after build"
}

Write-Host "Build OK: $ElfPath"
& (Join-Path $ToolBin "arm-none-eabi-size.exe") $ElfPath
