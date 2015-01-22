@echo off
REM Batch file for configuring Android platforms of ICU
REM Run this from the ICU directory

setlocal
set CYGWIN=winsymlinks:native

REM Android Configs
if not exist ./Android (
	echo Error: Android directory does not exist. Did you forget to run configuration?
	goto:eof)
cd ./Android
	
	REM ARMv7 Make
	if not exist ./ARMv7 (
		echo Error: ARMv7 directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./ARMv7
		set PATH=%PATH%;%NDKROOT%\toolchains\llvm-3.3\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\arm-linux-androideabi\bin\		
		bash -c 'make clean'
		bash -c 'make all'
	cd ../

REM Back to root
cd ../
endlocal