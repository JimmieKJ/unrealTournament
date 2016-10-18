// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformAtomics.h"
#elif PLATFORM_PS4
#include "PS4/PS4Atomics.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneAtomics.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformAtomics.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformAtomics.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidAtomics.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5PlatformAtomics.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformAtomics.h"
#endif