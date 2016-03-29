// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "CookStats.h"

#if ENABLE_COOK_STATS
CORE_API FCookStatsManager::FGatherCookStatsDelegate FCookStatsManager::CookStatsCallbacks;

CORE_API void FCookStatsManager::LogCookStats(AddStatFuncRef AddStat)
{
	CookStatsCallbacks.Broadcast(AddStat);
}
#endif