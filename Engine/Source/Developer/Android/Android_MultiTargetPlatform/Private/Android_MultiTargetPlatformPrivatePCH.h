// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================
	Android_MultiTargetPlatformPrivatePCH.h: Pre-compiled header file for the Android_MultiTargetPlatform module.
=============================================================================*/

#pragma once

/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Runtime/Core/Public/Android/AndroidProperties.h"

#if WITH_ENGINE
	#include "Engine.h"
#endif

#include "PlatformInfo.h"
#include "TargetPlatform.h"
#include "TargetPlatformBase.h"
#include "AndroidDeviceDetection.h"
#include "IAndroid_MultiTargetPlatformModule.h"

/* Private includes
 *****************************************************************************/

#include "AndroidTargetDevice.h"
#include "AndroidTargetPlatform.h"
