// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "RHI.h"
#include "SceneUtils.h"


#if HAS_GPU_STATS

static TAutoConsoleVariable<int> CVarGPUStatsEnabled(
	TEXT("r.GPUStatsEnabled"),
	0,
	TEXT("Enables or disables GPU stat recording"));

// If this is enabled, the child stat timings will be included in their parents' times.
// This presents problems for non-hierarchical stats if we're expecting them to add up
// to the total GPU time, so we probably want this disabled
#define GPU_STATS_CHILD_TIMES_INCLUDED 0

DECLARE_FLOAT_COUNTER_STAT(TEXT("[TOTAL SCENE]"), Stat_GPU_Total, STATGROUP_GPU);

#endif //HAS_GPU_STATS


#if WANTS_DRAW_MESH_EVENTS

template<typename TRHICmdList>
void TDrawEvent<TRHICmdList>::Start(TRHICmdList& InRHICmdList, FColor Color, const TCHAR* Fmt, ...)
{
	check(IsInParallelRenderingThread() || IsInRHIThread());
	{
		va_list ptr;
		va_start(ptr, Fmt);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, Fmt, ptr);
		InRHICmdList.PushEvent(TempStr, Color);
		RHICmdList = &InRHICmdList;
	}
}

template<typename TRHICmdList>
void TDrawEvent<TRHICmdList>::Stop()
{
	if (RHICmdList)
	{
		RHICmdList->PopEvent();
		RHICmdList = NULL;
	}
}
template struct TDrawEvent<FRHICommandList>;
template struct TDrawEvent<FRHIAsyncComputeCommandList>;

void FDrawEventRHIExecute::Start(IRHIComputeContext& InRHICommandContext, FColor Color, const TCHAR* Fmt, ...)
{
	check(IsInParallelRenderingThread() || IsInRHIThread() || (!GRHIThread && IsInRenderingThread()));
	{
		va_list ptr;
		va_start(ptr, Fmt);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, Fmt, ptr);
		RHICommandContext = &InRHICommandContext;
		RHICommandContext->RHIPushEvent(TempStr, Color);
	}
}

void FDrawEventRHIExecute::Stop()
{
	RHICommandContext->RHIPopEvent();
}

#endif // WANTS_DRAW_MESH_EVENTS


bool IsMobileHDR()
{
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	return MobileHDRCvar->GetValueOnAnyThread() == 1;
}

bool IsMobileHDR32bpp()
{
	static auto* MobileHDR32bppModeCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bppMode"));
	return IsMobileHDR() && (GSupportsRenderTargetFormat_PF_FloatRGBA == false || MobileHDR32bppModeCvar->GetValueOnRenderThread() != 0);
}

bool IsMobileHDRMosaic()
{
	if (!IsMobileHDR32bpp())
		return false;

	static auto* MobileHDR32bppMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bppMode"));
	switch (MobileHDR32bppMode->GetValueOnRenderThread())
	{
		case 1:
			return true;
		case 2:
			return false;
		default:
			return !(GSupportsHDR32bppEncodeModeIntrinsic && GSupportsShaderFramebufferFetch);
	}
}

#if HAS_GPU_STATS
static const int32 NumGPUProfilerBufferedFrames = 4;

/*-----------------------------------------------------------------------------
FRealTimeGPUProfilerEvent class
-----------------------------------------------------------------------------*/
class FRealtimeGPUProfilerEvent
{
public:
	static const uint64 InvalidQueryResult = 0xFFFFFFFFFFFFFFFFull;

public:
	FRealtimeGPUProfilerEvent(const TStatId& InStatId)
		: StartResultMicroseconds(InvalidQueryResult)
		, EndResultMicroseconds(InvalidQueryResult)
		, FrameNumber(-1)
		, bInsideQuery(false)
	{
		StatName = InStatId.GetName();
	}

	void Begin(FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());
		check(!bInsideQuery);
		bInsideQuery = true;

		if (!StartQuery)
		{
			StartQuery = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);
			EndQuery = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);
		}

		RHICmdList.EndRenderQuery(StartQuery);
		StartResultMicroseconds = InvalidQueryResult;
		EndResultMicroseconds = InvalidQueryResult;
		FrameNumber = GFrameNumberRenderThread;
	}

	void End(FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());
		check(bInsideQuery);
		bInsideQuery = false;
		RHICmdList.EndRenderQuery(EndQuery);
	}

	bool GatherQueryResults(FRHICommandListImmediate& RHICmdList)
	{
		// Get the query results which are still outstanding
		if (StartResultMicroseconds == InvalidQueryResult)
		{
			if (!RHICmdList.GetRenderQueryResult(StartQuery, StartResultMicroseconds, false))
			{
				StartResultMicroseconds = InvalidQueryResult;
			}
		}
		if (EndResultMicroseconds == InvalidQueryResult)
		{
			if (!RHICmdList.GetRenderQueryResult(EndQuery, EndResultMicroseconds, false))
			{
				EndResultMicroseconds = InvalidQueryResult;
			}
		}
		return HasValidResult();
	}

	float GetResultMS() const
	{
		check(HasValidResult());
		if (EndResultMicroseconds < StartResultMicroseconds)
		{
			// This should never happen...
			return 0.0f;
		}
		return float(EndResultMicroseconds - StartResultMicroseconds) / 1000.0f;
	}

	bool HasValidResult() const
	{
		return StartResultMicroseconds != FRealtimeGPUProfilerEvent::InvalidQueryResult && EndResultMicroseconds != FRealtimeGPUProfilerEvent::InvalidQueryResult;
	}

	const FName& GetStatName() const
	{
		return StatName;
	}

