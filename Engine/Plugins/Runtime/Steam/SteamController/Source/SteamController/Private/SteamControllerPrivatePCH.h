// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "ISteamControllerPlugin.h"

#if WITH_STEAM_CONTROLLER

#include "InputDevice.h"

// Disable crazy warnings that claim that standard C library is "deprecated".
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#pragma push_macro("ARRAY_COUNT")
#undef ARRAY_COUNT

#if STEAMSDK_FOUND == 0
#error Steam SDK not located.  Expected to be found in Engine/Source/ThirdParty/Steamworks/{SteamVersion}
#endif
#include <steam/steam_api.h>

#pragma pop_macro("ARRAY_COUNT")

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // WITH_STEAM_CONTROLLER

