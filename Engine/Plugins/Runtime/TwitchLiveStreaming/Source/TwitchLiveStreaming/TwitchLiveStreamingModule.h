// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#ifndef WITH_TWITCH
	#error Expecting WITH_TWITCH to be defined to either 0 or 1 in TwitchLiveStreaming.Build.cs
	#define WITH_TWITCH 1	// For IntelliSense to be happy
#endif