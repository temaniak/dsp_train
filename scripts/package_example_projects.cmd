@echo off
setlocal EnableExtensions

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package_example_projects.ps1" %*
exit /b %errorlevel%
