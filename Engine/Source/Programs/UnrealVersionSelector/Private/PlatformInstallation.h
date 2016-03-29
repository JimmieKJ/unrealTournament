// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealVersionSelector.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsPlatformInstallation.h"
	typedef FWindowsPlatformInstallation FPlatformInstallation;
#else
	#include "GenericPlatform/GenericPlatformInstallation.h"
	typedef FGenericPlatformInstallation FPlatformInstallation;
#endif
