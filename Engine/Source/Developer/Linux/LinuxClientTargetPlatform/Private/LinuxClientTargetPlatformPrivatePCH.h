// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxClientTargetPlatformPrivatePCH.h: Pre-compiled header file for the LinuxClientTargetPlatform module.
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
