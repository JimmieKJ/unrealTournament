@echo off

@echo off

rem %1 is the game name
rem %2 is the platform name
rem %3 is the configuration name

echo Cleaning %1 Binaries...

rem ## Unreal Engine 4 cleanup script
rem ## Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

rem ## This script is expecting to exist in the UE4 root directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
rem ## verify that our relative path to the /Engine/Source directory is correct
if not exist "%~dp0..\..\Source" goto Error_BatchFileInWrongLocation


rem ## Change the CWD to /Engine/Source.  We always need to run UnrealBuildTool from /Engine/Source!
pushd "%~dp0..\..\Source"
if not exist ..\Build\BatchFiles\Clean.bat goto Error_BatchFileInWrongLocation


rem ## Check to see if we're already running under a Visual Studio environment shell
if not "%INCLUDE%" == "" if not "%LIB%" == "" goto ReadyToCompile


rem ## Check for Visual Studio 2013

pushd %~dp0
call GetVSComnToolsPath 12
popd

if "%VsComnToolsPath%" == "" goto NoVisualStudio2013Environment
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile

:NoVisualStudio2013Environment
rem ## Check for Visual Studio 2012

pushd %~dp0
call GetVSComnToolsPath 11
popd

if "%VsComnToolsPath%" == "" goto NoVisualStudio2012Environment
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile


:NoVisualStudio2012Environment
rem ## User has no version of Visual Studio installed?
goto Error_NoVisualStudioEnvironment


:ReadyToCompile
if not exist Programs\UnrealBuildTool\UnrealBuildTool.csproj goto ReadyToClean
msbuild /nologo /verbosity:quiet Programs\UnrealBuildTool\UnrealBuildTool.csproj /property:Configuration=Development /property:Platform=AnyCPU
if not %ERRORLEVEL% == 0 goto Error_UBTCompileFailed

:ReadyToClean
IF EXIST ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe (
         ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe %* -clean
) ELSE (
	ECHO UnrealBuildTool.exe not found in ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe 
	EXIT /B 999
)
goto Exit

:Error_BatchFileInWrongLocation
echo Clean ERROR: The batch file does not appear to be located in the UE4/Engine/Build/BatchFiles directory.  This script must be run from within that directory.
pause
goto Exit

:Error_NoVisualStudioEnvironment
echo GenerateProjectFiles ERROR: A valid version of Visual Studio does not appear to be installed.
pause
goto Exit

:Error_UBTCompileFailed
echo Clean ERROR: Failed to build UnrealBuildTool.
pause
goto Exit

:Error_CleanFailed
echo Clean ERROR: Clean failed.
pause
goto Exit

:Exit

