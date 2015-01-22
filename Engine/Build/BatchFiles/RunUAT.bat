@echo off
setlocal 

echo Running AutomationTool...

rem ## Unreal Engine 4 AutomationTool setup script
rem ## Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

rem ## This script is expecting to exist in the UE4/Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

set UATExecutable=AutomationToolLauncher.exe
set UATDirectory=Binaries\DotNET\
set UATNoCompileArg=

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

echo path="%path%"

rem ## Check for Visual Studio 2013

pushd %~dp0
call GetVSComnToolsPath 12
popd

if "%VsComnToolsPath%" == "" goto NoVisualStudio2013Environment
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile


rem ## Check for Visual Studio 2012
:NoVisualStudio2013Environment

pushd %~dp0
call GetVSComnToolsPath 11
popd

if "%VsComnToolsPath%" == "" goto RunPrecompiled
call "%VsComnToolsPath%/../../VC/bin/x86_amd64/vcvarsx86_amd64.bat" >NUL
goto ReadyToCompile


rem ## ok, well it doesn't look like visual studio is installed, let's try running the precompiled one.
:RunPrecompiled

if not exist Binaries\DotNET\AutomationTool.exe goto Error_NoFallbackExecutable

set UATNoCompileArg=-NoCompile

if not exist Binaries\DotNET\AutomationToolLauncher.exe set UATExecutable=AutomationTool.exe
goto DoRunUAT


:ReadyToCompile
msbuild /nologo /verbosity:quiet Source\Programs\AutomationToolLauncher\AutomationToolLauncher.csproj /property:Configuration=Development /property:Platform=AnyCPU
if not %ERRORLEVEL% == 0 goto Error_UATCompileFailed
msbuild /nologo /verbosity:quiet Source\Programs\AutomationTool\AutomationTool.csproj /property:Configuration=Development /property:Platform=AnyCPU
if not %ERRORLEVEL% == 0 goto Error_UATCompileFailed


rem ## Run AutomationTool
:DoRunUAT
pushd %UATDirectory%
%UATExecutable% %* %UATNoCompileArg%
if not %ERRORLEVEL% == 0 goto Error_UATFailed
popd

rem ## Success!
goto Exit


:Error_BatchFileInWrongLocation
echo RunUAT.bat ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
goto Exit_Failure

:Error_NoVisualStudioEnvironment
echo RunUAT.bat ERROR: A valid version of Visual Studio 2013 or Visual Studio 2012 does not appear to be installed.
goto Exit_Failure

:Error_NoFallbackExecutable
echo RunUAT.bat ERROR: Visual studio and/or AutomationTool.csproj was not found, nor was Engine\Binaries\DotNET\AutomationTool.exe. Can't run the automation tool.
goto Exit_Failure

:Error_UATCompileFailed
echo RunUAT.bat ERROR: AutomationTool failed to compile.
goto Exit_Failure


:Error_UATFailed
echo copying UAT log files...
if not "%uebp_LogFolder%" == "" copy log*.txt %uebp_LogFolder%\UAT_*.*
rem if "%uebp_LogFolder%" == "" copy log*.txt c:\LocalBuildLogs\UAT_*.*
popd
echo RunUAT.bat ERROR: AutomationTool was unable to run successfully.
goto Exit_Failure

:Exit_Failure
echo BUILD FAILED
popd
exit /B %ERRORLEVEL%

:Exit
rem ## Restore original CWD in case we change it
popd
exit /B 0


