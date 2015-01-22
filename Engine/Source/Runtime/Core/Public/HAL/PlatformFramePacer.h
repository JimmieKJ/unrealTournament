// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HAL/Platform.h"

#if PLATFORM_IOS
#include "IOS/IOSPlatformFramePacer.h"
#else
#include "GenericPlatform/GenericPlatformFramePacer.h"
typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;
#endif