private:
	FRenderQueryRHIRef StartQuery;
	FRenderQueryRHIRef EndQuery;
	FName StatName;
	uint64 StartResultMicroseconds;
	uint64 EndResultMicroseconds;
	uint32 FrameNumber;

	bool bInsideQuery;
};

/*-----------------------------------------------------------------------------
FRealtimeGPUProfilerFrame class
Container for a single frame's GPU stats
-----------------------------------------------------------------------------*/
class FRealtimeGPUProfilerFrame
{
public:
	FRealtimeGPUProfilerFrame()
		: FrameNumber(-1)
	{}

	~FRealtimeGPUProfilerFrame()
	{
		Clear();
	}

	void PushEvent(FRHICommandListImmediate& RHICmdList, TStatId StatId)
	{
#if GPU_STATS_CHILD_TIMES_INCLUDED == 0
		if (EventStack.Num() > 0)
		{
			// GPU Stats are not hierarchical. If we already have an event in the stack, 
			// we need end it and resume it once the child event completes 
			FRealtimeGPUProfilerEvent* ParentEvent = EventStack.Last();
			ParentEvent->End(RHICmdList);
		}
#endif
		FRealtimeGPUProfilerEvent* Event = CreateNewEvent(StatId);
		EventStack.Push(Event);
		StatStack.Push(StatId);
		Event->Begin(RHICmdList);
	}

	void PopEvent(FRHICommandListImmediate& RHICmdList)
	{
		FRealtimeGPUProfilerEvent* Event = EventStack.Pop();
		StatStack.Pop();
		Event->End(RHICmdList);

#if GPU_STATS_CHILD_TIMES_INCLUDED == 0
		if (EventStack.Num() > 0)
		{
			// Resume the parent event (requires creation of a new FRealtimeGPUProfilerEvent)
			TStatId PrevStatId = StatStack.Last();
			FRealtimeGPUProfilerEvent* ResumedEvent = CreateNewEvent(PrevStatId);
			EventStack.Last() = ResumedEvent;
			ResumedEvent->Begin(RHICmdList);
		}
#endif

	}

	void Clear()
	{
		EventStack.Empty();
		StatStack.Empty();

		for (int Index = 0; Index < GpuProfilerEvents.Num(); Index++)
		{
			if (GpuProfilerEvents[Index])
			{
				delete GpuProfilerEvents[Index];
			}
		}
		GpuProfilerEvents.Empty();
	}

	bool UpdateStats(FRHICommandListImmediate& RHICmdList)
	{
		// Gather any remaining results and check all the results are ready
		for (int Index = 0; Index < GpuProfilerEvents.Num(); Index++)
		{
			FRealtimeGPUProfilerEvent* Event = GpuProfilerEvents[Index];
			check(Event != nullptr);
			if (!Event->HasValidResult())
			{
				Event->GatherQueryResults(RHICmdList);
			}
			if (!Event->HasValidResult())
			{
				// The frame isn't ready yet. Don't update stats - we'll try again next frame. 
				return false;
			}
		}

		float TotalMS = 0.0f;
		// Update the stats
		TMap<FName, bool> StatSeenMap;
		for (int Index = 0; Index < GpuProfilerEvents.Num(); Index++)
		{
			FRealtimeGPUProfilerEvent* Event = GpuProfilerEvents[Index];
			check(Event != nullptr);
			check(Event->HasValidResult());
			EStatOperation::Type StatOp;
			const FName& StatName = Event->GetStatName();

			// Check if we've seen this stat yet 
			if (StatSeenMap.Find(StatName) == nullptr)
			{
				StatSeenMap.Add(StatName, true);
				StatOp = EStatOperation::Set;
			}
			else
			{
				// Stat was seen before, so accumulate 
				StatOp = EStatOperation::Add;
			}
			float ResultMS = Event->GetResultMS();
			FThreadStats::AddMessage(StatName, StatOp, double(ResultMS));
			TotalMS += ResultMS;
		}

		FThreadStats::AddMessage( GET_STATFNAME(Stat_GPU_Total), EStatOperation::Set, double(TotalMS) );
		return true;
	}

private:
	FRealtimeGPUProfilerEvent* CreateNewEvent(const TStatId& StatId)
	{
		FRealtimeGPUProfilerEvent* NewEvent = new FRealtimeGPUProfilerEvent(StatId);
		GpuProfilerEvents.Add(NewEvent);
		return NewEvent;
	}

