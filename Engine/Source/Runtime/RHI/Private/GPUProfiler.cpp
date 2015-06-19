// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUProfiler.h: Hierarchical GPU Profiler Implementation.
=============================================================================*/

#include "RHI.h"
#include "GPUProfiler.h"

#if !UE_BUILD_SHIPPING
#include "STaskGraph.h"
#include "ModuleManager.h"
#include "TaskGraphInterfaces.h"
#endif

#define LOCTEXT_NAMESPACE "GpuProfiler"

static TAutoConsoleVariable<FString> GProfileGPUPatternCVar(
	TEXT("r.ProfileGPU.Pattern"),
	TEXT("*"),
	TEXT("Allows to filter the entries when using ProfileGPU, the pattern match is case sensitive.\n")
	TEXT("'*' can be used in the end to get all entries starting with the string.\n")
	TEXT("    '*' without any leading characters disables the pattern matching and uses a time threshold instead (default).\n")
	TEXT("'?' allows to ignore one character.\n")
	TEXT("e.g. AmbientOcclusionSetup, AmbientOcclusion*, Ambient???lusion*, *"),
	ECVF_Default);

static TAutoConsoleVariable<FString> GProfileGPURootCVar(
	TEXT("r.ProfileGPU.Root"),
	TEXT("*"),
	TEXT("Allows to filter the tree when using ProfileGPU, the pattern match is case sensitive."),
	ECVF_Default);

struct FNodeStatsCompare
{
	/** Sorts nodes by descending durations. */
	FORCEINLINE bool operator()( const FGPUProfilerEventNodeStats& A, const FGPUProfilerEventNodeStats& B ) const
	{
		return B.TimingResult < A.TimingResult;
	}
};


/** Recursively generates a histogram of nodes and stores their timing in TimingResult. */
static void GatherStatsEventNode(FGPUProfilerEventNode* Node, int32 Depth, TMap<FString, FGPUProfilerEventNodeStats>& EventHistogram)
{
	if (Node->NumDraws > 0 || Node->Children.Num() > 0)
	{
		Node->TimingResult = Node->GetTiming() * 1000.0f;

		FGPUProfilerEventNodeStats* FoundHistogramBucket = EventHistogram.Find(Node->Name);
		if (FoundHistogramBucket)
		{
			FoundHistogramBucket->NumDraws += Node->NumDraws;
			FoundHistogramBucket->NumPrimitives += Node->NumPrimitives;
			FoundHistogramBucket->NumVertices += Node->NumVertices;
			FoundHistogramBucket->TimingResult += Node->TimingResult;
			FoundHistogramBucket->NumEvents++;
		}
		else
		{
			FGPUProfilerEventNodeStats NewNodeStats;
			NewNodeStats.NumDraws = Node->NumDraws;
			NewNodeStats.NumPrimitives = Node->NumPrimitives;
			NewNodeStats.NumVertices = Node->NumVertices;
			NewNodeStats.TimingResult = Node->TimingResult;
			NewNodeStats.NumEvents = 1;
			EventHistogram.Add(Node->Name, NewNodeStats);
		}

		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			// Traverse children
			GatherStatsEventNode(Node->Children[ChildIndex], Depth + 1, EventHistogram);
		}
	}
}

