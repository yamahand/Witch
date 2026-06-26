@echo off
rem Generate.ps1 のダブルクリック / コマンドプロンプト用ラッパー。
rem 例: Generate.bat            （debug）
rem     Generate.bat release    （-Config release として渡る）
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Generate.ps1" -Config %CONFIG%
exit /b %ERRORLEVEL%
