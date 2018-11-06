@echo off
set /p projName= Project name: 

call build\tools\lua53.exe build\new_project.lua %projName% "AppSample" || pause

if %ERRORLEVEL% equ 0 
(
	call build\tools\premake5.exe --file=build\premake.lua vs2017 || pause
)