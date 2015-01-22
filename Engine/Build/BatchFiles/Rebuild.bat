@echo off

REM %1 is the game name
REM %2 is the platform name
REM %3 is the configuration name

IF EXIST "%~dp0\Clean.bat" (
	call "%~dp0\Clean.bat" %*
) ELSE (
	ECHO Clean.bat not found in %~dp0 
	EXIT /B 999
)

IF EXIST "%~dp0\Build.bat" (
	call "%~dp0\Build.bat" %*
) ELSE (
	ECHO Build.bat not found in %~dp0 
	EXIT /B 999
)
