@echo off


echo This batch file, takes .ucap packet capture files from Oodle 'training' mode play sessions, and processes them into a dictionary file.
echo.


REM This batch file can only work from within the Oodle folder.
set BaseFolder="..\..\..\..\..\.."

if exist %BaseFolder:"=%\Engine goto SetUE4Editor

echo Could not locate Engine folder. This .bat must be located within UE4\Engine\Plugins\Runtime\PacketHandlers\CompressionComponents\Oodle
goto End


:SetUE4Editor
set UE4EditorLoc="%BaseFolder:"=%\Engine\Binaries\Win64\UE4Editor.exe"

if exist %UE4EditorLoc:"=% goto SetTrainer

echo Could not locate UE4Editor.exe
goto End


:SetTrainer
set TrainerLoc="Source\ThirdParty\NotForLicensees\OodleTrainer\x64\Release\OodleTrainer.exe"

if exist %TrainerLoc:"=% goto GetGame

echo Could not locate OodleTrainer.exe, make sure it is built.
goto End


:GetGame
set /p GameName=Type the name of the game you are working with: 
echo.

if exist %BaseFolder:"=%\%GameName:"=% goto GetCaptures
if exist %BaseFolder:"=%\Samples\Games\%GameName:"=% goto GetCaptures

set /p SkipGameCheck=Could not locate folder for game '%GameName:"=%'; continue anyway y/n?
if not "%SkipGameCheck%"=="y" goto End


:GetCaptures
echo Place all .ucap files you want to process, into a folder, and then drag+drop the folder over this prompt.
echo Output and Input capture files, should be processed separately.

set /p CapturesFolder=Drag or paste captures folder here: 
echo.

if exist %CapturesFolder% goto GetOutput

echo Could not locate folder: %CapturesFolder%
goto End


:GetOutput
set BaseOutputPath="%BaseFolder:"=%\%GameName:"=%\Content\Oodle"

if not exist "%BaseOutputPath%" mkdir "%BaseOutputPath%"

echo Type the desired name of the dictionary file. This will be placed in %GameName:"=%\Content\Oodle\
set /p OutputFile=Dictionary file name: 
echo.

if not exist %BaseOutputPath:"=%\%OutputFile:"=% goto ExecuteCommandlet

set /p OverwriteOutput=Output file already exists. Overwrite y/n?
if not "%OverwriteOutput%"=="y" goto End

echo.


:ExecuteCommandlet
set MergedPacketsFile="%~dp0TempMerge.ucap"
set CommandletParms=-run=OodleHandlerComponent.OodleTrainerCommandlet MergePackets "%MergedPacketsFile:"=%" all "%CapturesFolder:"=%
set FinalUE4CmdLine=%GameName:"=% %CommandletParms%

echo Executing Oodle MergePackets commandlet - commandline:
echo %FinalUE4CmdLine%

@echo on
%UE4EditorLoc:"=% %FinalUE4CmdLine%
@echo off
echo.


if %errorlevel%==0 goto ExecuteTrainer

echo WARNING! Detected error! Review logfile for commandlet.
goto CommandletCleanup


:ExecuteTrainer
if not exist %BaseFolder:"=%\%GameName:"=% goto AltExecuteTrainer

set FinalOutputFile="%~dp0%BaseOutputPath:"=%\%OutputFile:"=%"
set FinalTrainerCmdLine="%MergedPacketsFile:"=%" "%FinalOutputFile:"=%"

echo Executing trainer for producing final dictionary file - commandline:
echo %FinalTrainerCmdLine%

@echo on
%TrainerLoc:"=% %FinalTrainerCmdLine%
@echo off

echo.

if not %errorlevel%==0 goto TrainerError
goto TrainerSuccess


:AltExecuteTrainer
set AltOutputFile="%~dp0%OutputFile:"=%"
set FinalAltTrainerCmdLine="%MergedPacketsFile:"=%" %AltOutputFile%

echo Executing trainer for producing final dictionary file - commandline:
echo %FinalAltTrainerCmdLine%

@echo on
%TrainerLoc:"=% %FinalAltTrainerCmdLine%
@echo off

if not %errorlevel%==0 goto TrainerError


echo.
echo As the game folder was not detected, you need to manually copy:
echo %AltOutputFile%
echo.
echo To the folder:
echo %GameName:"=%\Content\Oodle
echo.
pause


:TrainerSuccess
echo.
echo Dictionary successfully generated! Set '*Dictionary=Content\Oodle\%OutputFile:"=%' under [OodleHandlerComponent], in *Engine.ini.
echo If you want to switch Oodle from 'Training' mode to 'Release' mode, set 'Mode=Release' in the same location.
goto PostTrainer
echo.


:TrainerError
echo Error generating dictionary file. Review trainer output.
goto End


:PostTrainer
:CommandletCleanup
del %MergedPacketsFile% 



:End
echo Execution complete.
pause


REM Put nothing past here.


