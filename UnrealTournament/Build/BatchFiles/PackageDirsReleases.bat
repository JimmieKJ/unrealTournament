@ECHO OFF
if not "%1" == "" GOTO USECOMMANDLINE

:USERINPUT
ECHO Zip up a release build.
set /p BUILDCL=Enter the CL of the release build you want to package:
GOTO ZIPUP

:USECOMMANDLINE
set BUILDCL=%1
GOTO ZIPUP

:ZIPUP

xcopy RunUT-Shipping.bat \\isi-shares\Builds_UnrealTournament\++depot+UE4-UT-Releases-CL-%BUILDCL%\WindowsNoEditor
xcopy RunUT-Shipping32.bat \\isi-shares\Builds_UnrealTournament\++depot+UE4-UT-Releases-CL-%BUILDCL%\WindowsNoEditor
xcopy RunUT-Test.bat \\isi-shares\Builds_UnrealTournament\++depot+UE4-UT-Releases-CL-%BUILDCL%\WindowsNoEditor
xcopy RunUT-Test32.bat \\isi-shares\Builds_UnrealTournament\++depot+UE4-UT-Releases-CL-%BUILDCL%\WindowsNoEditor

pushd \\isi-shares\Builds_UnrealTournament\++depot+UE4-UT-Releases-CL-%BUILDCL%

"C:\Program Files (x86)\wxChecksums\wxcksums" --create=LinuxServer\%BUILDCL%.sfv LinuxServer\

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%BUILDCL%-Win.zip WindowsNoEditor

"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%BUILDCL%-Linux.zip LinuxServer
"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%BUILDCL%-Win64.zip WindowsServer

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%BUILDCL%-Linux.zip LinuxNoEditor
"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%BUILDCL%-Mac.zip MacNoEditor


popd