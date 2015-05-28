// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "TaskGraphInterfaces.h"
#include "DefaultValueHelper.h"

#if STATS

#include "StatsData.h"
#include "StatsFile.h"
#include "StatsMallocProfilerProxy.h"

DECLARE_CYCLE_STAT(TEXT("Hitch Scan"),STAT_HitchScan,STATGROUP_StatSystem);
DECLARE_CYCLE_STAT(TEXT("HUD Group"),STAT_HUDGroup,STATGROUP_StatSystem);

DECLARE_CYCLE_STAT(TEXT("Accumulate"),STAT_Accumulate,STATGROUP_StatSystem);
DECLARE_CYCLE_STAT(TEXT("GetFlatAggregates"),STAT_GetFlatAggregates,STATGROUP_StatSystem);

void FromString( EStatCompareBy::Type& OutValue, const TCHAR* Buffer )
{
	OutValue = EStatCompareBy::Sum;

	if (FCString::Stricmp(Buffer, TEXT("CallCount")) == 0)
	{
		OutValue = EStatCompareBy::CallCount;
	}
	else if (FCString::Stricmp(Buffer, TEXT("Name")) == 0)
	{
		OutValue = EStatCompareBy::Name;
	}
}

struct FGroupFilter : public IItemFiler
{
	TSet<FName> const& EnabledItems;
	FGroupFilter( TSet<FName> const& InEnabledItems )
		: EnabledItems( InEnabledItems )
	{}
	virtual bool Keep( FStatMessage const& Item )
	{
		return EnabledItems.Contains( Item.NameAndInfo.GetRawName() );
	}
};

/** Holds parameters used by the 'stat hier' or 'stat group ##' command. */
struct FStatGroupParams
{
	/** Default constructor. */
	FStatGroupParams( const TCHAR* Cmd = nullptr )
		: Group( Cmd, TEXT("group="), NAME_None )
		, SortBy( Cmd, TEXT("sortby="), EStatCompareBy::Sum )
		, MaxHistoryFrames( Cmd, TEXT("maxhistoryframes="), 60 )
		, MaxHierarchyDepth( Cmd, TEXT("maxdepth="), 16 )
		, bReset( FCString::Stristr( Cmd, TEXT("-reset") ) != nullptr )
	{}

	/**
	 * @return whether we should run stat hier reset command.
	 */
	bool ShouldReset() const
	{
		return bReset;
	}

	/** -group=[name]. */
	TParsedValueWithDefault<FName> Group;

	/** -sortby=[name|callcount|sum]. */
	TParsedValueWithDefault<EStatCompareBy::Type> SortBy;

	/**
	 *	Maximum number of frames to be included in the history. 
	 *	-maxhistoryframes=[20:20-120]
	 */
	// @TODO yrx 2014-08-21 Replace with TParsedValueWithDefaultAndRange
	TParsedValueWithDefault<int32> MaxHistoryFrames;

	/**
	 *	Maximum depth for the hierarchy
	 * -maxdepth=16
	 */
	TParsedValueWithDefault<int32> MaxHierarchyDepth;

	/** Whether to reset all collected data. */
	bool bReset;
};

void DumpHistoryFrame(FStatsThreadState const& StatsData, int64 TargetFrame, float DumpCull = 0.0f, int32 MaxDepth = MAX_int32, TCHAR const* Filter = NULL)
{
	UE_LOG(LogStats, Log, TEXT("Single Frame %lld ---------------------------------"), TargetFrame);
	if (DumpCull == 0.0f)
	{
		UE_LOG(LogStats, Log, TEXT("Full data, use -ms=5, for example to show just the stack data with a 5ms threshhold."));
	}
	else
	{
		UE_LOG(LogStats, Log, TEXT("Culled to %fms, use -ms=0, for all data and aggregates."), DumpCull);
	}
	{
		UE_LOG(LogStats, Log, TEXT("Stack ---------------"));
		FRawStatStackNode Stack;
		StatsData.UncondenseStackStats(TargetFrame, Stack);
		if (DumpCull != 0.0f)
		{
			Stack.Cull(int64(DumpCull / FPlatformTime::ToMilliseconds(1.0f)));
		}
		Stack.AddNameHierarchy();
		Stack.AddSelf();
		Stack.DebugPrint(Filter, MaxDepth);
	}
	if (DumpCull == 0.0f)
	{
		UE_LOG(LogStats, Log, TEXT("Inclusive aggregate stack data---------------"));
		TArray<FStatMessage> Stats;
		StatsData.GetInclusiveAggregateStackStats(TargetFrame, Stats);
		Stats.Sort(FGroupSort());
		FName LastGroup = NAME_None;
		for (int32 Index = 0; Index < Stats.Num(); Index++)
		{
			FStatMessage const& Meta = Stats[Index];
			if (LastGroup != Meta.NameAndInfo.GetGroupName())
			{
				LastGroup = Meta.NameAndInfo.GetGroupName();
				UE_LOG(LogStats, Log, TEXT("%s"), *LastGroup.ToString());
			}
			UE_LOG(LogStats, Log, TEXT("  %s"), *FStatsUtils::DebugPrint(Meta));
		}

		UE_LOG(LogStats, Log, TEXT("Exclusive aggregate stack data---------------"));
		Stats.Empty();
		StatsData.GetExclusiveAggregateStackStats(TargetFrame, Stats);
		Stats.Sort(FGroupSort());
		LastGroup = NAME_None;
		for (int32 Index = 0; Index < Stats.Num(); Index++)
		{
			FStatMessage const& Meta = Stats[Index];
			if (LastGroup != Meta.NameAndInfo.GetGroupName())
			{
				LastGroup = Meta.NameAndInfo.GetGroupName();
				UE_LOG(LogStats, Log, TEXT("%s"), *LastGroup.ToString());
			}
			UE_LOG(LogStats, Log, TEXT("  %s"), *FStatsUtils::DebugPrint(Meta));
		}
	}
}

void DumpNonFrame(FStatsThreadState const& StatsData)
{
	UE_LOG(LogStats, Log, TEXT("Full non-frame data ---------------------------------"));

	TArray<FStatMessage> Stats;
	for (auto It = StatsData.NotClearedEveryFrame.CreateConstIterator(); It; ++It)
	{
		Stats.Add(It.Value());
	}
	Stats.Sort(FGroupSort());
	FName LastGroup = NAME_None;
	for (int32 Index = 0; Index < Stats.Num(); Index++)
	{
		FStatMessage const& Meta = Stats[Index];
		if (LastGroup != Meta.NameAndInfo.GetGroupName())
		{
			LastGroup = Meta.NameAndInfo.GetGroupName();
			UE_LOG(LogStats, Log, TEXT("%s"), *LastGroup.ToString());
		}
		UE_LOG(LogStats, Log, TEXT("  %s"), *FStatsUtils::DebugPrint(Meta));
	}
}

