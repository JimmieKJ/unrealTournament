@ECHO OFF
if not "%1" == "" GOTO USECOMMANDLINE

:USERINPUT
ECHO Zip up a release editor build.
set /p BUILDCL=Enter the CL of the release editor build you want to package:
GOTO ZIPUP

:USECOMMANDLINE
set BUILDCL=%1
GOTO ZIPUP

:ZIPUP
pushd P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%BUILDCL%

"C:\Program Files\7-Zip\7z" a UnrealTournament-Editor-XAN-%BUILDCL%-Win64.zip WindowsEditor

popd