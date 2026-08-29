@echo off
setlocal
title Zero Company Mandalorian Wardrobe Installer
echo Installing Zero Company Mandalorian Wardrobe...
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Wardrobe.ps1" -Mode Enabled
set "INSTALL_EXIT=%ERRORLEVEL%"
echo.
if not "%INSTALL_EXIT%"=="0" (
    echo INSTALL FAILED. Read the error above. No game save was changed.
) else (
    echo INSTALL COMPLETE. The wardrobe is enabled.
)
echo.
pause
exit /b %INSTALL_EXIT%