void DumpCPUSummary(FStatsThreadState const& StatsData, int64 TargetFrame)
{
	UE_LOG(LogStats, Log, TEXT("CPU Summary: Single Frame %lld ---------------------------------"), TargetFrame);

	struct FTimeInfo
	{
		int32 StartCalls;
		int32 StopCalls;
		int32 Recursion;
		FTimeInfo()
			: StartCalls(0)
			, StopCalls(0)
			, Recursion(0)
		{

		}
	};
	TMap<FName, TMap<FName, FStatMessage> > StallsPerThreads;
	TMap<FName, FTimeInfo> Timing;
	TMap<FName, FStatMessage> ThisFrameMetaData;
	TArray<FStatMessage> const& Data = StatsData.GetCondensedHistory(TargetFrame);

	static FName NAME_STATGROUP_CPUStalls("STATGROUP_CPUStalls");
	static FName Total("Total");

	int32 Level = 0;
	FName LastThread;
	for (int32 Index = 0; Index < Data.Num(); Index++)
	{
		FStatMessage const& Item = Data[Index];
		FName LongName = Item.NameAndInfo.GetRawName();

		// The description of a thread group contains the thread name marker
		const FString Desc = Item.NameAndInfo.GetDescription();
		bool bIsThread = Desc.StartsWith( FStatConstants::ThreadNameMarker );
		bool bIsStall = !bIsThread && Desc.StartsWith("CPU Stall");
		
		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		if ((Op == EStatOperation::ChildrenStart || Op == EStatOperation::ChildrenEnd ||  Op == EStatOperation::Leaf) && Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
		{

			FTimeInfo& ItemTime = Timing.FindOrAdd(LongName);
			if (Op == EStatOperation::ChildrenStart)
			{
				ItemTime.StartCalls++;
				ItemTime.Recursion++;
				Level++;
				if (bIsThread)
				{
					LastThread = LongName;
				}
			}
			else
			{
				if (Op == EStatOperation::ChildrenEnd)
				{
					ItemTime.StopCalls++;
					ItemTime.Recursion--;
					Level--;
					if (bIsThread)
					{
						{
							FStatMessage* Result = ThisFrameMetaData.Find(LongName);
							if (!Result)
							{
								Result = &ThisFrameMetaData.Add(LongName, Item);
								Result->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
								Result->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
								Result->Clear();
							}
							FStatsUtils::AccumulateStat(*Result, Item, EStatOperation::Add);
						}
						{
							FStatMessage* TotalResult = ThisFrameMetaData.Find(Total);
							if (!TotalResult)
							{
								TotalResult = &ThisFrameMetaData.Add(Total, Item);
								TotalResult->NameAndInfo.SetRawName(Total);
								TotalResult->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
								TotalResult->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
								TotalResult->Clear();
							}
							FStatsUtils::AccumulateStat(*TotalResult, Item, EStatOperation::Add, true);
						}
						LastThread = NAME_None;
					}
				}
				check(!bIsStall || (!ItemTime.Recursion && LastThread != NAME_None));
				if (!ItemTime.Recursion) // doing aggregates here, so ignore misleading recursion which would be counted twice
				{
					if (LastThread != NAME_None && bIsStall)
					{
						{
							TMap<FName, FStatMessage>& ThreadStats = StallsPerThreads.FindOrAdd(LastThread);
							FStatMessage* ThreadResult = ThreadStats.Find(LongName);
							if (!ThreadResult)
							{
								ThreadResult = &ThreadStats.Add(LongName, Item);
								ThreadResult->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
								ThreadResult->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
								ThreadResult->Clear();
							}
							FStatsUtils::AccumulateStat(*ThreadResult, Item, EStatOperation::Add);
						}
						{
							FStatMessage* Result = ThisFrameMetaData.Find(LastThread);
							if (!Result)
							{
								Result = &ThisFrameMetaData.Add(LastThread, Item);
								Result->NameAndInfo.SetRawName(LastThread);
								Result->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
								Result->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
								Result->Clear();
							}
							FStatsUtils::AccumulateStat(*Result, Item, EStatOperation::Subtract, true);
						}
						{
							FStatMessage* TotalResult = ThisFrameMetaData.Find(Total);
							if (!TotalResult)
							{
								TotalResult = &ThisFrameMetaData.Add(Total, Item);
								TotalResult->NameAndInfo.SetRawName(Total);
								TotalResult->NameAndInfo.SetField<EStatOperation>(EStatOperation::Set);
								TotalResult->NameAndInfo.SetFlag(EStatMetaFlags::IsPackedCCAndDuration, true);
								TotalResult->Clear();
							}
							FStatsUtils::AccumulateStat(*TotalResult, Item, EStatOperation::Subtract, true);
						}
					}
				}
			}
		}
	}

	const FStatMessage* TotalStat = NULL;
	for (TMap<FName, FStatMessage>::TConstIterator ThreadIt(ThisFrameMetaData); ThreadIt; ++ThreadIt)
	{
		const FStatMessage& Item = ThreadIt.Value();
		if (Item.NameAndInfo.GetRawName() == Total)
		{
			TotalStat = &Item; 
		}
		else
		{
			UE_LOG(LogStats, Log, TEXT("%s%s"), FCString::Spc(2), *FStatsUtils::DebugPrint(Item));
			TMap<FName, FStatMessage>& ThreadStats = StallsPerThreads.FindOrAdd(ThreadIt.Key());
			for (TMap<FName, FStatMessage>::TConstIterator ItStall(ThreadStats); ItStall; ++ItStall)
			{
				const FStatMessage& Stall = ItStall.Value();
				UE_LOG(LogStats, Log, TEXT("%s%s"), FCString::Spc(4), *FStatsUtils::DebugPrint(Stall));
			}
		}
	}
	if (TotalStat)
	{
		UE_LOG(LogStats, Log, TEXT("----------------------------------------"));
		UE_LOG(LogStats, Log, TEXT("%s%s"), FCString::Spc(2), *FStatsUtils::DebugPrint(*TotalStat));
	}
}

static int32 HitchIndex = 0;
static float TotalHitchTime = 0.0f;

static void DumpHitch(int64 Frame)
{
	// !!!CAUTION!!! 
	// Due to chain reaction of hitch reports after detecting the first hitch, the hitch detector is disabled for the next 4 frames.
	// There is no other safe method to detect if the next hitch is a real hitch or just waiting for flushing the threaded logs or waiting for the stats. 
	// So, the best way is to just wait until stats gets synchronized with the game thread.

	static int64 LastHitchFrame = -(MAX_STAT_LAG + STAT_FRAME_SLOP);
	if( LastHitchFrame + (MAX_STAT_LAG + STAT_FRAME_SLOP) > Frame )
	{
		return;
	}

	FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	SCOPE_CYCLE_COUNTER(STAT_HitchScan);

	const float GameThreadTime = FPlatformTime::ToSeconds(Stats.GetFastThreadFrameTime(Frame, EThreadType::Game));
	const float RenderThreadTime = FPlatformTime::ToSeconds(Stats.GetFastThreadFrameTime(Frame, EThreadType::Renderer));

	if( GameThreadTime > GHitchThreshold || RenderThreadTime > GHitchThreshold )
	{
		HitchIndex++;
		float ThisHitch = FMath::Max<float>(GameThreadTime, RenderThreadTime) * 1000.0f;
		TotalHitchTime += ThisHitch;
		UE_LOG(LogStats, Log, TEXT("------------------Thread Hitch %d, Frame %lld  %6.1fms ---------------"), HitchIndex, Frame, ThisHitch);
		FRawStatStackNode Stack;
		Stats.UncondenseStackStats(Frame, Stack);
		Stack.AddNameHierarchy();
		Stack.AddSelf();

		int64 MinCycles = int64( FMath::Min<float>( FMath::Max<float>( GHitchThreshold - 33.3f / 1000.0f, 1.0f / 1000.0f ), 1.0f / 1000.0f ) / FPlatformTime::GetSecondsPerCycle() );
		FRawStatStackNode* GameThread = NULL;
		FRawStatStackNode* RenderThread = NULL;

		for( auto ChildIter = Stack.Children.CreateConstIterator(); ChildIter; ++ChildIter )
		{
			const FName ThreadName = ChildIter.Value()->Meta.NameAndInfo.GetShortName();

			if( ThreadName == FName( NAME_GameThread ) )
			{
				GameThread = ChildIter.Value();
				UE_LOG( LogStats, Log, TEXT( "------------------ Game Thread %.2fms" ), GameThreadTime * 1000.0f );
				GameThread->Cull( MinCycles );
				GameThread->DebugPrint();
			}
			else if( ThreadName == FName( NAME_RenderThread ) )
			{
				RenderThread = ChildIter.Value();
				UE_LOG( LogStats, Log, TEXT( "------------------ Render Thread (%s) %.2fms" ), *RenderThread->Meta.NameAndInfo.GetRawName().ToString(), RenderThreadTime * 1000.0f );
				RenderThread->Cull( MinCycles );
				RenderThread->DebugPrint();
			}
		}

		if( !GameThread )
		{
			UE_LOG( LogStats, Warning, TEXT( "No game thread?!" ) );
		}

		if( !RenderThread )
		{
			UE_LOG( LogStats, Warning, TEXT( "No render thread." ) );
		}

		LastHitchFrame = Frame;
	}
}

#endif

static bool HandleCommandBroadcast(const FName& InStatName, bool& bOutCurrentEnabled, bool& bOutOthersEnabled)
{
	bOutCurrentEnabled = true;
	bOutOthersEnabled = false;

	// Check to see if all stats have been disabled... 
	static const FName NAME_NoGroup = FName(TEXT("STATGROUP_None"));
	if (InStatName == NAME_NoGroup)
	{
		// Iterate through all enabled groups.
		FCoreDelegates::StatDisableAll.Broadcast(true);

		return false;
	}

	// Check to see if/how this is already enabled.. (default to these incase it's not bound)
	FString StatString = InStatName.ToString();
	StatString.RemoveFromStart("STATGROUP_");
	if (FCoreDelegates::StatCheckEnabled.IsBound())
	{
		FCoreDelegates::StatCheckEnabled.Broadcast(*StatString, bOutCurrentEnabled, bOutOthersEnabled);
		if (!bOutCurrentEnabled)
		{
			FCoreDelegates::StatEnabled.Broadcast(*StatString);
		}
		else
		{
			FCoreDelegates::StatDisabled.Broadcast(*StatString);
		}
	}

	return true;
}

#if STATS

void FHUDGroupGameThreadRenderer::NewData(FGameThreadHudData* Data)
{
	delete Latest;
	Latest = Data;
}

FHUDGroupGameThreadRenderer& FHUDGroupGameThreadRenderer::Get()
{
	static FHUDGroupGameThreadRenderer Singleton;
	return Singleton;
}

FStatGroupGameThreadNotifier& FStatGroupGameThreadNotifier::Get()
{
	static FStatGroupGameThreadNotifier Singleton;
	return Singleton;
}

struct FInternalGroup
{
	/** Initialization constructor. */
	FInternalGroup(const FName InGroupName, const FName InGroupCategory, const EStatDisplayMode::Type InDisplayMode, TSet<FName>& InEnabledItems, FString& InGroupDescription)
		: GroupName( InGroupName )
		, GroupCategory(InGroupCategory)
		, DisplayMode( InDisplayMode )
	{
		// To avoid copy.
		Exchange( EnabledItems, InEnabledItems );
		Exchange( GroupDescription, InGroupDescription );
	}

	/** Set of elements which should be included in this group stats. */
	TSet<FName> EnabledItems;

	/** Name of this stat group. */
	FName GroupName;

	/** Category of this stat group. */
	FName GroupCategory;

	/** Description of this stat group. */
	FString GroupDescription;

	/** Display mode for this group. */
	EStatDisplayMode::Type DisplayMode;
};

/** Stats for the particular frame. */
struct FHudFrame
{
	TArray<FStatMessage> InclusiveAggregate;
	TArray<FStatMessage> ExclusiveAggregate;
	TArray<FStatMessage> NonStackStats;
	FRawStatStackNode HierarchyInclusive;
};

struct FHUDGroupManager 
{
	/** Contains all enabled groups. */
	TMap<FName,FInternalGroup> EnabledGroups;

	/** Contains all history frames. */
	TMap<int64,FHudFrame> History;

	/** Root stat stack for all frames, it's accumulating all the time, but can be reset with a command 'stat hier -reset'. */
	FRawStatStackNode TotalHierarchyInclusive;
	
	/** Flat array of messages, it's accumulating all the time, but can be reset with a command 'stat hier -reset'. */
	TArray<FStatMessage> TotalAggregateInclusive;
	TArray<FStatMessage> TotalNonStackStats;

	/** Root stat stack for history frames, by default it's for the last 20 frames. */
	FComplexRawStatStackNode AggregatedHierarchyHistory;
	TArray<FComplexStatMessage> AggregatedFlatHistory;
	TArray<FComplexStatMessage> AggregatedNonStackStatsHistory;

	/** Copy of the stat group command parameters. */
	FStatGroupParams Params;
	
	/** Number of frames for the root stat stack. */
	int32 NumTotalStackFrames;

	/** Index of the latest frame. */
	int64 LatestFrame;

	/** Reference to the stats state. */
	FStatsThreadState const& Stats;

	/** Whether it's enabled or not. */
	bool bEnabled;

	/** NewFrame delegate handle */
	FDelegateHandle NewFrameDelegateHandle;

	/** Default constructor. */
	FHUDGroupManager(FStatsThreadState const& InStats)
		: NumTotalStackFrames(0)
		, LatestFrame(-2)
		, Stats(InStats)
		, bEnabled(false)
	{
	}

	/** Handles hier or group command. */
	void HandleCommand( const FStatGroupParams& InParams, const bool bHierarchy )
	{
		Params = InParams;
		if( Params.ShouldReset() )
		{
			NumTotalStackFrames = 0;
		}

		ResizeFramesHistory( Params.MaxHistoryFrames.Get() );

		const FName MaybeGroupFName = FName(*(FString(TEXT("STATGROUP_")) + Params.Group.Get().GetPlainNameString()));
		bool bCurrentEnabled, bOthersEnabled;
		if (!HandleCommandBroadcast(MaybeGroupFName, bCurrentEnabled, bOthersEnabled))
		{
			// Remove all groups.
			EnabledGroups.Empty();
		}
		else
		{
			// Is this a group stat (as opposed to a simple stat?)
			const bool bGroupStat = Stats.Groups.Contains(MaybeGroupFName);
			if (bGroupStat)
			{
				// Is this group stat currently enabled?
				if (FInternalGroup* InternalGroup = EnabledGroups.Find(MaybeGroupFName))
				{
					// If this was only being used by the current viewport, remove it
					if (bCurrentEnabled && !bOthersEnabled)
					{
						if ((InternalGroup->DisplayMode & EStatDisplayMode::Hierarchical) && !bHierarchy)
						{
							InternalGroup->DisplayMode = EStatDisplayMode::Flat;
						}
						else if ((InternalGroup->DisplayMode & EStatDisplayMode::Flat) && bHierarchy)
						{
							InternalGroup->DisplayMode = EStatDisplayMode::Hierarchical;
						}
						else
						{
							EnabledGroups.Remove(MaybeGroupFName);
							NumTotalStackFrames = 0;
						}
					}
				}
				else
				{
					// If InternalGroup is null, it shouldn't be being used by any viewports					
					TSet<FName> EnabledItems;
					GetStatsForGroup(EnabledItems, MaybeGroupFName);

					const FStatMessage& Group = Stats.ShortNameToLongName.FindChecked(MaybeGroupFName);

					const FName GroupCategory = Group.NameAndInfo.GetGroupCategory();

					FString GroupDescription = Group.NameAndInfo.GetDescription();

					EnabledGroups.Add(MaybeGroupFName, FInternalGroup(MaybeGroupFName, GroupCategory, bHierarchy ? EStatDisplayMode::Hierarchical : EStatDisplayMode::Flat, EnabledItems, GroupDescription));
				}
			}
		}

		if( EnabledGroups.Num() && !bEnabled )
		{
			bEnabled = true;
			NewFrameDelegateHandle = Stats.NewFrameDelegate.AddRaw( this, &FHUDGroupManager::NewFrame );
			StatsMasterEnableAdd();
		}
		else if( !EnabledGroups.Num() && bEnabled )
		{
			Stats.NewFrameDelegate.Remove( NewFrameDelegateHandle );
			StatsMasterEnableSubtract();
			bEnabled = false;

			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.StatsToGame"),
				STAT_FSimpleDelegateGraphTask_StatsToGame,
				STATGROUP_TaskGraphTasks);

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
			(
				FSimpleDelegateGraphTask::FDelegate::CreateRaw(&FHUDGroupGameThreadRenderer::Get(), &FHUDGroupGameThreadRenderer::NewData, (FGameThreadHudData*)nullptr),
				GET_STATID(STAT_FSimpleDelegateGraphTask_StatsToGame), nullptr, ENamedThreads::GameThread
			);
		}
	}

	void ResizeFramesHistory( int32 MaxFrames )
	{
		History.Empty(MaxFrames + 1);
	}

	void LinearizeStackForItems( const FComplexRawStatStackNode& StackNode, const TSet<FName>& EnabledItems, TArray<FComplexStatMessage>& out_HistoryStack, TArray<int32>& out_Indentation, int32 Depth )
	{
		const bool bToBeAdded = EnabledItems.Contains( StackNode.ComplexStat.NameAndInfo.GetRawName() );
		if( bToBeAdded )
		{
			out_HistoryStack.Add( StackNode.ComplexStat );
			out_Indentation.Add( Depth );
		}

		for( auto It = StackNode.Children.CreateConstIterator(); It; ++It )
		{
			const FComplexRawStatStackNode& Child = *It.Value();
			LinearizeStackForItems( Child, EnabledItems, out_HistoryStack, out_Indentation, Depth+1 ); 
		}
	}

	void NewFrame(int64 TargetFrame)
	{
		SCOPE_CYCLE_COUNTER(STAT_HUDGroup);
		check(bEnabled);

		TSet<FName> HierEnabledItems;
		for( auto It = EnabledGroups.CreateConstIterator(); It; ++It )
		{
			HierEnabledItems.Append( It.Value().EnabledItems );
		}
		FGroupFilter Filter(HierEnabledItems);

		// Add a new frame to the history.
		FHudFrame& NewFrame = History.FindOrAdd(TargetFrame);
		
		// Generate root stats stack for current frame.
		Stats.UncondenseStackStats( TargetFrame, NewFrame.HierarchyInclusive, &Filter, &NewFrame.NonStackStats );
		NewFrame.HierarchyInclusive.Cull( TNumericLimits<int64>::Max(), Params.MaxHierarchyDepth.Get() );
		//NewFrame.HierarchyInclusive.AddNameHierarchy();
		NewFrame.HierarchyInclusive.AddSelf();

		{
			SCOPE_CYCLE_COUNTER(STAT_GetFlatAggregates);
			Stats.GetInclusiveAggregateStackStats( TargetFrame, NewFrame.InclusiveAggregate, &Filter, false );
			Stats.GetExclusiveAggregateStackStats( TargetFrame, NewFrame.ExclusiveAggregate, &Filter, false );
		}

		// Aggregate hierarchical stats.
		if( NumTotalStackFrames == 0 )
		{
			TotalHierarchyInclusive = NewFrame.HierarchyInclusive;
		}
		else
		{
			TotalHierarchyInclusive.MergeAdd( NewFrame.HierarchyInclusive );
		}
		
		// Aggregate flat stats.
		if( NumTotalStackFrames == 0 )
		{
			TotalAggregateInclusive = NewFrame.InclusiveAggregate;
		}
		else
		{
			FStatsUtils::AddMergeStatArray( TotalAggregateInclusive, NewFrame.InclusiveAggregate );
		}

		// Aggregate non-stack stats.
		if( NumTotalStackFrames == 0 )
		{
			TotalNonStackStats = NewFrame.NonStackStats;
		}
		else
		{
			FStatsUtils::AddMergeStatArray( TotalNonStackStats, NewFrame.NonStackStats );
		}
		NumTotalStackFrames ++;
		
		/** Not super efficient, but allows to sort different stat data types. */
		struct FStatValueComparer
		{
			FORCEINLINE_STATS bool operator()( const FStatMessage& A, const FStatMessage& B ) const
			{
				// We assume that stat data type may be only int64 or double.
				const EStatDataType::Type DataTypeA = A.NameAndInfo.GetField<EStatDataType>();
				const EStatDataType::Type DataTypeB = B.NameAndInfo.GetField<EStatDataType>();
				const bool bAIsInt = DataTypeA == EStatDataType::ST_int64;
				const bool bBIsInt = DataTypeB == EStatDataType::ST_int64;
				const bool bAIsDbl = DataTypeA == EStatDataType::ST_double;
				const bool bBIsDbl = DataTypeB == EStatDataType::ST_double;

				const double ValueA = bAIsInt ? A.GetValue_int64() : A.GetValue_double();
				const double ValueB = bBIsInt ? B.GetValue_int64() : B.GetValue_double();

				return ValueA == ValueB ? FStatNameComparer<FStatMessage>()(A,B) : ValueA > ValueB;
			}
		};
			
		// Sort total history stats by the specified item.
		EStatCompareBy::Type StatCompare = Params.SortBy.Get();
		if( StatCompare == EStatCompareBy::Sum )
		{
			TotalHierarchyInclusive.Sort( FStatDurationComparer<FRawStatStackNode>() );
			TotalAggregateInclusive.Sort( FStatDurationComparer<FStatMessage>() );
			TotalNonStackStats.Sort( FStatValueComparer() );
		}
		else if( StatCompare == EStatCompareBy::CallCount )
		{
			TotalHierarchyInclusive.Sort( FStatCallCountComparer<FRawStatStackNode>() );
			TotalAggregateInclusive.Sort( FStatCallCountComparer<FStatMessage>() );
			TotalNonStackStats.Sort( FStatValueComparer() );
		}
		else if( StatCompare == EStatCompareBy::Name )
		{
			TotalHierarchyInclusive.Sort( FStatNameComparer<FRawStatStackNode>() );
			TotalAggregateInclusive.Sort( FStatNameComparer<FStatMessage>() );
			TotalNonStackStats.Sort( FStatNameComparer<FStatMessage>() );
		}		

		// We want contiguous frames only.
		if( TargetFrame - LatestFrame > 1 ) 
		{
			ResizeFramesHistory( Params.MaxHistoryFrames.Get() );
		}

		RemoveFramesOutOfHistory(TargetFrame);

		const int32 NumFrames = History.Num();
		check(NumFrames <= Params.MaxHistoryFrames.Get());
		if( NumFrames > 0 )
		{
			FGameThreadHudData* ToGame = new FGameThreadHudData(false);

			// Copy the total stats stack to the history stats stack and clear all nodes' data and set data type to none.
			// Called to maintain the hierarchy.
			AggregatedHierarchyHistory.CopyNameHierarchy( TotalHierarchyInclusive );

			// Copy flat-stack stats
			AggregatedFlatHistory.Reset( TotalAggregateInclusive.Num() );
			for( int32 Index = 0; Index < TotalAggregateInclusive.Num(); ++Index )
			{
				const FStatMessage& StatMessage = TotalAggregateInclusive[Index];
				new(AggregatedFlatHistory) FComplexStatMessage(StatMessage);
			}

			// Copy non-stack stats
			AggregatedNonStackStatsHistory.Reset( TotalNonStackStats.Num() );
			for( int32 Index = 0; Index < TotalNonStackStats.Num(); ++Index )
			{
				const FStatMessage& StatMessage = TotalNonStackStats[Index];
				new(AggregatedNonStackStatsHistory) FComplexStatMessage(StatMessage);
			}
			
			// Accumulate hierarchy, flat and non-stack stats.
			for( auto It = History.CreateConstIterator(); It; ++It )
			{
				SCOPE_CYCLE_COUNTER(STAT_Accumulate);
				const FHudFrame& Frame = It.Value();

				AggregatedHierarchyHistory.MergeAddAndMax( Frame.HierarchyInclusive );

				FComplexStatUtils::MergeAddAndMaxArray( AggregatedFlatHistory, Frame.InclusiveAggregate, EComplexStatField::IncSum, EComplexStatField::IncMax );
				FComplexStatUtils::MergeAddAndMaxArray( AggregatedFlatHistory, Frame.ExclusiveAggregate, EComplexStatField::ExcSum, EComplexStatField::ExcMax );

				FComplexStatUtils::MergeAddAndMaxArray( AggregatedNonStackStatsHistory, Frame.NonStackStats, EComplexStatField::IncSum, EComplexStatField::IncMax );
			}

			// Divide stats to get average values.
			AggregatedHierarchyHistory.Divide( NumFrames );
			AggregatedHierarchyHistory.CopyExclusivesFromSelf();

			FComplexStatUtils::DiviveStatArray( AggregatedFlatHistory, NumFrames, EComplexStatField::IncSum, EComplexStatField::IncAve );
			FComplexStatUtils::DiviveStatArray( AggregatedFlatHistory, NumFrames, EComplexStatField::ExcSum, EComplexStatField::ExcAve );

			FComplexStatUtils::DiviveStatArray( AggregatedNonStackStatsHistory, NumFrames, EComplexStatField::IncSum, EComplexStatField::IncAve );
	
			// Iterate through all enabled groups.
			for( auto It = EnabledGroups.CreateIterator(); It; ++It )
			{
				const FName& GroupName = It.Key();
				FInternalGroup& InternalGroup = It.Value();

				// Create a new hud group.
				new(ToGame->HudGroups) FHudGroup();
				FHudGroup& HudGroup = ToGame->HudGroups.Last();

				ToGame->GroupNames.Add( GroupName );
				ToGame->GroupDescriptions.Add( InternalGroup.GroupDescription );

				if( InternalGroup.DisplayMode & EStatDisplayMode::Hierarchical )
				{
					// Linearize stack stats for easier rendering.
					LinearizeStackForItems( AggregatedHierarchyHistory, InternalGroup.EnabledItems, HudGroup.HierAggregate, HudGroup.Indentation, 0 );
				}
				
				if( InternalGroup.DisplayMode & EStatDisplayMode::Flat )
				{
					// Copy flat stats
					for( int32 Index = 0; Index < AggregatedFlatHistory.Num(); ++Index )
					{
						const FComplexStatMessage& AggregatedStatMessage = AggregatedFlatHistory[Index];
						const bool bIsNonStackStat = !AggregatedStatMessage.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration);
						const bool bToBeAdded = InternalGroup.EnabledItems.Contains( AggregatedStatMessage.NameAndInfo.GetRawName() );
						if( bToBeAdded )
						{
							new(HudGroup.FlatAggregate) FComplexStatMessage( AggregatedStatMessage );
						}
					}
				}

				// Copy non-stack stats assigned to memory and counter groups.
				for( int32 Index = 0; Index < AggregatedNonStackStatsHistory.Num(); ++Index )
				{
					const FComplexStatMessage& AggregatedStatMessage = AggregatedNonStackStatsHistory[Index];
					const bool bIsMemory = AggregatedStatMessage.NameAndInfo.GetFlag( EStatMetaFlags::IsMemory );
					TArray<FComplexStatMessage>& Dest = bIsMemory ? HudGroup.MemoryAggregate : HudGroup.CountersAggregate; 

					const bool bToBeAdded = InternalGroup.EnabledItems.Contains( AggregatedStatMessage.NameAndInfo.GetRawName() );
					if( bToBeAdded )
					{
						new(Dest) FComplexStatMessage(AggregatedStatMessage);
					}	
				}
			}

			for (auto It = Stats.MemoryPoolToCapacityLongName.CreateConstIterator(); It; ++It)
			{
				FName LongName = It.Value();
				// dig out the abbreviation
				{
					FString LongNameStr = LongName.ToString();
					int32 Open = LongNameStr.Find("[");
					int32 Close = LongNameStr.Find("]");
					if (Open >= 0 && Close >= 0 && Open + 1 < Close)
					{
						FString Abbrev = LongNameStr.Mid(Open + 1, Close - Open - 1);
						ToGame->PoolAbbreviation.Add(It.Key(), Abbrev);
					}
				}
				// see if we have a capacity
				FStatMessage const* Result = Stats.NotClearedEveryFrame.Find(LongName);
				if (Result && Result->NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
				{
					int64 Capacity = Result->GetValue_int64();
					if (Capacity > 0)
					{
						ToGame->PoolCapacity.Add(It.Key(), Capacity);
					}
				}
			}

			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.StatsHierToGame"),
				STAT_FSimpleDelegateGraphTask_StatsHierToGame,
				STATGROUP_TaskGraphTasks);

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
			(
				FSimpleDelegateGraphTask::FDelegate::CreateRaw(&FHUDGroupGameThreadRenderer::Get(), &FHUDGroupGameThreadRenderer::NewData, ToGame),
				GET_STATID(STAT_FSimpleDelegateGraphTask_StatsHierToGame), nullptr, ENamedThreads::GameThread
			);
		}	
	}

	void RemoveFramesOutOfHistory( int64 TargetFrame )
	{
		LatestFrame = TargetFrame;
		for (auto It = History.CreateIterator(); It; ++It)
		{
			if (int32(LatestFrame - It.Key()) >= Params.MaxHistoryFrames.Get())
			{
				It.RemoveCurrent();
			}
		}
		check(History.Num() <= Params.MaxHistoryFrames.Get());
	}

	void RebuildItems( TSet<FName>& out_EnabledItems, const TSet<FName>& InEnabledGroups )
	{
		out_EnabledItems.Empty();
		for (auto It = InEnabledGroups.CreateConstIterator(); It; ++It)
		{
			TArray<FName> GroupItems;
			Stats.Groups.MultiFind(*It, GroupItems);
			for (int32 Index = 0; Index < GroupItems.Num(); Index++)
			{
				out_EnabledItems.Add(GroupItems[Index]); // short name
				FStatMessage const* LongName = Stats.ShortNameToLongName.Find(GroupItems[Index]);
				if (LongName)
				{
					out_EnabledItems.Add(LongName->NameAndInfo.GetRawName()); // long name
				}
			}
		}

		out_EnabledItems.Add(NAME_Self);
		out_EnabledItems.Add(NAME_OtherChildren);
	}

	void GetStatsForGroup( TSet<FName>& out_EnabledItems, const FName GroupName )
	{
		out_EnabledItems.Empty();
	
		TArray<FName> GroupItems;
		Stats.Groups.MultiFind( GroupName, GroupItems );
		for (int32 Index = 0; Index < GroupItems.Num(); Index++)
		{
			out_EnabledItems.Add(GroupItems[Index]); // short name
			FStatMessage const* LongName = Stats.ShortNameToLongName.Find(GroupItems[Index]);
			if (LongName)
			{
				out_EnabledItems.Add(LongName->NameAndInfo.GetRawName()); // long name
			}
		}

		out_EnabledItems.Add(NAME_Self);
		out_EnabledItems.Add(NAME_OtherChildren);
	}

	static FHUDGroupManager& Get(FStatsThreadState const& Stats)
	{
		static FHUDGroupManager Singleton(Stats);
		return Singleton;
	}
};


