@echo off

echo Setting up Rocket project files...

rem ## Rocket Visual Studio project setup script
rem ## Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

rem ## This script is expecting to exist in the Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
rem ## verify that our relative path to the /Engine/Source directory is correct
if not exist "%~dp0..\..\Source" goto Error_BatchFileInWrongLocation


rem ## Change the CWD to /Engine/Source.  We always need to run UnrealBuildTool from /Engine/Source!
pushd "%~dp0..\..\Source"
if not exist ..\Build\BatchFiles\RocketGenerateProjectFiles.bat goto Error_BatchFileInWrongLocation


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
goto Error_NoVisualStudioEnvironment


:ReadyToCompile
rem ## Run UnrealBuildTool to generate Visual Studio solution and project files
rem ## NOTE: We also pass along any arguments to the GenerateProjectFiles.bat here
..\Binaries\DotNET\UnrealBuildTool.exe -ProjectFiles -rocket %*
if not %ERRORLEVEL% == 0 goto Error_ProjectGenerationFailed

rem ## Success!
goto Exit


:Error_BatchFileInWrongLocation
echo GenerateProjectFiles ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
pause
goto Exit


:Error_NoVisualStudioEnvironment
echo GenerateProjectFiles ERROR: A valid version of Visual Studio 2013 does not appear to be installed.
pause
goto Exit


:Error_ProjectGenerationFailed
echo GenerateProjectFiles ERROR: UnrealBuildTool was unable to generate project files.
pause
goto Exit


:Exit
rem ## Restore original CWD in case we change it
popd

