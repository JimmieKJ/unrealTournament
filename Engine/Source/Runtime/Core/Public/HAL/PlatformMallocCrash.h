// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_PS4
#include "PS4/PS4MallocCrash.h"
#else
#include "GenericPlatform/GenericPlatformMallocCrash.h"
typedef FGenericPlatformMallocCrash FPlatformMallocCrash;
#endif