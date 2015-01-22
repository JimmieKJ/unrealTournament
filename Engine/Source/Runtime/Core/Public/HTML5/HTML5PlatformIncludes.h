// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	HTML5Includes.h: Includes the platform specific headers for HTML5
==================================================================================*/

#pragma once
#include "HTML5/HTML5SystemIncludes.h"

// include platform implementations
#include "HTML5/HTML5PlatformMemory.h"
#include "HTML5/HTML5PlatformString.h"
#include "HTML5/HTML5PlatformMisc.h"
#include "HTML5/HTML5PlatformStackWalk.h"
#include "HTML5/HTML5PlatformMath.h"
#include "HTML5/HTML5PlatformTime.h"
#include "HTML5/HTML5PlatformProcess.h"
#include "HTML5/HTML5PlatformOutputDevices.h"
#include "HTML5/HTML5PlatformAtomics.h"
#include "HTML5/HTML5PlatformTLS.h"
#include "HTML5/HTML5PlatformSplash.h"
#include "HTML5/HTML5PlatformSurvey.h"
#include "HTML5/HTML5PlatformHttp.h"

typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;

typedef FGenericPlatformAffinity FPlatformAffinity;

// include platform properties and typedef it for the runtime
#include "HTML5PlatformProperties.h"
typedef FHTML5PlatformProperties FPlatformProperties;


