// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusRiftPrivatePCH.h"
#include "OculusRiftHMD.h"

#if PLATFORM_WINDOWS
	// Required for OVR_CAPIShim.c
	#include "AllowWindowsPlatformTypes.h"
#endif

#include <OVR_CAPIShim.c>
#include <OVR_CAPI_Util.cpp>
#include <OVR_StereoProjection.cpp>

#if PLATFORM_WINDOWS
	// It is required to undef WINDOWS_PLATFORM_TYPES_GUARD for any further D3D11 private includes
	#include "HideWindowsPlatformTypes.h"
#endif

#include <HeadMountedDisplayCommon.cpp>
#include <AsyncLoadingSplash.cpp>
#include <OculusStressTests.cpp>
