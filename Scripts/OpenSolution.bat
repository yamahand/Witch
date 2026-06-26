@echo off
rem Wrapper for OpenSolution.ps1 (double-click / cmd).
rem   OpenSolution.bat            -> open build/debug/Witch.slnx
rem   OpenSolution.bat release    -> passes -Config release
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0OpenSolution.ps1" -Config %CONFIG%
exit /b %ERRORLEVEL%