/** Recursively dumps stats for each node with a depth first traversal. */
static void DumpStatsEventNode(FGPUProfilerEventNode* Node, float RootResult, int32 Depth, const FWildcardString& WildcardFilter, bool bParentMatchedFilter, int32& NumNodes, int32& NumDraws)
{
	NumNodes++;
	if (Node->NumDraws > 0 || Node->Children.Num() > 0)
	{
		NumDraws += Node->NumDraws;
		// Percent that this node was of the total frame time
		const float Percent = Node->TimingResult * 100.0f / (RootResult * 1000.0f);

		const int32 EffectiveDepth = FMath::Max(Depth - 1, 0);

		const bool bMatchesFilter = bParentMatchedFilter || WildcardFilter.IsMatch(Node->Name);

		if (bMatchesFilter)
		{
			// Print information about this node, padded to its depth in the tree
			UE_LOG(LogRHI, Warning, TEXT("%s%4.1f%%%5.2fms   %s %u draws %u prims %u verts"), 
				*FString(TEXT("")).LeftPad(EffectiveDepth * 3), 
				Percent,
				Node->TimingResult,
				*Node->Name,
				Node->NumDraws,
				Node->NumPrimitives,
				Node->NumVertices
				);
		}

		float TotalChildTime = 0;
		uint32 TotalChildDraws = 0;
		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			FGPUProfilerEventNode* ChildNode = Node->Children[ChildIndex];

			int32 NumChildDraws = 0;
			// Traverse children
			DumpStatsEventNode(Node->Children[ChildIndex], RootResult, Depth + 1, WildcardFilter, bMatchesFilter, NumNodes, NumChildDraws);
			NumDraws += NumChildDraws;

			TotalChildTime += ChildNode->TimingResult;
			TotalChildDraws += NumChildDraws;
		}

		const float UnaccountedTime = FMath::Max(Node->TimingResult - TotalChildTime, 0.0f);
		const float UnaccountedPercent = UnaccountedTime * 100.0f / (RootResult * 1000.0f);

		// Add an 'Other Children' node if necessary to show time spent in the current node that is not in any of its children
		if (bMatchesFilter && Node->Children.Num() > 0 && TotalChildDraws > 0 && (UnaccountedPercent > 2.0f || UnaccountedTime > .2f))
		{
			UE_LOG(LogRHI, Warning, TEXT("%s%4.1f%%%5.2fms Other Children"), 
				*FString(TEXT("")).LeftPad((EffectiveDepth + 1) * 3), 
				UnaccountedPercent,
				UnaccountedTime);
		}
	}
}

#if !UE_BUILD_SHIPPING

/**
 * Converts GPU profile data to Visualizer data
 *
 * @param InProfileData GPU profile data
 * @param OutVisualizerData Visualizer data
 */
static TSharedPtr< FVisualizerEvent > CreateVisualizerDataRecursively( const TRefCountPtr< class FGPUProfilerEventNode >& InNode, TSharedPtr< FVisualizerEvent > InParentEvent, const double InStartTimeMs, const double InTotalTimeMs )
{
	TSharedPtr< FVisualizerEvent > VisualizerEvent( new FVisualizerEvent( InStartTimeMs / InTotalTimeMs, InNode->TimingResult / InTotalTimeMs, InNode->TimingResult, 0, InNode->Name ) );
	VisualizerEvent->ParentEvent = InParentEvent;

	double ChildStartTimeMs = InStartTimeMs;
	for( int32 ChildIndex = 0; ChildIndex < InNode->Children.Num(); ChildIndex++ )
	{
		TRefCountPtr< FGPUProfilerEventNode > ChildNode = InNode->Children[ ChildIndex ];
		TSharedPtr< FVisualizerEvent > ChildEvent = CreateVisualizerDataRecursively( ChildNode, VisualizerEvent, ChildStartTimeMs, InTotalTimeMs );
		VisualizerEvent->Children.Add( ChildEvent );

		ChildStartTimeMs += ChildNode->TimingResult;
	}

	return VisualizerEvent;
}

/**
 * Converts GPU profile data to Visualizer data
 *
 * @param InProfileData GPU profile data
 * @param OutVisualizerData Visualizer data
 */
static TSharedPtr< FVisualizerEvent > CreateVisualizerData( const TArray<TRefCountPtr<class FGPUProfilerEventNode> >& InProfileData )
{
	// Calculate total time first
	double TotalTimeMs = 0.0;
	for( int32 Index = 0; Index < InProfileData.Num(); ++Index )
	{
		TotalTimeMs += InProfileData[ Index ]->TimingResult;
	}
	
	// Assumption: InProfileData contains only one (root) element. Otherwise an extra FVisualizerEvent root event is required.
	TSharedPtr< FVisualizerEvent > DummyRoot;
	// Recursively create visualizer event data.
	TSharedPtr< FVisualizerEvent > StatEvents( CreateVisualizerDataRecursively( InProfileData[0], DummyRoot, 0.0, TotalTimeMs ) );
	return StatEvents;
}

