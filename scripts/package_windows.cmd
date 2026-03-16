@echo off
setlocal EnableExtensions

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package_windows.ps1" %*
exit /b %errorlevel%
