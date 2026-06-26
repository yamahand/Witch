@echo off
rem Wrapper for Run.ps1 (double-click / cmd).
rem   Run.bat            -> build and run debug
rem   Run.bat release    -> passes -Config release
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run.ps1" -Config %CONFIG%
exit /b %ERRORLEVEL%