#endif

void FGPUProfilerEventNodeFrame::DumpEventTree()
{
	if (EventTree.Num() > 0)
	{
		float RootResult = GetRootTimingResults();

		UE_LOG(LogRHI, Warning, TEXT("Perf marker hierarchy, total GPU time %.2fms"), RootResult * 1000.0f);

		// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled
		FText VsyncEnabledWarningText = FText::GetEmpty();
		static IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		if (CVSyncVar->GetInt() != 0 && !PlatformDisablesVSync())
		{
			VsyncEnabledWarningText = LOCTEXT("GpuProfileVsyncEnabledWarning",
				"WARNING: This GPU profile was captured with v-sync enabled.  V-sync wait time may show up in any bucket, and as a result the data in this profile may be skewed. Please profile with v-sync disabled to obtain the most accurate data.");
			UE_LOG(LogRHI, Warning, TEXT("%s"), *(VsyncEnabledWarningText.ToString()));
		}

		LogDisjointQuery();

		TMap<FString, FGPUProfilerEventNodeStats> EventHistogram;
		for (int32 BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			GatherStatsEventNode(EventTree[BaseNodeIndex], 0, EventHistogram);
		}

		static IConsoleVariable* CVar2 = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.Root"));
		FString RootWildcardString = CVar2->GetString(); 
		FWildcardString RootWildcard(RootWildcardString);

		int32 NumNodes = 0;
		int32 NumDraws = 0;
		for (int32 BaseNodeIndex = 0; BaseNodeIndex < EventTree.Num(); BaseNodeIndex++)
		{
			DumpStatsEventNode(EventTree[BaseNodeIndex], RootResult, 0, RootWildcard, false, NumNodes, NumDraws);
		}

		//@todo - calculate overhead instead of hardcoding
		// This .012ms of overhead is based on what Nsight shows as the minimum draw call duration on a 580 GTX, 
		// Which is apparently how long it takes to issue two timing events.
		UE_LOG(LogRHI, Warning, TEXT("Total Nodes %u Draws %u approx overhead %.2fms"), NumNodes, NumDraws, .012f * NumNodes);
		UE_LOG(LogRHI, Warning, TEXT(""));
		UE_LOG(LogRHI, Warning, TEXT(""));

		if (RootWildcardString == TEXT("*"))
		{
			// Sort descending based on node duration
			EventHistogram.ValueSort( FNodeStatsCompare() );

			// Log stats about the node histogram
			UE_LOG(LogRHI, Warning, TEXT("Node histogram %u buckets"), EventHistogram.Num());

			static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.Pattern"));

			// bad: reading on render thread but we don't support ECVF_RenderThreadSafe on strings yet
			// It's very unlikely to cause a problem as the cvar is only changes by the user.
			FString WildcardString = CVar->GetString(); 

			const float ThresholdInMS = 5.0f;

			if(WildcardString == FString(TEXT("*")))
			{
				// disable Wildcard functionality
				WildcardString.Empty();
			}

			if(WildcardString.IsEmpty())
			{
				UE_LOG(LogRHI, Warning, TEXT(" r.ProfileGPU.Pattern = '*' (using threshold of %g ms)"), ThresholdInMS);
			}
			else
			{
				UE_LOG(LogRHI, Warning, TEXT(" r.ProfileGPU.Pattern = '%s' (not using time threshold)"), *WildcardString);
			}

			FWildcardString Wildcard(WildcardString);

			int32 NumNotShown = 0;
			for (TMap<FString, FGPUProfilerEventNodeStats>::TIterator It(EventHistogram); It; ++It)
			{
				const FGPUProfilerEventNodeStats& NodeStats = It.Value();

				bool bDump = NodeStats.TimingResult > RootResult * ThresholdInMS;

				if(!Wildcard.IsEmpty())
				{
					// if a Wildcard string was specified, we want to always dump all entries
					bDump = Wildcard.IsMatch(*It.Key());
				}

				if (bDump)
				{
					UE_LOG(LogRHI, Warning, TEXT("   %.2fms   %s   Events %u   Draws %u"), NodeStats.TimingResult, *It.Key(), NodeStats.NumEvents, NodeStats.NumDraws);
				}
				else
				{
					NumNotShown++;
				}
			}

			UE_LOG(LogRHI, Warning, TEXT("   %u buckets not shown"), NumNotShown);
		}

#if !UE_BUILD_SHIPPING
		// Create and display profile visualizer data
		if (RHIConfig::ShouldShowProfilerAfterProfilingGPU())
		{

		// execute on main thread
			{
				struct FDisplayProfilerVisualizer
				{
					void Thread( TSharedPtr<FVisualizerEvent> InVisualizerData, const FText InVsyncEnabledWarningText )
					{
						static FName TaskGraphModule(TEXT("TaskGraph"));			
						if (FModuleManager::Get().IsModuleLoaded(TaskGraphModule))
						{
							IProfileVisualizerModule& ProfileVisualizer = FModuleManager::GetModuleChecked<IProfileVisualizerModule>(TaskGraphModule);
							// Display a warning if this is a GPU profile and the GPU was profiled with v-sync enabled (otherwise InVsyncEnabledWarningText is empty)
							ProfileVisualizer.DisplayProfileVisualizer( InVisualizerData, TEXT("GPU"), InVsyncEnabledWarningText, FLinearColor::Red );
						}
					}
				} DisplayProfilerVisualizer;

				TSharedPtr<FVisualizerEvent> VisualizerData = CreateVisualizerData( EventTree );

				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DisplayProfilerVisualizer"),
					STAT_FSimpleDelegateGraphTask_DisplayProfilerVisualizer,
					STATGROUP_TaskGraphTasks);

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateRaw(&DisplayProfilerVisualizer, &FDisplayProfilerVisualizer::Thread, VisualizerData, VsyncEnabledWarningText),
					GET_STATID(STAT_FSimpleDelegateGraphTask_DisplayProfilerVisualizer), nullptr, ENamedThreads::GameThread
				);
			}

			
		}
