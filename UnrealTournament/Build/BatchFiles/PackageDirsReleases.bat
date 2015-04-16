xcopy RunUT-Test.bat P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%1\Win64\WindowsNoEditor
xcopy RunUT-Test32.bat P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%1\Win32\WindowsNoEditor

pushd P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%1

"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%1-Linux.zip LinuxServer
"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%1-Win64.zip WindowsServer

cd Win64
"C:\Program Files\7-Zip\7z" a ..\UnrealTournament-Client-XAN-%1-Win64.zip WindowsNoEditor
cd ..

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%1-Linux.zip LinuxNoEditor
"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%1-Mac.zip MacNoEditor

cd Win32
"C:\Program Files\7-Zip\7z" a ..\UnrealTournament-Client-XAN-%1-Win32.zip WindowsNoEditor
cd ..

popd