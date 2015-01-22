// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjVer.cpp: Unreal version definitions.
=============================================================================*/

#include "CorePrivatePCH.h"

// Defined separately so the build script can get to it easily (DO NOT CHANGE THIS MANUALLY)
// caution when merging, we want the UE3 version in ENGINE_VERSION_UE3
#define	ENGINE_VERSION	0

/** Version numbers for networking */
int32   GEngineNetVersion			= ENGINE_VERSION;
int32	GEngineMinNetVersion		= 7038;
int32	GEngineNegotiationVersion	= 3077;

// @see ObjectVersion.h for the list of changes/defines
int32	GPackageFileUE4Version = VER_LATEST_ENGINE_UE4;
int32	GPackageFileLicenseeUE4Version = VER_LATEST_ENGINE_LICENSEEUE4;