#endif
	}
}

void FGPUProfiler::PushEvent(const TCHAR* Name)
{
	if (bTrackingEvents)
	{
		check(IsInRenderingThread() || IsInRHIThread());
		if (CurrentEventNode)
		{
			// Add to the current node's children
			CurrentEventNode->Children.Add(CreateEventNode(Name, CurrentEventNode));
			CurrentEventNode = CurrentEventNode->Children.Last();
		}
		else
		{
			// Add a new root node to the tree
			CurrentEventNodeFrame->EventTree.Add(CreateEventNode(Name, NULL));
			CurrentEventNode = CurrentEventNodeFrame->EventTree.Last();
		}

		check(CurrentEventNode);
		// Start timing the current node
		CurrentEventNode->StartTiming();
	}
}

void FGPUProfiler::PopEvent()
{
	if (bTrackingEvents)
	{
		check(CurrentEventNode && (IsInRenderingThread() || IsInRHIThread()));
		// Stop timing the current node and move one level up the tree
		CurrentEventNode->StopTiming();
		CurrentEventNode = CurrentEventNode->Parent;
	}
}

/** Whether GPU timing measurements are supported by the driver. */
bool FGPUTiming::GIsSupported = false;

/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
uint64 FGPUTiming::GTimingFrequency = 0;

/** Whether the static variables have been initialized. */
bool FGPUTiming::GAreGlobalsInitialized = false;

#undef LOCTEXT_NAMESPACE