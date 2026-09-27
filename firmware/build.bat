@echo off

:: ============================================================
::  CAN MAY XUC V3 -- Build, Flash & Monitor
::  Board : ESP32-P4-WIFI6-POE-ETH
::  IDF   : 5.5.4
::  Usage : build.bat [COM_PORT] [BAUD]
:: ============================================================

set "COM_PORT=COM6"
set "BAUD=115200"
set "IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.5.5"

if not "%~1" == "" set "COM_PORT=%~1"
if not "%~2" == "" set "BAUD=%~2"

echo ========================================================
echo   CAN MAY XUC V3 - ESP32-P4 Build System
echo   PORT: %COM_PORT%  ^|  BAUD: %BAUD%
echo   IDF : %IDF_PATH%
echo ========================================================

if not exist "%IDF_PATH%\export.bat" goto ERR_IDF

echo [1/5] Khoi dong moi truong ESP-IDF...
call "%IDF_PATH%\export.bat"
if %errorlevel% neq 0 goto ERR_ENV

echo [2/5] Xoa thu muc build cu...
if exist "build\" (
    rmdir /s /q "build"
    if %errorlevel% neq 0 (
        echo [ERROR] Khong the xoa thu muc build\. Dong het cac chuong trinh dang dung no roi thu lai.
        pause
        exit /b 1
    )
    echo       Da xoa build\ thanh cong.
) else (
    echo       Thu muc build\ khong ton tai, bo qua.
)

echo [3/5] Set target: esp32p4...
call idf.py set-target esp32p4
if %errorlevel% neq 0 goto ERR_TARGET

echo [4/5] Bien dich firmware (build)...
call idf.py build
if %errorlevel% neq 0 goto ERR_BUILD

echo [5/5] Nap chuong trinh len %COM_PORT%...
call idf.py -p %COM_PORT% -b 921600 flash
if %errorlevel% neq 0 goto ERR_FLASH

echo ========================================================
echo   Monitor bat dau. Nhan Ctrl+] de thoat.
echo ========================================================
call idf.py -p %COM_PORT% -b %BAUD% monitor
goto END

:ERR_IDF
echo.
echo [ERROR] Khong tim thay ESP-IDF tai: %IDF_PATH%
echo         Kiem tra lai bien IDF_PATH trong build.bat
pause
exit /b 1

:ERR_ENV
echo.
echo [ERROR] Khong the kich hoat export.bat
echo         Thu mo "ESP-IDF 5.5 CMD" tu Start Menu roi chay lai.
pause
exit /b 1

:ERR_TARGET
echo.
echo [ERROR] set-target esp32p4 that bai.
echo         Kiem tra lai ESP-IDF version (can >= 5.4).
pause
exit /b 1

:ERR_BUILD
echo.
echo [ERROR] Build that bai! Xem log o tren.
pause
exit /b 1

:ERR_FLASH
echo.
echo [ERROR] Flash that bai!
echo         - Kiem tra cap USB
echo         - Kiem tra COM port trong Device Manager
echo         - Giu nut BOOT roi nhan RESET, sau do thu lai
pause
exit /b 1

:END
echo Hoan tat.
pause
