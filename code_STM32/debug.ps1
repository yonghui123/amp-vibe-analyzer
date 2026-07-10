# OpenOCD + GDB debug for STM32F407 (works with clone ST-LINK)
#
# Usage:
#   1. Build in CubeIDE first
#   2. Close CubeIDE and STM32CubeProgrammer
#   3. .\debug.ps1           - flash + debug
#   4. .\debug.ps1 -NoFlash  - debug only (program already flashed)
#
# First time: .\setup_openocd.ps1

param(
    [switch]$NoFlash
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$ElfPath     = Join-Path $ProjectRoot "Debug\code.elf"
$GdbInit     = Join-Path $ProjectRoot "tools\gdbinit"

# winget 安装 OpenOCD 后，当前终端 PATH 可能未刷新
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" `
          + [System.Environment]::GetEnvironmentVariable("Path", "User")

function Find-Gdb {
    $fixed = "E:\workoffice\stm32cubeide\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-gdb.exe"
    if (Test-Path $fixed) { return $fixed }
    $hit = Get-ChildItem "E:\workoffice\stm32cubeide" -Recurse -Filter "arm-none-eabi-gdb.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.FullName }
    throw "arm-none-eabi-gdb not found. Install STM32CubeIDE or add GDB to PATH."
}

function Find-OpenOcd {
    $cmd = Get-Command openocd -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $local = Join-Path $ProjectRoot "tools\openocd\bin\openocd.exe"
    if (Test-Path $local) { return $local }

    $wingetRoot = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path $wingetRoot) {
        $hit = Get-ChildItem $wingetRoot -Recurse -Filter "openocd.exe" -ErrorAction SilentlyContinue |
               Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }

    throw "OpenOCD not found. Run: .\setup_openocd.ps1  (then reopen PowerShell)"
}

if (-not (Test-Path $ElfPath)) {
    throw "Missing $ElfPath - build in CubeIDE first."
}

$Gdb    = Find-Gdb
$OpenOcd = Find-OpenOcd

# 烧录前必须先释放 ST-LINK（OpenOCD/Programmer 会占用）
Get-Process -Name openocd -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "Stopping leftover OpenOCD (PID $($_.Id))..."
    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
}

if (-not $NoFlash) {
    Write-Host "==> Flashing latest firmware..."
    & (Join-Path $ProjectRoot "flash.ps1")
}

Write-Host "==> Starting OpenOCD in new window..."
Write-Host "    (close that window to stop the debug session)"
$ocdArgs = "-f interface/stlink.cfg -f target/stm32f4x.cfg -c `"bindto 127.0.0.1`" -c `"gdb port 5000`""
Start-Process -FilePath $OpenOcd -ArgumentList $ocdArgs -WindowStyle Normal

Start-Sleep -Seconds 2

Write-Host "==> Starting GDB: $Gdb"
Write-Host @"

GDB commands:
  c / continue     - run
  n / next         - step over
  s / step         - step into
  bt               - backtrace (where am I?)
  info threads     - list FreeRTOS threads
  break GUI_TaskHandler
  print g_tabview
  quit             - exit (then close OpenOCD window)

"@

& $Gdb -q $ElfPath -x $GdbInit
