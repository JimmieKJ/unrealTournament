// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	WinRTIncludes.h: Includes the platform specific headers for WinRT
==================================================================================*/

#pragma once

// Set up compiler pragmas, etc
#include "WinRT/WinRTCompilerSetup.h"

// include platform implementations
#include "WinRT/WinRTMemory.h"
#include "WinRT/WinRTString.h"
#include "WinRT/WinRTMisc.h"
#include "WinRT/WinRTStackWalk.h"
#include "WinRT/WinRTMath.h"
#include "WinRT/WinRTTime.h"
#include "WinRT/WinRTProcess.h"
#include "WinRT/WinRTOutputDevices.h"
#include "WinRT/WinRTAtomics.h"
#include "WinRT/WinRTTLS.h"
#include "WinRT/WinRTSplash.h"
#include "WinRT/WinRTSurvey.h"
#include "WinRT/WinRTHttp.h"

typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;

typedef FGenericPlatformAffinity FPlatformAffinity;

// include platform properties and typedef it for the runtime
#include "WinRTProperties.h"
typedef FWinRTPlatformProperties FPlatformProperties;