/*-----------------------------------------------------------------------------
	Dump...
-----------------------------------------------------------------------------*/

static float DumpCull = 5.0f;
static int32 MaxDepth = MAX_int32;
static FString NameFilter;
static FDelegateHandle DumpFrameDelegateHandle;
static FDelegateHandle DumpCPUDelegateHandle;
static FDelegateHandle DumpMemoryDelegateHandle;

static void DumpFrame(int64 Frame)
{
	FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	int64 Latest = Stats.GetLatestValidFrame();
	check(Latest > 0);
	DumpHistoryFrame(Stats, Latest, DumpCull, MaxDepth, *NameFilter);
	Stats.NewFrameDelegate.Remove(DumpFrameDelegateHandle);
	StatsMasterEnableSubtract();
}

static void DumpCPU(int64 Frame)
{
	FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	int64 Latest = Stats.GetLatestValidFrame();
	check(Latest > 0);
	DumpCPUSummary(Stats, Latest);
	Stats.NewFrameDelegate.Remove(DumpCPUDelegateHandle);
	StatsMasterEnableSubtract();
}

static struct FDumpMultiple* DumpMultiple = NULL;

struct FDumpMultiple
{
	FStatsThreadState& Stats;
	bool bAverage;
	bool bSum;
	int32 NumFrames;
	int32 NumFramesToGo;
	FRawStatStackNode* Stack;
	FDelegateHandle NewFrameDelegateHandle;

