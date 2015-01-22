// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	AndroidIncludes.h: Includes the platform specific headers for Android
==================================================================================*/

#pragma once
#include "Android/AndroidSystemIncludes.h"

// include platform implementations
#include "Android/AndroidMemory.h"
#include "Android/AndroidString.h"
#include "Android/AndroidMisc.h"
#include "Android/AndroidPlatformStackWalk.h"
#include "Android/AndroidMath.h"
#include "Android/AndroidTime.h"
#include "Android/AndroidProcess.h"
#include "Android/AndroidOutputDevices.h"
#include "Android/AndroidAtomics.h"
#include "Android/AndroidTLS.h"
#include "Android/AndroidSplash.h"
#include "Android/AndroidFile.h"
#include "Android/AndroidSurvey.h"
#include "Android/AndroidHttp.h"
#include "Android/AndroidAffinity.h"

// include platform properties and typedef it for the runtime
#include "AndroidProperties.h"
typedef FAndroidPlatformProperties FPlatformProperties;
