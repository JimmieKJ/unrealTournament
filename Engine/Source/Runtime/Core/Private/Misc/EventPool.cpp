// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "CorePrivatePCH.h"
#include "EventPool.h"

TStatId FEventStats::CreateStatID()
{
#if	STATS
	static FThreadSafeCounter UniqueId;
	const int32 PreviousId = UniqueId.Add(1);
	const FString UniqueEventName = FString::Printf( TEXT("CPU Stall - Wait for event %3i"), PreviousId );

	const FName StatName = FName( *UniqueEventName );
	FStartupMessages::Get().AddMetadata( StatName, *UniqueEventName, 
		STAT_GROUP_TO_FStatGroup( STATGROUP_CPUStalls )::GetGroupName(), 
		STAT_GROUP_TO_FStatGroup( STATGROUP_CPUStalls )::GetGroupCategory(), 
		STAT_GROUP_TO_FStatGroup( STATGROUP_CPUStalls )::GetDescription(),
		true, EStatDataType::ST_int64, true );

	return IStatGroupEnableManager::Get().GetHighPerformanceEnableForStat(StatName, 
		STAT_GROUP_TO_FStatGroup( STATGROUP_CPUStalls )::GetGroupName(), 
		STAT_GROUP_TO_FStatGroup( STATGROUP_CPUStalls )::GetGroupCategory(), 
		STAT_GROUP_TO_FStatGroup( STATGROUP_CPUStalls )::DefaultEnable,
		true, EStatDataType::ST_int64, *UniqueEventName, true);
#else
	return TStatId();
#endif // STATS
}
