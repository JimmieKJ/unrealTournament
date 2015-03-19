@echo off
setlocal
pushd %~dp0

rem Sync the dependencies...
.\Engine\Binaries\DotNET\GitDependencies.exe --prompt %*
if ERRORLEVEL 1 goto error

rem Setup the git hooks...
if not exist .git\hooks goto no_git_hooks_directory
echo Registering git hooks...
echo #!/bin/sh >.git\hooks\post-checkout
echo Engine/Binaries/DotNET/GitDependencies.exe >>.git\hooks\post-checkout
echo #!/bin/sh >.git\hooks\post-merge
echo Engine/Binaries/DotNET/GitDependencies.exe >>.git\hooks\post-merge
:no_git_hooks_directory

rem Install prerequisites...
echo Installing prerequisites...
start /wait Engine\Extras\Redist\en-us\UE4PrereqSetup_x64.exe /quiet

rem Done!
goto :EOF

rem Error happened. Wait for a keypress before quitting.
:error
pause
