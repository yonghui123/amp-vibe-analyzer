# Download portable OpenOCD (xPack) into tools/openocd/
# Run once: .\setup_openocd.ps1

$ErrorActionPreference = "Stop"
$ToolsDir = Join-Path $PSScriptRoot "tools\openocd"
$Bin = Join-Path $ToolsDir "bin\openocd.exe"

if (Test-Path $Bin) {
    Write-Host "OpenOCD already installed: $Bin"
    exit 0
}

Write-Host "Installing OpenOCD via winget..."
$winget = Get-Command winget -ErrorAction SilentlyContinue
if ($winget) {
    winget install --id xpack-dev-tools.openocd-xpack -e `
        --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -eq 0) {
        Write-Host "OpenOCD installed to PATH. Restart terminal and run .\debug.ps1"
        exit 0
    }
}

Write-Host @"

Could not auto-install OpenOCD.

Option A - winget (manual):
  winget install xpack-dev-tools.openocd-xpack

Option B - download xPack OpenOCD:
  https://github.com/xpack-dev-tools/openocd-xpack/releases
  Extract to: $ToolsDir
  (expect: tools\openocd\bin\openocd.exe)

Then run: .\debug.ps1

"@

exit 1
