@echo off
py -3 "%~dp0world.py" play --profile development
if errorlevel 1 pause
