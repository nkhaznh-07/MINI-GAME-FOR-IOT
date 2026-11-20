@echo off
chcp 65001 >nul
title Dong goi PPT_Remote_Bridge.exe cho USB
color 0B

cd /d "%~dp0"

if not exist "ppt_bridge.py" (
    color 0C
    echo [LOI] Khong tim thay file ppt_bridge.py trong thu muc nay!
    pause
    exit /b
)

echo [1/4] Dang tao moi truong ao sach de file .exe gon nhe nhat...
python -m venv build_env
if %errorlevel% neq 0 (
    color 0C
    echo [LOI] Khong khoi tao duoc Python venv. Hay kiem tra lai cai dat Python!
    pause
    exit /b
)
call build_env\Scripts\activate.bat

echo [2/4] Dang cai dat cac thu vien can thiet (pyserial, pywin32, Pillow, pyinstaller)...
python -m pip install --upgrade pip
pip install pyserial pywin32 Pillow pyinstaller

echo [3/4] Dang dong goi thanh PPT_Remote_Bridge.exe...
pyinstaller --onefile --clean --name "PPT_Remote_Bridge" ppt_bridge.py

echo [4/4] Dang don dep file tam...
move /y "dist\PPT_Remote_Bridge.exe" "%~dp0PPT_Remote_Bridge.exe" >nul
rmdir /s /q build dist build_env 2>nul
del /q PPT_Remote_Bridge.spec 2>nul

color 0A
echo.
echo ========================================================
echo   HOAN TAT! File da san sang tai:
echo   %~dp0PPT_Remote_Bridge.exe
echo ========================================================
pause