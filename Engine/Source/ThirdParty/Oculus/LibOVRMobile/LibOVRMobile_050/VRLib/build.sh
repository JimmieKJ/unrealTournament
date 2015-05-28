#!/bin/sh

export BUILD_MODULE=VRLib

echo "========================== Update "${BUILD_MODULE}" Project ==========================="
android update project -t android-19 -p . -s

if [ -z "$ANDROID_NDK" ]; then
  ANDROID_NDK=~/ndk
fi

#We never pass NDK_DEBUG=1 to vrlib as this generates a duplicate gdbserver
#instead the app using vrlib can set it 

if [ "$1" == "" ]; then
    echo "========================== Build "${BUILD_MODULE}" ==========================="
    $ANDROID_NDK/ndk-build -j16 NDK_DEBUG=0 OVR_DEBUG=1 
fi

if [ "$1" == "debug" ]; then
    echo "========================== Build "${BUILD_MODULE} $1 " ==========================="
    $ANDROID_NDK/ndk-build -j16 NDK_DEBUG=0 OVR_DEBUG=1 
fi

if [ "$1" == "release" ]; then
    echo "========================== Build "${BUILD_MODULE} $1 " ==========================="
    $ANDROID_NDK/ndk-build -j16 NDK_DEBUG=0 OVR_DEBUG=0 
fi

if [ "$1" == "clean" ]; then
    echo "========================== Build "${BUILD_MODULE} $1 " ==========================="
    $ANDROID_NDK/ndk-build clean NDK_DEBUG=1 
    $ANDROID_NDK/ndk-build clean NDK_DEBUG=0
    ant clean 
fi