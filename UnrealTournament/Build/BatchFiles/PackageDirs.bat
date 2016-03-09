@ECHO OFF
if not "%1" == "" GOTO USECOMMANDLINE

:USERINPUT
ECHO Zip up a build.
set /p BUILDCL=Enter the CL of the release build you want to package:
GOTO ZIPUP

:USECOMMANDLINE
set BUILDCL=%1
GOTO ZIPUP

:ZIPUP
xcopy RunUT-Shipping.bat P:\Builds\UnrealTournament\++depot+UE4-UT-CL-%BUILDCL%\WindowsNoEditor
xcopy RunUT-Shipping32.bat P:\Builds\UnrealTournament\++depot+UE4-UT-CL-%BUILDCL%\WindowsNoEditor
xcopy RunUT-Test.bat P:\Builds\UnrealTournament\++depot+UE4-UT-CL-%BUILDCL%\WindowsNoEditor
xcopy RunUT-Test32.bat P:\Builds\UnrealTournament\++depot+UE4-UT-CL-%BUILDCL%\WindowsNoEditor

pushd P:\Builds\UnrealTournament\++depot+UE4-UT-CL-%BUILDCL%

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%BUILDCL%-Win.zip WindowsNoEditor

"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%BUILDCL%-Linux.zip LinuxServer
"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%BUILDCL%-Win64.zip WindowsServer

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%BUILDCL%-Linux.zip LinuxNoEditor
"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%BUILDCL%-Mac.zip MacNoEditor

popd