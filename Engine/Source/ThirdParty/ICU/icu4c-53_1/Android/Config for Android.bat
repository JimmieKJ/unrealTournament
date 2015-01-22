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
		set PATH=%NDKROOT%\toolchains\llvm-3.3\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.8\prebuilt\windows-x86_64\arm-linux-androideabi\bin\;%PATH%
		bash -c '../../ConfigForAndroid-armv7.sh'
	cd ../

	REM x86 Config
	if not exist ./x86 mkdir x86
	cd ./x86
		set PATH=%NDKROOT%\toolchains\llvm-3.3\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86-4.8\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86-4.8\prebuilt\windows-x86_64\i686-linux-android\bin\;%PATH%
		bash -c '../../ConfigForAndroid-x86.sh'
	cd ../

	REM x64 Config
REM	if not exist ./x64 mkdir x64
REM	cd ./x64
REM		set PATH=%NDKROOT%\toolchains\llvm-3.4\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86_64-4.9\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\x86_64-4.9\prebuilt\windows-x86_64\x86_64-linux-android\bin\;%PATH%
REM		bash -c '../../ConfigForAndroid-x64.sh'
REM	cd ../		

REM Back to root
cd ../
endlocal