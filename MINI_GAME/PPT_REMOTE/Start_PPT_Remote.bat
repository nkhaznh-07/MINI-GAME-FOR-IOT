@echo off
chcp 65001 >nul
title PowerPoint Wireless Remote Bridge
color 0A

set "EXE_PATH=%~dp0PPT_Remote_Bridge.exe"
set "PY_PATH=%~dp0ppt_bridge.py"

if exist "%EXE_PATH%" (
    echo [+] Dang khoi dong PPT_Remote_Bridge.exe...
    "%EXE_PATH%"
    goto :END
)

if exist "%PY_PATH%" (
    echo [+] Dang khoi dong ppt_bridge.py...
    where python >nul 2>nul
    if %errorlevel% equ 0 (
        python "%PY_PATH%"
    ) else (
        py "%PY_PATH%"
    )
    goto :END
)

color 0C
echo [LOI] Khong tim thay PPT_Remote_Bridge.exe hoac ppt_bridge.py cung thu muc!

:END
echo.
pause