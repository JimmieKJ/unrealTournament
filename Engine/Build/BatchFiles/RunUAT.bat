@echo off
setlocal 

echo Running AutomationTool...

rem ## Unreal Engine 4 AutomationTool setup script
rem ## Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

rem ## This script is expecting to exist in the UE4/Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

set UATExecutable=AutomationToolLauncher.exe
set UATDirectory=Binaries\DotNET\
set UATCompileArg=-compile

rem ## Change the CWD to /Engine. 
pushd %~dp0..\..\
if not exist Build\BatchFiles\RunUAT.bat goto Error_BatchFileInWrongLocation

rem ## Use the pre-compiled UAT scripts if -nocompile is specified in the command line
for %%P in (%*) do if /I "%%P" == "-nocompile" goto RunPrecompiled

rem ## check for force precompiled
if not "%ForcePrecompiledUAT%"=="" goto RunPrecompiled

if not exist Source\Programs\AutomationTool\AutomationTool.csproj goto RunPrecompiled
if not exist Source\Programs\AutomationToolLauncher\AutomationToolLauncher.csproj goto RunPrecompiled

rem ## Check to see if we're already running under a Visual Studio environment shell
if not "%INCLUDE%" == "" if not "%LIB%" == "" goto ReadyToCompile

rem ## Check for Visual Studio 2015
for %%P in (%*) do if "%%P" == "-2013" goto NoVisualStudio2015Environment

pushd %~dp0
call GetVSComnToolsPath 14
popd

if "%VsComnToolsPath%" == "" goto NoVisualStudio2015Environment
rem ## Check if the C++ toolchain is not installed
if not exist "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" goto NoVisualStudio2015Environment
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile


rem ## Check for Visual Studio 2013
:NoVisualStudio2015Environment

pushd %~dp0
call GetVSComnToolsPath 12
popd

if "%VsComnToolsPath%" == "" goto NoVisualStudio2013Environment
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile



rem ## Check for Visual Studio 2013

pushd %~dp0
call GetVSComnToolsPath 12
popd

if "%VsComnToolsPath%" == "" goto NoVisualStudio2013Environment
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile

:NoVisualStudio2013Environment

rem ## ok, well it doesn't look like visual studio is installed, let's try running the precompiled one.
:RunPrecompiled

if not exist Binaries\DotNET\AutomationTool.exe goto Error_NoFallbackExecutable

set UATCompileArg=

if not exist Binaries\DotNET\AutomationToolLauncher.exe set UATExecutable=AutomationTool.exe
goto DoRunUAT


:ReadyToCompile
msbuild /nologo /verbosity:quiet Source\Programs\AutomationToolLauncher\AutomationToolLauncher.csproj /property:Configuration=Development /property:Platform=AnyCPU
if not %ERRORLEVEL% == 0 goto Error_UATCompileFailed
msbuild /nologo /verbosity:quiet Source\Programs\AutomationTool\AutomationTool.csproj /property:Configuration=Development /property:Platform=AnyCPU /property:AutomationToolProjectOnly=true
if not %ERRORLEVEL% == 0 goto Error_UATCompileFailed


rem ## Run AutomationTool
:DoRunUAT
pushd %UATDirectory%
%UATExecutable% %* %UATCompileArg%
if not %ERRORLEVEL% == 0 goto Error_UATFailed
popd

rem ## Success!
goto Exit


:Error_BatchFileInWrongLocation
echo RunUAT.bat ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
set RUNUAT_EXITCODE=1
goto Exit_Failure

:Error_NoVisualStudioEnvironment
echo RunUAT.bat ERROR: A valid version of Visual Studio 2015 does not appear to be installed.
set RUNUAT_EXITCODE=1
goto Exit_Failure

:Error_NoFallbackExecutable
echo RunUAT.bat ERROR: Visual studio and/or AutomationTool.csproj was not found, nor was Engine\Binaries\DotNET\AutomationTool.exe. Can't run the automation tool.
set RUNUAT_EXITCODE=1
goto Exit_Failure

:Error_UATCompileFailed
echo RunUAT.bat ERROR: AutomationTool failed to compile.
set RUNUAT_EXITCODE=1
goto Exit_Failure


:Error_UATFailed
set RUNUAT_EXITCODE=%ERRORLEVEL%
goto Exit_Failure

:Exit_Failure
echo BUILD FAILED
popd
exit /B %RUNUAT_EXITCODE%

:Exit
rem ## Restore original CWD in case we change it
popd
exit /B 0