	FDumpMultiple()
		: Stats(FStatsThreadState::GetLocalState())
		, bAverage(true)
		, bSum(false)
		, NumFrames(0)
		, NumFramesToGo(0)
		, Stack(NULL)
	{
		StatsMasterEnableAdd();
		NewFrameDelegateHandle = Stats.NewFrameDelegate.AddRaw(this, &FDumpMultiple::NewFrame);
	}

	~FDumpMultiple()
	{
		if (Stack && NumFrames)
		{
			if (bAverage)
			{
				if (NumFrames > 1)
				{
					Stack->Divide(NumFrames);
				}
				UE_LOG(LogStats, Log, TEXT("------------------ %d frames, average ---------------"), NumFrames );
			}
			else if (bSum)
			{
				UE_LOG(LogStats, Log, TEXT("------------------ %d frames, sum ---------------"), NumFrames );
			}
			else
			{
				UE_LOG(LogStats, Log, TEXT("------------------ %d frames, max ---------------"), NumFrames );
			}
			Stack->AddNameHierarchy();
			Stack->AddSelf();
			if (DumpCull != 0.0f)
			{
				Stack->Cull(int64(DumpCull / FPlatformTime::ToMilliseconds(1.0f)));
			}
			Stack->DebugPrint(*NameFilter, MaxDepth);
			delete Stack;
			Stack = NULL;
		}
		Stats.NewFrameDelegate.Remove(NewFrameDelegateHandle);
		StatsMasterEnableSubtract();
		DumpMultiple = NULL;
	}

