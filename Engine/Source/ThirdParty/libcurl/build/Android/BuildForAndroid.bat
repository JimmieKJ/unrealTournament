@echo off
REM Batch file for building Android platforms of OpenSSL
REM Run this from the ICU directory

setlocal
set CYGWIN=winsymlinks:native
set ANDROID_NDK_ROOT=%NDKROOT%
set SSL_VER=openssl-1.0.1g
set CURL_VER=curl-7.34.0

set ANDROID_NDK=android-ndk-r9d
set ANDROID_EABI=arm-linux-androideabi-4.8
set ANDROID_ARCH=arch-arm
set ANDROID_API=android-19
set ANDROID_GCC_VER=4.8
set PATH=%NDKROOT%/toolchains/llvm-3.3/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/arm-linux-androideabi-4.8/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/arm-linux-androideabi-4.8/prebuilt/windows-x86_64/arm-linux-androideabi/bin/;%PATH%
bash -c './BuildForAndroid.sh'

set ANDROID_NDK=android-ndk-r9d
set ANDROID_EABI=x86-4.8
set ANDROID_ARCH=arch-x86
set ANDROID_API=android-19
set ANDROID_GCC_VER=4.8
set PATH=%NDKROOT%/toolchains/llvm-3.3/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86-4.8/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86-4.8/prebuilt/windows-x86_64/i686-linux-android/bin/;%PATH%
bash -c './BuildForAndroid.sh'

set ANDROID_NDK=android-ndk-r10e
set ANDROID_EABI=x86_64-4.9
set ANDROID_ARCH=arch-x86_64
set ANDROID_API=android-21
set ANDROID_GCC_VER=4.9
set PATH=%NDKROOT%/toolchains/llvm-3.6/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86_64-4.9/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86_64-4.9/prebuilt/windows-x86_64/x86_64-linux-android/bin/;%PATH%
bash -c './BuildForAndroid.sh'

endlocal