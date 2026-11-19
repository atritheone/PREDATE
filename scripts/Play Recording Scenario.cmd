@echo off
setlocal
cd /d "%~dp0.."
python "%~dp0recording.py"
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
