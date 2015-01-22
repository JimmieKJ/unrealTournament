// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowClientTargetPlatformPrivatePCH.h: Pre-compiled header file for the WindowsClientTargetPlatform module.
=============================================================================*/

#pragma once


/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleManager.h"
#include "WindowsPlatformProperties.h"

#if WITH_ENGINE
	#include "Engine.h"
	#include "TextureCompressorModule.h"
#endif

#include "TargetPlatform.h"
#include "TargetPlatformBase.h"

#include "AllowWindowsPlatformTypes.h"
	#include <TlHelp32.h>
#include "HideWindowsPlatformTypes.h"


/* Private includes
 *****************************************************************************/

#include "LocalPcTargetDevice.h"
#include "GenericWindowsTargetPlatform.h"
