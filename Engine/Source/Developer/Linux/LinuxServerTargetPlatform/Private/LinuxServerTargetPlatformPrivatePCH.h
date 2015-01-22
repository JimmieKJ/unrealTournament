// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxServerTargetPlatformPrivatePCH.h: Pre-compiled header file for the AndroidTargetPlatform module.
=============================================================================*/

#pragma once


/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleManager.h"
#include "Runtime/Core/Public/Linux/LinuxPlatformProperties.h"

#if WITH_ENGINE
	#include "Engine.h"
	#include "TextureCompressorModule.h"
#endif

#include "TargetPlatform.h"
#include "TargetPlatformBase.h"


/* Private includes
 *****************************************************************************/

#include "LinuxTargetDevice.h"
#include "LinuxTargetPlatform.h"