	void NewFrame(int64 TargetFrame)
	{
		if (!Stack)
		{
			Stack = new FRawStatStackNode();
			Stats.UncondenseStackStats(TargetFrame, *Stack);
		}
		else
		{
			FRawStatStackNode FrameStack;
			Stats.UncondenseStackStats(TargetFrame, FrameStack);
			if (bAverage || bSum)
			{
				Stack->MergeAdd(FrameStack);
			}
			else
			{
				Stack->MergeMax(FrameStack);
			}
		}
		NumFrames++;
		if (NumFrames >= NumFramesToGo)
		{
			delete this;
		}
	}
};

/** Prints stats help to the specified output device. This is queued to be executed on the game thread. */
static void PrintStatsHelpToOutputDevice( FOutputDevice& Ar )
{
	Ar.Log( TEXT("Empty stat command!"));
	Ar.Log( TEXT("Here is the brief list of stats console commands"));
	Ar.Log( TEXT("stat dumpframe [-ms=5.0] [-root=empty] [-depth=maxint] - dumps a frame of stats"));
	Ar.Log( TEXT("    stat dumpframe -ms=.001 -root=initviews"));
	Ar.Log( TEXT("    stat dumpframe -ms=.001 -root=shadow"));

	Ar.Log( TEXT("stat dumpave|dumpmax|dumpsum  [-start | -stop | -num=30] [-ms=5.0] [-depth=maxint] - aggregate stats over multiple frames"));
	Ar.Log( TEXT("stat dumphitches  - dumps hitches"));
	Ar.Log( TEXT("stat dumpnonframe - dumps non-frame stats, usually memory stats"));
	Ar.Log( TEXT("stat dumpcpu - dumps cpu stats"));

	Ar.Log( TEXT("stat groupname[+] - toggles displaying stats group, + enables hierarchical display"));
	Ar.Log( TEXT("stat hier -group=groupname [-sortby=name] [-maxhistoryframes=60] [-reset]"));
	Ar.Log( TEXT("    - groupname is a stat group like initviews or statsystem"));
	Ar.Log( TEXT("    - sortby can be name (by stat FName), callcount (by number of calls, only for scoped cycle counters), num(by total inclusive time)"));
	Ar.Log( TEXT("    - maxhistoryframes (default 60, number of frames used to generate the stats displayed on the hud)"));
	Ar.Log( TEXT("    - reset (reset the accumulated history)"));
	Ar.Log( TEXT("stat none - disables drawing all stats groups"));

	Ar.Log( TEXT("stat group list|listall|enable name|disable name|none|all|default - manages stats groups"));

#if WITH_ENGINE
	// Engine @see FStatCmdEngine
	Ar.Log( TEXT( "stat display -font=small[tiny]" ) );
	Ar.Log( TEXT( "    Changes stats rendering display options" ) );
#endif // WITH_ENGINE

	Ar.Log( TEXT("stat startfile - starts dumping a capture"));
	Ar.Log( TEXT("stat stopfile - stops dumping a capture (regular, raw, memory)"));

	Ar.Log( TEXT("stat startfileraw - starts dumping a raw capture"));

	Ar.Log( TEXT("stat toggledebug - toggles tracking the most memory expensive stats"));

	Ar.Log( TEXT("add -memoryprofiler in the command line to enable the memory profiling"));
	Ar.Log( TEXT("stat stopfile - stops tracking all memory operations and writes the results to the file"));
}

