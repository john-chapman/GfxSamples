@echo off
set /p target= Target (e.g. vs2017): 

call build\tools\premake5.exe --file=build\premake.lua %target% || pause
