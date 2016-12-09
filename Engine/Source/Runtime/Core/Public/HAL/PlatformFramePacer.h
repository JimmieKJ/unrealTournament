// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_IOS
#include "IOS/IOSPlatformFramePacer.h"
#else
#include "GenericPlatform/GenericPlatformFramePacer.h"
typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;
#endif
