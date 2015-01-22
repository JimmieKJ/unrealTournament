// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include <windows.h>
#include "Public/Modules/ModuleVersion.h"
#include "ModuleVersionResource.h"
#include "CoreMiscDefines.h"


// Neutral language
LANGUAGE LANG_NEUTRAL,SUBLANG_NEUTRAL

// Declare the engine API version string
#if !IS_MONOLITHIC
	ID_MODULE_API_VERSION_RESOURCE RCDATA
	{
		PREPROCESSOR_TO_STRING(MODULE_API_VERSION) 0
	}
#endif