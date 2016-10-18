// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformFile.h"
#elif PLATFORM_PS4
#include "PS4/PS4File.h"
#elif PLATFORM_XBOXONE
#include "XboxOne/XboxOneFile.h"
#elif PLATFORM_MAC
#include "Apple/ApplePlatformFile.h"
#elif PLATFORM_IOS
#include "Apple/ApplePlatformFile.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidFile.h"
#elif PLATFORM_HTML5
//#include "HTML5PlatformFile.h"
#elif PLATFORM_LINUX
#include "Linux/LinuxPlatformFile.h"
#endif
