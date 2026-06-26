@echo off
rem Run.ps1 のダブルクリック / コマンドプロンプト用ラッパー。
rem 例: Run.bat            （debug をビルドして実行）
rem     Run.bat release    （-Config release として渡る）
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run.ps1" -Config %CONFIG%
exit /b %ERRORLEVEL%
