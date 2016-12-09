// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "OculusInput.h"

#if OCULUS_INPUT_SUPPORTED_PLATFORMS

#if PLATFORM_WINDOWS
	// Required for OVR_CAPIShim.c
	#include "WindowsHWrapper.h"
	#include "AllowWindowsPlatformTypes.h"
#endif

#if !IS_MONOLITHIC // otherwise it will clash with OculusRift
#include <OVR_CAPIShim.c>
#include <OVR_CAPI_Util.cpp>
#include <OVR_StereoProjection.cpp>
#endif // #if !IS_MONOLITHIC

#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformTypes.h"
#endif

#endif	// OCULUS_INPUT_SUPPORTED_PLATFORMS