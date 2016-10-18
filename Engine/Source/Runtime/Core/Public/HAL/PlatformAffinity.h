// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

#if PLATFORM_XBOXONE
#include "XboxOne/XBoxOneAffinity.h"
#elif PLATFORM_PS4
#include "PS4/PS4Affinity.h"
#elif PLATFORM_IOS
#include "IOS/IOSPlatformAffinity.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidAffinity.h"
#else
#include "GenericPlatform/GenericPlatformAffinity.h"
typedef FGenericPlatformAffinity FPlatformAffinity;
#endif