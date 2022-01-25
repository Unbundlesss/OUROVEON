@echo off
cd /D "%~dp0"

"premake5.exe" --file=premake.lua vs2019

pause
