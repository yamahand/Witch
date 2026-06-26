@echo off
rem Wrapper for Generate.ps1 (double-click / cmd).
rem   Generate.bat            -> debug
rem   Generate.bat release    -> passes -Config release
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Generate.ps1" -Config %CONFIG%
exit /b %ERRORLEVEL%
