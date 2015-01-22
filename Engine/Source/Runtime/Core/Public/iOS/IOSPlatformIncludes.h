// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	IOSPlatformIncludes.h: Includes the platform specific headers for iOS
==================================================================================*/

#pragma once
#include "IOS/IOSSystemIncludes.h"

// include platform implementations
#include "IOS/IOSPlatformMemory.h"
#include "Apple/ApplePlatformString.h"
#include "IOS/IOSPlatformMisc.h"
#include "Apple/ApplePlatformStackWalk.h"
#include "IOS/IOSPlatformMath.h"
#include "Apple/ApplePlatformTime.h"
#include "IOS/IOSPlatformProcess.h"
#include "IOS/IOSPlatformOutputDevices.h"
#include "Apple/ApplePlatformAtomics.h"
#include "Apple/ApplePlatformTLS.h"
#include "IOS/IOSPlatformSplash.h"
#include "Apple/ApplePlatformFile.h"
#include "IOS/IOSPlatformSurvey.h"
#include "IOSAsyncTask.h"
#include "IOS/IOSPlatformHttp.h"
#include "IOS/IOSPlatformFramePacer.h"
#import "IOSPlatformString.h"

// include platform properties and typedef it for the runtime

#include "IOS/IOSPlatformProperties.h"

typedef FIOSPlatformProperties FPlatformProperties;