// @TODO yrx 2014-12-01 Move to StatsFile.cpp/.h
static void CommandTestFile()
{
	const FString& LastFileSaved = FCommandStatsFile::Get().LastFileSaved;

	FStatsThreadState Loaded( LastFileSaved );
	if( Loaded.GetLatestValidFrame() < 0 )
	{
		UE_LOG( LogStats, Log, TEXT( "Failed to stats file: %s" ), *LastFileSaved );
		return;
	}
	UE_LOG( LogStats, Log, TEXT( "Loaded stats file: %s, %lld frame" ), *LastFileSaved, 1 + Loaded.GetLatestValidFrame() - Loaded.GetOldestValidFrame() );
	{
		int64 TestFrame = Loaded.GetOldestValidFrame();
		UE_LOG( LogStats, Log, TEXT( "**************************** Test Frame %lld" ), TestFrame );
		DumpHistoryFrame( Loaded, TestFrame );
	}
	{
		int64 TestFrame = (Loaded.GetLatestValidFrame() + Loaded.GetOldestValidFrame()) / 2;
		if( Loaded.IsFrameValid( TestFrame ) )
		{
			UE_LOG( LogStats, Log, TEXT( "**************************** Test Frame %lld" ), TestFrame );
			DumpHistoryFrame( Loaded, TestFrame );
		}
	}
	{
		int64 TestFrame = Loaded.GetLatestValidFrame();
		UE_LOG( LogStats, Log, TEXT( "**************************** Test Frame %lld" ), TestFrame );
		DumpHistoryFrame( Loaded, TestFrame );
	}
}

