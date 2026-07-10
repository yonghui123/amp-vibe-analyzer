# SignalX PC Simulator - Build & Run Script
# 用法: .\build_and_run.ps1 [-Clean]

param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$BuildDir = Join-Path $PSScriptRoot "build"
$ExeName = "pc_simulator.exe"

# 停止旧进程
Write-Host "Stopping existing process..." -ForegroundColor Yellow
Stop-Process -Name ($ExeName -replace '\.exe$', '') -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 200

# 清理构建目录
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# 创建构建目录
if (-not (Test-Path $BuildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# CMake 配置（仅在需要时）
$CacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $CacheFile)) {
    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    Push-Location $BuildDir
    cmake .. -G "MinGW Makefiles"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
}

# 编译
Write-Host "Building..." -ForegroundColor Cyan
Push-Location $BuildDir
cmake --build . --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

# 确保 SDL2.dll 存在
$SDL2Dll = Join-Path $PSScriptRoot "SDL2\SDL2-2.30.3\x86_64-w64-mingw32\bin\SDL2.dll"
$TargetDll = Join-Path $BuildDir "SDL2.dll"
if ((Test-Path $SDL2Dll) -and -not (Test-Path $TargetDll)) {
    Copy-Item $SDL2Dll $TargetDll -Force
}

# 复制包络线测试数据到 build/Envelope
$TestCsv = Join-Path $PSScriptRoot "test_data\envelope_test.csv"
if (Test-Path $TestCsv) {
    $EnvDir = Join-Path $BuildDir "Envelope"
    New-Item -ItemType Directory -Path $EnvDir -Force | Out-Null
    Copy-Item $TestCsv (Join-Path $EnvDir "envelope_test.csv") -Force
}

# 运行
Write-Host "Starting $ExeName..." -ForegroundColor Green
Start-Process (Join-Path $BuildDir $ExeName)
Write-Host "Done!" -ForegroundColor Green
