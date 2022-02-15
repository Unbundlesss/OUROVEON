@echo off
cd /D "%~dp0"

"premake-bin/win64/premake5.exe" --file=premake.lua vs2019

pause
