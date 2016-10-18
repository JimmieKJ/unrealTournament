// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "OnlineSubsystemSteamModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "SocketSubsystem.h"

#include "ModuleManager.h"

#define INVALID_INDEX -1

/** Compile Steam SDK version in use */
#define STEAM_SDK_VER TEXT("1.32")
/** Path of the current Steam SDK version in use */
#define STEAM_SDK_VER_PATH TEXT("Steamv132")
/** Root location of Steam SDK */
#define STEAM_SDK_ROOT_PATH TEXT("Binaries/ThirdParty/Steamworks")

/** URL Prefix when using Steam socket connection */
#define STEAM_URL_PREFIX TEXT("steam.")
/** Filename containing the appid during development */
#define STEAMAPPIDFILENAME TEXT("steam_appid.txt")

/** pre-pended to all steam logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("STEAM: ")

// @todo Steam: Steam headers trigger secure-C-runtime warnings in Visual C++. Rather than mess with _CRT_SECURE_NO_WARNINGS, we'll just
//	disable the warnings locally. Remove when this is fixed in the SDK
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#pragma push_macro("ARRAY_COUNT")
#undef ARRAY_COUNT

// Steamworks SDK headers
#if STEAMSDK_FOUND == 0
#error Steam SDK not located.  Expected to be found in Engine/Source/ThirdParty/Steamworks/{SteamVersion}
#endif

#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"

#pragma pop_macro("ARRAY_COUNT")

// @todo Steam: See above
#ifdef _MSC_VER
#pragma warning(pop)
#endif

