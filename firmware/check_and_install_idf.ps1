# ============================================================
#  check_and_install_idf.ps1
#  Kiểm tra ESP-IDF trên Windows 11, cài bản mới nhất nếu chưa có
#  Chạy: PowerShell (Run as Administrator)
#        Right-click → "Run with PowerShell"
#        hoặc: powershell -ExecutionPolicy Bypass -File check_and_install_idf.ps1
# ============================================================

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================================" -ForegroundColor Yellow
Write-Host "  ESP-IDF Checker & Installer" -ForegroundColor Yellow
Write-Host "  Board: ESP32-P4-WIFI6-POE-ETH" -ForegroundColor Yellow
Write-Host "================================================" -ForegroundColor Yellow
Write-Host ""

# ── 1. Tìm ESP-IDF đã cài ────────────────────────────────────────────────

$idfCandidates = @(
    "C:\Espressif\frameworks\esp-idf-v5.4",
    "C:\Espressif\frameworks\esp-idf-v5.3",
    "C:\esp\esp-idf",
    "$env:USERPROFILE\esp\esp-idf",
    "$env:USERPROFILE\.espressif\frameworks\esp-idf-v5.4",
    "C:\esp-idf"
)

$foundIdf = $null
foreach ($path in $idfCandidates) {
    if (Test-Path "$path\export.bat") {
        $foundIdf = $path
        break
    }
}

# Kiểm tra thêm qua registry (Espressif Windows installer)
$regKey = "HKLM:\SOFTWARE\Espressif\ESP-IDF"
if (-not $foundIdf -and (Test-Path $regKey)) {
    $regPath = (Get-ItemProperty $regKey -ErrorAction SilentlyContinue).InstallDir
    if ($regPath -and (Test-Path "$regPath\export.bat")) {
        $foundIdf = $regPath
    }
}

# Kiểm tra idf.py trong PATH
if (-not $foundIdf) {
    $idfPy = Get-Command "idf.py" -ErrorAction SilentlyContinue
    if ($idfPy) {
        $foundIdf = Split-Path (Split-Path $idfPy.Source)
    }
}

# ── 2. Báo cáo kết quả ──────────────────────────────────────────────────

if ($foundIdf) {
    Write-Host "[OK] Tìm thấy ESP-IDF tại:" -ForegroundColor Green
    Write-Host "     $foundIdf" -ForegroundColor Cyan
    Write-Host ""

    # Lấy version
    $versionFile = "$foundIdf\version.txt"
    if (Test-Path $versionFile) {
        $ver = Get-Content $versionFile -Raw
        Write-Host "     Phiên bản: $($ver.Trim())" -ForegroundColor Cyan
    } else {
        # Thử lấy từ git tag
        try {
            Push-Location $foundIdf
            $ver = & git describe --tags --abbrev=0 2>$null
            Pop-Location
            Write-Host "     Phiên bản (git): $ver" -ForegroundColor Cyan
        } catch { }
    }

    Write-Host ""
    Write-Host "Cập nhật file build.bat..." -ForegroundColor Yellow

    $buildBat = Join-Path $PSScriptRoot "build.bat"
    if (Test-Path $buildBat) {
        $content = Get-Content $buildBat -Raw
        $content = $content -replace 'set "IDF_PATH=.*"', "set `"IDF_PATH=$foundIdf`""
        Set-Content $buildBat $content -Encoding ASCII
        Write-Host "[OK] build.bat đã cập nhật IDF_PATH = $foundIdf" -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "================================================" -ForegroundColor Green
    Write-Host "  ESP-IDF SAN SANG. Buoc tiep theo:" -ForegroundColor Green
    Write-Host ""
    Write-Host "  1. Ket noi board ESP32-P4 qua USB" -ForegroundColor White
    Write-Host "  2. Kiem tra COM port (Device Manager)" -ForegroundColor White
    Write-Host "  3. Chay: build.bat COM6  (thay COM6 bang cong thuc te)" -ForegroundColor White
    Write-Host "================================================" -ForegroundColor Green

} else {
    Write-Host "[!] Chua tim thay ESP-IDF tren may tinh nay." -ForegroundColor Red
    Write-Host ""
    Write-Host "Dang tai ESP-IDF Installer cho Windows..." -ForegroundColor Yellow
    Write-Host "(Espressif Online Installer - khoang 300 MB)" -ForegroundColor Gray
    Write-Host ""

    # Tải Espressif Windows Online Installer mới nhất
    $installerUrl = "https://dl.espressif.com/dl/idf-installer/esp-idf-tools-setup-online.exe"
    $installerPath = "$env:TEMP\esp-idf-tools-setup-online.exe"

    Write-Host "Downloading: $installerUrl" -ForegroundColor Cyan
    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing
        Write-Host "[OK] Tai xong: $installerPath" -ForegroundColor Green
        Write-Host ""
        Write-Host "Dang mo installer..." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "HUONG DAN cai dat:" -ForegroundColor Cyan
        Write-Host "  1. Chon 'ESP-IDF v5.4 (recommended)'" -ForegroundColor White
        Write-Host "  2. Thu muc cai dat: C:\Espressif  (giu mac dinh)" -ForegroundColor White
        Write-Host "  3. Tick 'Register ESP-IDF in Windows PATH'" -ForegroundColor White
        Write-Host "  4. Tick 'Create Desktop shortcut (ESP-IDF CMD)'" -ForegroundColor White
        Write-Host "  5. Doi cai xong, chay lai script nay de kiem tra" -ForegroundColor White
        Write-Host ""
        Start-Process $installerPath -Wait
        Write-Host ""
        Write-Host "Cai dat hoan tat! Chay lai script nay de xac nhan." -ForegroundColor Green
    } catch {
        Write-Host "[ERROR] Khong the tai installer: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "Tai thu cong tai: https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/windows-setup.html" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Nhan Enter de thoat..."
Read-Host
