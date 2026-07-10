# Flash STM32F407 via STM32CubeProgrammer CLI (bypasses CubeIDE GDB Server)
# Usage:
#   .\flash.ps1              - flash only (build in CubeIDE first)
#   .\flash.ps1 -Build       - try make in Debug/ then flash (needs CubeIDE sh.exe in PATH)
#   .\flash.ps1 -SkipBuild   - same as default, flash only

param(
    [switch]$Build,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$ElfPath     = Join-Path $ProjectRoot "Debug\code.elf"
$CLI         = "E:\workoffice\stm32cubeide\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.400.202601091506\tools\bin\STM32_Programmer_CLI.exe"

function Release-StLink {
    $names = @("openocd", "ST-LINK_gdbserver", "STM32_Programmer_CLI")
    foreach ($name in $names) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "Stopping $($_.ProcessName) (PID $($_.Id))..."
            Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep -Seconds 1
}

function Flash-Elf {
    param([string]$Freq)
    Write-Host "Flashing $ElfPath (SWD ${Freq}kHz) ..."
    & $CLI -c port=SWD freq=$Freq -w $ElfPath -v -rst
    return ($LASTEXITCODE -eq 0)
}

if (-not (Test-Path $CLI)) {
    throw "STM32_Programmer_CLI.exe not found. Check CubeIDE install path."
}

if ($Build -and -not $SkipBuild) {
    Write-Host "Building with make (use CubeIDE Build if this fails)..."
    $make = Get-Command make -ErrorAction SilentlyContinue
    if ($make) {
        Push-Location (Join-Path $ProjectRoot "Debug")
        & make all -j1
        Pop-Location
    }
    else {
        Write-Host "make not found. Build in CubeIDE first."
    }
}
elseif (-not (Test-Path $ElfPath)) {
    throw "Missing $ElfPath - build in CubeIDE first, then run: .\flash.ps1"
}

Write-Host "Releasing ST-LINK (close OpenOCD / CubeIDE / Programmer if still connected)..."
Release-StLink

if (-not (Flash-Elf 4000)) {
    Write-Host "Retry with 1000 kHz..."
    Release-StLink
    if (-not (Flash-Elf 1000)) {
        throw @"
Flash failed (ST-LINK DEV_CONNECT_ERR).

Checklist:
  1. Close STM32CubeIDE and STM32CubeProgrammer completely
  2. Close any OpenOCD window from a previous debug.ps1 run
  3. Confirm ST-LINK USB + SWD wiring + board power
  4. Replug ST-LINK USB, then run: .\flash.ps1 -SkipBuild
"@
    }
}

Write-Host "Flash OK."
