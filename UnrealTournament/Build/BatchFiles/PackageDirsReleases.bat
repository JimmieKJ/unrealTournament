xcopy RunUT-Shipping.bat P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%1\WindowsNoEditor
xcopy RunUT-Shipping32.bat P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%1\WindowsNoEditor

pushd P:\Builds\UnrealTournament\++depot+UE4-UT-Releases-CL-%1

"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%1-Linux.zip LinuxServer
"C:\Program Files\7-Zip\7z" a UnrealTournament-Server-XAN-%1-Win64.zip WindowsServer

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%1-Win.zip WindowsNoEditor

"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%1-Linux.zip LinuxNoEditor
"C:\Program Files\7-Zip\7z" a UnrealTournament-Client-XAN-%1-Mac.zip MacNoEditor


popd