	TArray<FRealtimeGPUProfilerEvent*> GpuProfilerEvents;
	TArray<FRealtimeGPUProfilerEvent*> EventStack;
	TArray<TStatId> StatStack;
	uint32 FrameNumber;
};

/*-----------------------------------------------------------------------------
FRealtimeGPUProfiler
-----------------------------------------------------------------------------*/
FRealtimeGPUProfiler* FRealtimeGPUProfiler::Instance = nullptr;

FRealtimeGPUProfiler* FRealtimeGPUProfiler::Get()
{
	if (Instance == nullptr)
	{
		Instance = new FRealtimeGPUProfiler;
	}
	return Instance;
}

FRealtimeGPUProfiler::FRealtimeGPUProfiler()
	: WriteBufferIndex(-1)
	, ReadBufferIndex(-1)
	, WriteFrameNumber(-1)
{
	for (int Index = 0; Index < NumGPUProfilerBufferedFrames; Index++)
	{
		Frames.Add(new FRealtimeGPUProfilerFrame());
	}
}

void FRealtimeGPUProfiler::Release()
{
	for (int Index = 0; Index < Frames.Num(); Index++)
	{
		delete Frames[Index];
	}
	Frames.Empty();
}

void FRealtimeGPUProfiler::Update(FRHICommandListImmediate& RHICmdList)
{
	check(Frames.Num() > 0);
	if (WriteFrameNumber == GFrameNumberRenderThread)
	{
		// Multiple views: only update if this is a new frame. Otherwise we just concatenate the stats
		return;
	}
	WriteFrameNumber = GFrameNumberRenderThread;
	WriteBufferIndex = (WriteBufferIndex + 1) % Frames.Num();
	Frames[WriteBufferIndex]->Clear();

	// If the write buffer catches the read buffer, we need to advance the read buffer before we write over it 
	if (WriteBufferIndex == ReadBufferIndex)
	{
		ReadBufferIndex = (ReadBufferIndex - 1) % Frames.Num();
	}
	UpdateStats(RHICmdList);
}

void FRealtimeGPUProfiler::PushEvent(FRHICommandListImmediate& RHICmdList, TStatId StatId)
{
	check(Frames.Num() > 0);
	if (WriteBufferIndex >= 0)
	{
		Frames[WriteBufferIndex]->PushEvent(RHICmdList, StatId);
	}
}

void FRealtimeGPUProfiler::PopEvent(FRHICommandListImmediate& RHICmdList)
{
	check(Frames.Num() > 0);
	if (WriteBufferIndex >= 0)
	{
		Frames[WriteBufferIndex]->PopEvent(RHICmdList);
	}
}

void FRealtimeGPUProfiler::UpdateStats(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());
	if (GSupportsTimestampRenderQueries == false || !CVarGPUStatsEnabled.GetValueOnRenderThread() )
	{
		return;
	}
	check(Frames.Num() > 0);
	bool bAdvanceReadIndex = false;
	if (ReadBufferIndex == -1)
	{
		bAdvanceReadIndex = true;
	}
	else
	{
		// If the read frame is valid, update the stats
		if (Frames[ReadBufferIndex]->UpdateStats(RHICmdList))
		{
			// On a successful read, advance the ReadBufferIndex
			bAdvanceReadIndex = true;
		}
	}
	if (bAdvanceReadIndex)
	{
		ReadBufferIndex = (ReadBufferIndex + 1) % Frames.Num();
	}
}

/*-----------------------------------------------------------------------------
FScopedGPUStatEvent
-----------------------------------------------------------------------------*/
void FScopedGPUStatEvent::Begin(FRHICommandList& InRHICmdList, TStatId InStatID)
{
	check(IsInRenderingThread());
	if (GSupportsTimestampRenderQueries == false || !CVarGPUStatsEnabled.GetValueOnRenderThread())
	{
		return;
	}

	// Non-immediate commandlists are not supported (silently fail)
	if (InRHICmdList.IsImmediate())
	{
		RHICmdList = (FRHICommandListImmediate*)&InRHICmdList;
		FRealtimeGPUProfiler::Get()->PushEvent(*RHICmdList, InStatID);
	}
}

void FScopedGPUStatEvent::End()
{
	check(IsInRenderingThread());
	if (GSupportsTimestampRenderQueries == false || !CVarGPUStatsEnabled.GetValueOnRenderThread() )
	{
		return;
	}
	if (RHICmdList != nullptr)
	{
		FRealtimeGPUProfiler::Get()->PopEvent(*RHICmdList);
	}
}
#endif // HAS_GPU_STATS
