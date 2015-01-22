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
		set PATH=%NDKROOT%\toolchains\llvm-3.3\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\arm-linux-androideabi\bin\;%PATH%	
		bash -c 'make clean'
		bash -c 'make all'
	cd ./data
		bash -c 'make'
	cd ../../
	
	REM x86 Make
	if not exist ./x86 (
		echo Error: x86 directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./x86
		set PATH=%NDKROOT%\toolchains\llvm-3.3\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86-4.8\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86-4.8\prebuilt\windows-x86_64\i686-linux-android\bin\;%PATH%
		bash -c 'make clean'
		bash -c 'make all'
	cd ./data
		bash -c 'make'
	cd ../../

	REM x64 Make
REM	if not exist ./x64 (
REM		echo Error: x64 directory does not exist. Did you forget to run configuration?
REM		goto:eof)
REM	cd ./x64
REM		set PATH=%NDKROOT%\toolchains\llvm-3.4\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86_64-4.9\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86_64-4.9\prebuilt\windows-x86_64\x86_64-linux-android\bin\;%PATH%
REM		bash -c 'make clean'
REM		bash -c 'make all'
REM	cd ./data
REM		bash -c 'make'
REM	cd ../../
	
REM Back to root
cd ../
endlocal