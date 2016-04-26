// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusInput.h"

#if USE_OVR_MOTION_SDK

#if PLATFORM_WINDOWS
	// Required for OVR_CAPIShim.c
	#include "AllowWindowsPlatformTypes.h"
#endif

#if !IS_MONOLITHIC // otherwise it will clash with OculusRift
#include <OVR_CAPIShim.c>
#endif // #if !IS_MONOLITHIC

#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformTypes.h"
#endif

#endif	// USE_OVR_MOTION_SDK