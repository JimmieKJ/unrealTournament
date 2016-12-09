// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// 
// Engine global defines

#pragma once

#include "CoreMinimal.h"

/** when set, disallows all network travel (pending level rejects all client travel attempts) */
extern ENGINE_API bool						GDisallowNetworkTravel;

/** The GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles(). */
extern ENGINE_API uint32					GGPUFrameTime;
