@echo off
REM Batch file for configuring Android platforms of ICU
REM Run this from the ICU directory

setlocal
set CYGWIN=winsymlinks:native

REM Android Configs
if not exist ./Android mkdir Android
cd ./Android

	REM ARMv7 Config
	if not exist ./ARMv7 mkdir ARMv7
	cd ./ARMv7
		set PATH=%PATH%;%NDKROOT%\toolchains\llvm-3.3\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\arm-linux-androideabi\bin\
		bash -c '../../ConfigForAndroid-Debug.sh'
	cd ../
	
REM Back to root
cd ../
endlocal