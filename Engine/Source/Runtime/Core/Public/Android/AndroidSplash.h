// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	AndroidSplash.h: Android platform splash screen...
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSplash.h"

/**
 * Android splash implementation
 */
struct CORE_API FAndroidSplash : public FGenericPlatformSplash
{
	// default implementation for now
};


typedef FAndroidSplash FPlatformSplash;
