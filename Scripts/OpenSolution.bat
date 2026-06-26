@echo off
rem OpenSolution.ps1 のダブルクリック / コマンドプロンプト用ラッパー。
rem 例: OpenSolution.bat            （build/debug/Witch.slnx を開く）
rem     OpenSolution.bat release    （-Config release として渡る）
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=debug"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0OpenSolution.ps1" -Config %CONFIG%
exit /b %ERRORLEVEL%
