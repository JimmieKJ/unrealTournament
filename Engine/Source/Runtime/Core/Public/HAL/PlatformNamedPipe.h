// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformNamedPipe.h"
#else
#include "GenericPlatform/GenericPlatformNamedPipe.h"
#endif