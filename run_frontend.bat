@echo off
setlocal enabledelayedexpansion

:: Ensure we are in the project root directory
cd /d "%~dp0"

echo ========================================================================
echo  Launching QueryLang Interactive Web IDE and Visualizer
echo ========================================================================
echo.

:: Detect Python executable
set "PY_CMD="

python --version >nul 2>&1
if !errorlevel! equ 0 (
    set "PY_CMD=python"
    goto :FOUND_PYTHON
)

py -3 --version >nul 2>&1
if !errorlevel! equ 0 (
    set "PY_CMD=py -3"
    goto :FOUND_PYTHON
)

if exist "C:\Python313\python.exe" (
    set "PY_CMD=C:\Python313\python.exe"
    goto :FOUND_PYTHON
)

if exist "C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python311\python.exe" (
    set "PY_CMD=C:\Users\%USERNAME%\AppData\Local\Programs\Python\Python311\python.exe"
    goto :FOUND_PYTHON
)

:FOUND_PYTHON
if "!PY_CMD!"=="" (
    echo [ERROR] Python was not found on your system!
    echo Please install Python 3 or ensure it is added to your PATH.
    echo.
    pause
    exit /b 1
)

echo [OK] Using Python: !PY_CMD!
echo [*] Starting QueryLang Web Server...
echo.

!PY_CMD! web\server.py

if !errorlevel! neq 0 (
    echo.
    echo [ERROR] Server terminated with an error.
    pause
)