#endif

static void StatCmd(FString InCmd)
{
	const TCHAR* Cmd = *InCmd;
#if STATS
	FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	DumpCull = 5.0f;
	MaxDepth = MAX_int32;
	NameFilter.Empty();

	FParse::Value(Cmd, TEXT("ROOT="), NameFilter);
	FParse::Value(Cmd, TEXT("MS="), DumpCull);
	FParse::Value(Cmd, TEXT("DEPTH="), MaxDepth);
	if( FParse::Command(&Cmd,TEXT("DUMPFRAME")) )
	{
		StatsMasterEnableAdd();
		DumpFrameDelegateHandle = Stats.NewFrameDelegate.AddStatic(&DumpFrame);
	} 
	else if ( FParse::Command(&Cmd,TEXT("DUMPNONFRAME")) )
	{
		DumpNonFrame(Stats);
	}
	else if ( FParse::Command(&Cmd,TEXT("DUMPCPU")) )
	{
		StatsMasterEnableAdd();
		DumpCPUDelegateHandle = Stats.NewFrameDelegate.AddStatic(&DumpCPU);
	}
	else if( FParse::Command(&Cmd,TEXT("STOP")) )
	{
		delete DumpMultiple;
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPAVE")) )
	{
		bool bIsStart = FString(Cmd).Find(TEXT("-start")) != INDEX_NONE;
		bool bIsStop = FString(Cmd).Find(TEXT("-stop")) != INDEX_NONE;
		delete DumpMultiple;
		if (!bIsStop)
		{
			DumpMultiple = new FDumpMultiple();
			DumpMultiple->NumFramesToGo = bIsStart ? MAX_int32 : 30;
			FParse::Value(Cmd, TEXT("NUM="), DumpMultiple->NumFramesToGo);
			DumpMultiple->bAverage = true;
			DumpMultiple->bSum = false;
		}
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPMAX")) )
	{
		bool bIsStart = FString(Cmd).Find(TEXT("-start")) != INDEX_NONE;
		bool bIsStop = FString(Cmd).Find(TEXT("-stop")) != INDEX_NONE;
		delete DumpMultiple;
		if (!bIsStop)
		{
			DumpMultiple = new FDumpMultiple();
			DumpMultiple->NumFramesToGo = bIsStart ? MAX_int32 : 30;
			FParse::Value(Cmd, TEXT("NUM="), DumpMultiple->NumFramesToGo);
			DumpMultiple->bAverage = false;
			DumpMultiple->bSum = false;
		}
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPSUM")) )
	{
		bool bIsStart = FString(Cmd).Find(TEXT("-start")) != INDEX_NONE;
		bool bIsStop = FString(Cmd).Find(TEXT("-stop")) != INDEX_NONE;
		delete DumpMultiple;
		if (!bIsStop)
		{
			DumpMultiple = new FDumpMultiple();
			DumpMultiple->NumFramesToGo = bIsStart ? MAX_int32 : 30;
			FParse::Value(Cmd, TEXT("NUM="), DumpMultiple->NumFramesToGo);
			DumpMultiple->bAverage = false;
			DumpMultiple->bSum = true;
		}
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPHITCHES")) )
	{
		static bool bToggle = false;
		static FDelegateHandle DumpHitchDelegateHandle;
		bToggle = !bToggle;
		if (bToggle)
		{
			StatsMasterEnableAdd();
			HitchIndex = 0;
			TotalHitchTime = 0.0f;
			DumpHitchDelegateHandle = Stats.NewFrameDelegate.AddStatic(&DumpHitch);
		}
		else
		{
			StatsMasterEnableSubtract();
			Stats.NewFrameDelegate.Remove(DumpHitchDelegateHandle);
			UE_LOG(LogStats, Log, TEXT( "**************************** %d hitches    %8.0fms total hitch time" ), HitchIndex, TotalHitchTime);
		}
	}
	else if( FParse::Command( &Cmd, TEXT( "STARTFILE" ) ) )
	{
		FString Filename;
		FParse::Token( Cmd, Filename, false );
		FCommandStatsFile::Get().Start( Filename );
	}
	else if( FParse::Command( &Cmd, TEXT( "StartFileRaw" ) ) )
	{
		FThreadStats::EnableRawStats();
		FString Filename;
		FParse::Token( Cmd, Filename, false );
		FCommandStatsFile::Get().StartRaw( Filename );
	}
	else if( FParse::Command( &Cmd, TEXT( "STOPFILE" ) ) 
		|| FParse::Command( &Cmd, TEXT( "StopFileRaw" ) ) )
	{
		// Stop writing to a file.
		FCommandStatsFile::Get().Stop();
		FThreadStats::DisableRawStats();

		if( FStatsMallocProfilerProxy::HasMemoryProfilerToken() )
		{
			if( FStatsMallocProfilerProxy::Get()->GetState() )
			{
				// Disable memory profiler and restore default stats groups.
				FStatsMallocProfilerProxy::Get()->SetState( false );
				IStatGroupEnableManager::Get().StatGroupEnableManagerCommand( TEXT( "default" ) );
			}
		}
		
		Stats.ResetStatsForRawStats();

		// Disable displaying the raw stats memory overhead.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(&FHUDGroupGameThreadRenderer::Get(), &FHUDGroupGameThreadRenderer::NewData, (FGameThreadHudData*)nullptr),
			TStatId(), nullptr, ENamedThreads::GameThread
		);
	}
	else if( FParse::Command( &Cmd, TEXT( "TESTFILE" ) ) )
	{
		CommandTestFile();
	}
	else if( FParse::Command( &Cmd, TEXT( "testdisable" ) ) )
	{
		FThreadStats::MasterDisableForever();
	}
	else if ( FParse::Command( &Cmd, TEXT( "none" ) ) )
	{
		FStatGroupParams Params;
		FHUDGroupManager::Get(Stats).HandleCommand(Params,false);
	}
	else if ( FParse::Command( &Cmd, TEXT( "group" ) ) )
	{
		IStatGroupEnableManager::Get().StatGroupEnableManagerCommand(Cmd);
	}
	else if ( FParse::Command( &Cmd, TEXT( "toggledebug" ) ) )
	{
		FStatsThreadState::GetLocalState().ToggleFindMemoryExtensiveStats();
	}
	else if ( FParse::Command( &Cmd, TEXT( "memoryprofiler" ) ) )
	{
		if( FParse::Command( &Cmd, TEXT( "snapshot" ) ) )
		{
			// Put a snapshot marker in the raw stats.
			// @TODO yrx 2014-12-03 
		}
	}
	// @see FStatHierParams
	else if ( FParse::Command( &Cmd, TEXT( "hier" ) ) )
	{
		FStatGroupParams Params( Cmd );
		FHUDGroupManager::Get(Stats).HandleCommand(Params, true);
	}
	else
#endif
	{
		FString MaybeGroup;
		FParse::Token(Cmd, MaybeGroup, false);

		if( MaybeGroup.Len() > 0 )
		{
			// If there is + at the end of the group name switch into hierarchical view mode.
			const int32 PlusPos = MaybeGroup.Len()-1;
			const bool bHierarchy = MaybeGroup[MaybeGroup.Len()-1] == TEXT('+');
			if( bHierarchy )
			{
				MaybeGroup.RemoveAt(PlusPos,1,false);
			}

			const FName MaybeGroupFName = FName(*MaybeGroup);
#if STATS
			// Try to parse.
			FStatGroupParams Params( Cmd );
			Params.Group.Set( MaybeGroupFName );
			FHUDGroupManager::Get(Stats).HandleCommand(Params, bHierarchy);
#else
			// If stats aren't enabled, broadcast so engine stats can still be triggered
			bool bCurrentEnabled, bOthersEnabled;
			HandleCommandBroadcast(MaybeGroupFName, bCurrentEnabled, bOthersEnabled);
#endif
		}
		else
		{
			// Display a help. Handled by DirectStatsCommand.
		}
	}
}

/** Exec used to execute core stats commands on the stats thread. */
static class FStatCmdCore : private FSelfRegisteringExec
{
public:
	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld*, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		// Block the thread as this affects external stat states now
		return DirectStatsCommand(Cmd,true,&Ar);
	}
}
StatCmdCoreExec;

bool DirectStatsCommand(const TCHAR* Cmd, bool bBlockForCompletion /*= false*/, FOutputDevice* Ar /*= nullptr*/)
{
	bool bResult = false;
	if(FParse::Command(&Cmd,TEXT("stat")))
	{
		FString AddArgs;
		const TCHAR* TempCmd = Cmd;

		FString ArgNoWhitespaces = FDefaultValueHelper::RemoveWhitespaces(TempCmd);
		const bool bIsEmpty = ArgNoWhitespaces.IsEmpty();
#if STATS
		bResult = true;
		if (bIsEmpty && Ar)
		{
			PrintStatsHelpToOutputDevice( *Ar );
		}
		else if( FParse::Command( &TempCmd, TEXT( "STARTFILE" ) ) )
		{
			AddArgs += TEXT(" ");
			AddArgs += CreateProfileFilename( FStatConstants::StatsFileExtension, true );
		}
		else if( FParse::Command( &TempCmd, TEXT( "StartFileRaw" ) ) )
		{
			AddArgs += TEXT( " " );
			AddArgs += CreateProfileFilename( FStatConstants::StatsFileRawExtension, true );
		}
		else if( FParse::Command(&TempCmd,TEXT("DUMPFRAME")) )
		{
		}
		else if ( FParse::Command(&TempCmd,TEXT("DUMPNONFRAME")) )
		{
		}
		else if ( FParse::Command(&TempCmd,TEXT("DUMPCPU")) )
		{
		}
		else if( FParse::Command(&TempCmd,TEXT("STOP")) )
		{
		}
		else if( FParse::Command(&TempCmd,TEXT("DUMPAVE")) )
		{
		}
		else if( FParse::Command(&TempCmd,TEXT("DUMPMAX")) )
		{
		}
		else if( FParse::Command(&TempCmd,TEXT("DUMPSUM")) )
		{
		}
		else if( FParse::Command(&TempCmd,TEXT("DUMPHITCHES")) )
		{
		}
		else if( FParse::Command( &TempCmd, TEXT( "STOPFILE" ) ) )
		{
		}
		else if( FParse::Command( &TempCmd, TEXT( "TESTFILE" ) ) )
		{
		}
		else if( FParse::Command( &TempCmd, TEXT( "testdisable" ) ) )
		{
		}
		else if ( FParse::Command( &TempCmd, TEXT( "none" ) ) )
		{
		}
		else if ( FParse::Command( &TempCmd, TEXT( "group" ) ) )
		{
		}
		else if ( FParse::Command( &TempCmd, TEXT( "hier" ) ) )
		{
		}
		else if ( FParse::Command( &TempCmd, TEXT( "net" ) ) )
		{
		}
		else if ( FParse::Command( &TempCmd, TEXT( "toggledebug" ) ) )
		{
		}
		else if ( FParse::Command( &TempCmd, TEXT( "memoryprofiler" ) ) )
		{
		}
		else
		{
			bResult = false;

			FString MaybeGroup;
			if (FParse::Token(TempCmd, MaybeGroup, false) && MaybeGroup.Len() > 0)
			{
				// If there is + at the end of the group name, remove it
				const int32 PlusPos = MaybeGroup.Len() - 1;
				const bool bHierarchy = MaybeGroup[MaybeGroup.Len() - 1] == TEXT('+');
				if (bHierarchy)
				{
					MaybeGroup.RemoveAt(PlusPos, 1, false);
				}

				const FName MaybeGroupFName = FName(*(FString(TEXT("STATGROUP_")) + MaybeGroup));
				bResult = FStatGroupGameThreadNotifier::Get().StatGroupNames.Contains(MaybeGroupFName);
			}
		}
#endif

		check(IsInGameThread());
		if( !bIsEmpty )
		{
			const FString FullCmd = FString(Cmd) + AddArgs;
#if STATS
			ENamedThreads::Type ThreadType = ENamedThreads::GameThread;
			if (FPlatformProcess::SupportsMultithreading())
			{
				ThreadType = ENamedThreads::StatsThread;
			}

			// make sure these are initialized on the game thread
			FHUDGroupGameThreadRenderer::Get();
			FStatGroupGameThreadNotifier::Get();

			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.StatCmd"),
				STAT_FSimpleDelegateGraphTask_StatCmd,
				STATGROUP_TaskGraphTasks);

			FGraphEventRef CompleteHandle = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&StatCmd, FullCmd),
				GET_STATID(STAT_FSimpleDelegateGraphTask_StatCmd), NULL, ThreadType
			);
			if (bBlockForCompletion)
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompleteHandle);
				GLog->FlushThreadedLogs();
			}
