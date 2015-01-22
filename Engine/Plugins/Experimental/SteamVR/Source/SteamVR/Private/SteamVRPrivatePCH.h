// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Name of the current Steam SDK version in use (matches directory name) */
#define STEAM_SDK_VER TEXT("Steamv130")

#include "Engine.h"
#include "IHeadMountedDisplay.h"

// @todo Steam: Steam headers trigger secure-C-runtime warnings in Visual C++. Rather than mess with _CRT_SECURE_NO_WARNINGS, we'll just
//	disable the warnings locally. Remove when this is fixed in the SDK
#if WITH_STEAMWORKS

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#include "steam/steam_api.h"
#include "steam/steamvr.h"

// @todo Steam: See above
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // WITH_STEAMWORKS