#else
			// If stats aren't enabled, broadcast so engine stats can still be triggered
			StatCmd(FullCmd);
#endif
		}
	}
	return bResult;
}

#if STATS

static void GetPermanentStats_StatsThread(TArray<FStatMessage>* OutStats)
{
	FStatsThreadState& StatsData = FStatsThreadState::GetLocalState();
	TArray<FStatMessage>& Stats = *OutStats;
	for (auto It = StatsData.NotClearedEveryFrame.CreateConstIterator(); It; ++It)
	{
		Stats.Add(It.Value());
	}
	Stats.Sort(FGroupSort());
}

void GetPermanentStats(TArray<FStatMessage>& OutStats)
{
	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.GetPermanentStatsString_StatsThread"),
		STAT_FSimpleDelegateGraphTask_GetPermanentStatsString_StatsThread,
		STATGROUP_TaskGraphTasks);

	FGraphEventRef CompleteHandle = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateStatic(&GetPermanentStats_StatsThread, &OutStats),
		GET_STATID(STAT_FSimpleDelegateGraphTask_GetPermanentStatsString_StatsThread), NULL,
		FPlatformProcess::SupportsMultithreading() ? ENamedThreads::StatsThread : ENamedThreads::GameThread
	);
	FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompleteHandle);
}


#endif

