// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WorldCollisionAsync.cpp: UWorld async collision implementation
=============================================================================*/

#include "EnginePrivate.h"
#include "WorldCollision.h"

#if WITH_PHYSX
	#include "../PhysicsEngine/PhysXSupport.h"
	#include "PhysXCollision.h"
#endif

/**
 * Async trace functions
 * Pretty much same parameter set except you can optional set delegate to be called when execution is completed and you can set UserData if you'd like
 * if no delegate, you can query trace data using QueryTraceData or QueryOverlapData
 * the data is available only in the next frame after request is made - in other words, if request is made in frame X, you can get the result in frame (X+1)
 *
 * @param InDeleagte	Delegate function to be called - to see example, search FTraceDelegate
 *						Example can be void MyActor::TraceDone(const FTraceHandle& TraceHandle, FTraceDatum & TraceData)
 *						Before sending to the function, 
 *						
 *						FTraceDelegate TraceDelegate;
 *						TraceDelegate.BindRaw(this, &MyActor::TraceDone);
 * 
 * @param UserData		UserData
 */

#define RUN_ASYNC_TRACE 1

namespace
{
	// Helper functions to return the right named member container based on a datum type
	template <typename DatumType> FORCEINLINE TArray<TUniquePtr<TTraceThreadData<DatumType    >>>& GetTraceContainer               (AsyncTraceData& DataBuffer);
	template <>                   FORCEINLINE TArray<TUniquePtr<TTraceThreadData<FTraceDatum  >>>& GetTraceContainer<FTraceDatum>  (AsyncTraceData& DataBuffer) { return DataBuffer.TraceData;   }
	template <>                   FORCEINLINE TArray<TUniquePtr<TTraceThreadData<FOverlapDatum>>>& GetTraceContainer<FOverlapDatum>(AsyncTraceData& DataBuffer) { return DataBuffer.OverlapData; }

	// Helper functions to return the right named member trace index based on a datum type
	template <typename DatumType> FORCEINLINE int32& GetTraceIndex               (FWorldAsyncTraceState& State);
	template <>                   FORCEINLINE int32& GetTraceIndex<FTraceDatum>  (FWorldAsyncTraceState& State) { return State.NextAvailableTraceIndex;   }
	template <>                   FORCEINLINE int32& GetTraceIndex<FOverlapDatum>(FWorldAsyncTraceState& State) { return State.NextAvailableOverlapIndex; }

	/** For referencing a thread data buffer and a datum within it */
	struct FBufferIndexPair
	{
		int32 Block;
		int32 Index;

		explicit FBufferIndexPair(int32 InVal)
			: Block(InVal / ASYNC_TRACE_BUFFER_SIZE)
			, Index(InVal % ASYNC_TRACE_BUFFER_SIZE)
		{
		}

		FBufferIndexPair(int32 InBlock, int32 InIndex)
			: Block(InBlock)
			, Index(InIndex)
		{
		}

		template <typename DatumType>
		DatumType* DatumLookup(TArray<TUniquePtr<TTraceThreadData<DatumType>>>& Array) const
		{
			// if not valid index, return
			if (!Array.IsValidIndex(Block))
			{
				return NULL;
			}

			if (Index < 0 || Index >= ASYNC_TRACE_BUFFER_SIZE)
			{
				return NULL;
			}

			return &Array[Block]->Buffer[Index];
		}

		template <typename DatumType>
		FORCEINLINE DatumType& DatumLookupChecked(TArray<TUniquePtr<TTraceThreadData<DatumType>>>& Array) const
		{
			check(Index >= 0 && Index < ASYNC_TRACE_BUFFER_SIZE);
			return Array[Block]->Buffer[Index];
		}
	};

	void RunTraceTask(FTraceDatum* TraceDataBuffer, int32 TotalCount)
	{
#if UE_WITH_PHYSICS
		check(TraceDataBuffer);

		for (; TotalCount; --TotalCount)
		{
			FTraceDatum& TraceData = *TraceDataBuffer++;
			TraceData.OutHits.Empty();

			if (TraceData.PhysWorld.IsValid())
			{
				if ((TraceData.CollisionParams.CollisionShape.ShapeType == ECollisionShape::Line) || TraceData.CollisionParams.CollisionShape.IsNearlyZero())
				{
					if (TraceData.bIsMultiTrace)
					{
						RaycastMulti(TraceData.PhysWorld.Get(), TraceData.OutHits, TraceData.Start, TraceData.End, TraceData.TraceChannel,
							TraceData.CollisionParams.CollisionQueryParam, TraceData.CollisionParams.ResponseParam, TraceData.CollisionParams.ObjectQueryParam);
					}
					else
					{
						TraceData.OutHits.AddZeroed(1);
						RaycastSingle(TraceData.PhysWorld.Get(), TraceData.OutHits[0], TraceData.Start, TraceData.End, TraceData.TraceChannel,
							TraceData.CollisionParams.CollisionQueryParam, TraceData.CollisionParams.ResponseParam, TraceData.CollisionParams.ObjectQueryParam);
					}
				}
				else
				{
					if (TraceData.bIsMultiTrace)
					{
						GeomSweepMulti(TraceData.PhysWorld.Get(), TraceData.CollisionParams.CollisionShape, FQuat::Identity, TraceData.OutHits, TraceData.Start, TraceData.End, TraceData.TraceChannel,
							TraceData.CollisionParams.CollisionQueryParam, TraceData.CollisionParams.ResponseParam, TraceData.CollisionParams.ObjectQueryParam);
					}
					else
					{
						TraceData.OutHits.AddZeroed(1);
						GeomSweepSingle(TraceData.PhysWorld.Get(), TraceData.CollisionParams.CollisionShape, FQuat::Identity, TraceData.OutHits[0], TraceData.Start, TraceData.End, TraceData.TraceChannel,
							TraceData.CollisionParams.CollisionQueryParam, TraceData.CollisionParams.ResponseParam, TraceData.CollisionParams.ObjectQueryParam);
					}
				}
			}
		}
	#endif //UE_WITH_PHYSICS
	}

	void RunTraceTask(FOverlapDatum* OverlapDataBuffer, int32 TotalCount)
	{
	#if UE_WITH_PHYSICS
		check(OverlapDataBuffer);

		for (; TotalCount; --TotalCount)
		{
			FOverlapDatum& OverlapData = *OverlapDataBuffer++;
			OverlapData.OutOverlaps.Empty();

			if (!OverlapData.PhysWorld.IsValid())
			{
				continue;
			}

			GeomOverlapMulti(
				OverlapData.PhysWorld.Get(),
				OverlapData.CollisionParams.CollisionShape,
				OverlapData.Pos,
				OverlapData.Rot,
				OverlapData.OutOverlaps,
				OverlapData.TraceChannel,
				OverlapData.CollisionParams.CollisionQueryParam,
				OverlapData.CollisionParams.ResponseParam,
				OverlapData.CollisionParams.ObjectQueryParam);
		}
	#endif //UE_WITH_PHYSICS
	}

	#if RUN_ASYNC_TRACE

	/** Helper class define the task of Async Trace running**/
	class FAsyncTraceTask
	{
		// this accepts either trace or overlap data array
		// don't use both of them, it won't work
		FTraceDatum*   TraceData;
		FOverlapDatum* OverlapData;

		// data count
		int32 DataCount;

	public:
		FAsyncTraceTask(FTraceDatum* InTraceData, int32 InDataCount)
		{
			check(InTraceData);
			check(InDataCount > 0);

			TraceData   = InTraceData;
			OverlapData = NULL;
			DataCount   = InDataCount;
		}

		FAsyncTraceTask(FOverlapDatum* InOverlapData, int32 InDataCount)
		{
			check(InOverlapData);
			check(InDataCount > 0);

			TraceData   = NULL;
			OverlapData = InOverlapData;
			DataCount   = InDataCount;
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncTraceTask, STATGROUP_TaskGraphTasks);
		}
		/** return the thread for this task **/
		static ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::AnyThread;
		}
		static ESubsequentsMode::Type GetSubsequentsMode() 
		{ 
			return ESubsequentsMode::TrackSubsequents; 
		}
		/** 
		 *	Actually execute the tick.
		 *	@param	CurrentThread; the thread we are running on
		 *	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite. 
		 *	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
		 **/
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			if (TraceData)
			{
				RunTraceTask(TraceData, DataCount);
			}
			else if (OverlapData)
			{
				RunTraceTask(OverlapData, DataCount);
			}
		}
	};
	#endif // RUN_ASYNC_TRACE

	// This runs each chunk whenever filled up to GAsyncChunkSizeToIncrement OR when ExecuteAll is true
	template <typename DatumType>
	void ExecuteAsyncTraceIfAvailable(FWorldAsyncTraceState& State, bool bExecuteAll)
	{
		FBufferIndexPair Next(GetTraceIndex<DatumType>(State));

		// when Next.Index == 0, and Next.Block > 0 , that means next one will be in the next buffer
		// but that case we'd like to send to thread
		if (Next.Index == 0 && Next.Block > 0)
		{
			Next.Block = Next.Block - 1;
			Next.Index = ASYNC_TRACE_BUFFER_SIZE;
		}
		// don't execute if we haven't been explicitly requested to OR there's nothing to run
		else if (!bExecuteAll || Next.Index == 0)
		{
			return;
		}

		auto& DataBuffer = State.GetBufferForCurrentFrame();
		auto* Datum      = GetTraceContainer<DatumType>(DataBuffer)[Next.Block]->Buffer;
		#if RUN_ASYNC_TRACE
			DataBuffer.AsyncTraceCompletionEvent.Emplace(TGraphTask<FAsyncTraceTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(Datum, Next.Index));
		#else
			RunTraceTask(Datum, Next.Index);
		#endif
	}

	template <typename DatumType>
	FTraceHandle StartNewTrace(FWorldAsyncTraceState& State, const DatumType& Val)
	{
		// Get the buffer for the current frame
		AsyncTraceData& DataBuffer = State.GetBufferForCurrentFrame();

		// Check we're allowed to do an async call here
		check(DataBuffer.bAsyncAllowed);

		auto&  TraceData  = GetTraceContainer<DatumType>(DataBuffer);
		int32& TraceIndex = GetTraceIndex    <DatumType>(State);

		// we calculate index as continuous, not as each chunk, but continuous way 
		int32 LastAvailableIndex = TraceData.Num() * ASYNC_TRACE_BUFFER_SIZE;

		// if smaller than next available index
		if (LastAvailableIndex <= TraceIndex)
		{
			// add one more buffer
			TraceData.Add(MakeUnique<TTraceThreadData<DatumType>>());
		}

		FTraceHandle Result(State.CurrentFrame, TraceIndex);
		FBufferIndexPair(TraceIndex).DatumLookupChecked(TraceData) = MoveTemp(Val);

		ExecuteAsyncTraceIfAvailable<DatumType>(State, false);

		++TraceIndex;

		return Result;
	}
}

FWorldAsyncTraceState::FWorldAsyncTraceState()
	: CurrentFrame             (0)
	, NextAvailableTraceIndex  (0)
	, NextAvailableOverlapIndex(0)
{
}

FTraceHandle UWorld::AsyncLineTraceByChannel(const FVector& Start,const FVector& End, ECollisionChannel TraceChannel, const FCollisionQueryParams& Params /* = FCollisionQueryParams::DefaultQueryParam */, const FCollisionResponseParams& ResponseParam /* = FCollisionResponseParams::DefaultResponseParam */, FTraceDelegate * InDelegate/* =NULL */, uint32 UserData /* = 0 */, bool bMultiTrace/* =false */ )
{
	return StartNewTrace(AsyncTraceState, FTraceDatum(this, FCollisionShape::LineShape, Params, ResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam, TraceChannel, UserData, bMultiTrace, Start, End, InDelegate, AsyncTraceState.CurrentFrame));
}

FTraceHandle UWorld::AsyncLineTraceByObjectType(const FVector& Start,const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionQueryParams& Params /* = FCollisionQueryParams::DefaultQueryParam */, FTraceDelegate * InDelegate/* =NULL */, uint32 UserData /* = 0 */, bool bMultiTrace/* =false */ )
{
	return StartNewTrace(AsyncTraceState, FTraceDatum(this, FCollisionShape::LineShape, Params, FCollisionResponseParams::DefaultResponseParam, ObjectQueryParams, DefaultCollisionChannel, UserData, bMultiTrace, Start, End, InDelegate, AsyncTraceState.CurrentFrame));
}

FTraceHandle UWorld::AsyncSweepByChannel(const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params /* = FCollisionQueryParams::DefaultQueryParam */, const FCollisionResponseParams& ResponseParam /* = FCollisionResponseParams::DefaultResponseParam */, FTraceDelegate * InDelegate /* = NULL */, uint32 UserData /* = 0 */, bool bMultiTrace /* = false */)
{
	return StartNewTrace(AsyncTraceState, FTraceDatum(this, CollisionShape, Params, ResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam, TraceChannel, UserData, bMultiTrace, Start, End, InDelegate, AsyncTraceState.CurrentFrame));
}

FTraceHandle UWorld::AsyncSweepByObjectType(const FVector& Start, const FVector& End, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params /* = FCollisionQueryParams::DefaultQueryParam */, FTraceDelegate * InDelegate /* = NULL */, uint32 UserData /* = 0 */, bool bMultiTrace /* = false */)
{
	return StartNewTrace(AsyncTraceState, FTraceDatum(this, CollisionShape, Params, FCollisionResponseParams::DefaultResponseParam, ObjectQueryParams, DefaultCollisionChannel, UserData, bMultiTrace, Start, End, InDelegate, AsyncTraceState.CurrentFrame));
}

// overlap functions
FTraceHandle UWorld::AsyncOverlapByChannel(const FVector& Pos, const FQuat& Rot, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params /* = FCollisionQueryParams::DefaultQueryParam */, const FCollisionResponseParams& ResponseParam /* = FCollisionResponseParams::DefaultResponseParam */, FOverlapDelegate * InDelegate /* = NULL */, uint32 UserData /* = 0 */)
{
	return StartNewTrace(AsyncTraceState, FOverlapDatum(this, CollisionShape, Params, ResponseParam, FCollisionObjectQueryParams::DefaultObjectQueryParam, TraceChannel, UserData, Pos, Rot, InDelegate, AsyncTraceState.CurrentFrame));
}

FTraceHandle UWorld::AsyncOverlapByObjectType(const FVector& Pos, const FQuat& Rot, const FCollisionObjectQueryParams& ObjectQueryParams, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params /* = FCollisionQueryParams::DefaultQueryParam */, FOverlapDelegate * InDelegate /* = NULL */, uint32 UserData /* = 0 */)
{
	return StartNewTrace(AsyncTraceState, FOverlapDatum(this, CollisionShape, Params, FCollisionResponseParams::DefaultResponseParam, ObjectQueryParams, DefaultCollisionChannel, UserData, Pos, Rot, InDelegate, AsyncTraceState.CurrentFrame));
}

bool UWorld::IsTraceHandleValid(const FTraceHandle& Handle, bool bOverlapTrace)
{
	// only valid if it's previous frame or current frame
	if (Handle._Data.FrameNumber != AsyncTraceState.CurrentFrame - 1 && Handle._Data.FrameNumber != AsyncTraceState.CurrentFrame)
	{
		return false;
	}

	// make sure it has valid index
	AsyncTraceData& DataBuffer = AsyncTraceState.GetBufferForFrame(Handle._Data.FrameNumber);

	// this function basically verifies if the address location 
	// is VALID, not necessarily that location was USED in that frame
	FBufferIndexPair Loc(Handle._Data.Index);
	if (bOverlapTrace)
	{
		return !!Loc.DatumLookup(DataBuffer.OverlapData);
	}
	else
	{
		return !!Loc.DatumLookup(DataBuffer.TraceData);
	}
}

bool UWorld::QueryTraceData(const FTraceHandle& Handle, FTraceDatum& OutData)
{
	// valid if previous frame request
	if (Handle._Data.FrameNumber != AsyncTraceState.CurrentFrame - 1)
	{
		return false;
	}

	AsyncTraceData& DataBuffer = AsyncTraceState.GetBufferForPreviousFrame();
	if (auto* Data = FBufferIndexPair(Handle._Data.Index).DatumLookup(DataBuffer.TraceData))
	{
		OutData = *Data;
		return true;
	}

	return false;
}

bool UWorld::QueryOverlapData(const FTraceHandle& Handle, FOverlapDatum& OutData)
{
	if (Handle._Data.FrameNumber != AsyncTraceState.CurrentFrame - 1)
	{
		return false;
	}

	AsyncTraceData& DataBuffer = AsyncTraceState.GetBufferForPreviousFrame();
	if (auto* Data = FBufferIndexPair(Handle._Data.Index).DatumLookup(DataBuffer.OverlapData))
	{
		OutData = *Data;
		return true;
	}

	return false;
}

void UWorld::WaitForAllAsyncTraceTasks()
{
#if RUN_ASYNC_TRACE
	// if running thread, wait until all threads finishes, if we don't do this, there might be more thread running
	AsyncTraceData& DataBufferExecuted = AsyncTraceState.GetBufferForPreviousFrame();
	if (DataBufferExecuted.AsyncTraceCompletionEvent.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitForAllAsyncTraceTasks);
		FTaskGraphInterface::Get().WaitUntilTasksComplete(DataBufferExecuted.AsyncTraceCompletionEvent,ENamedThreads::GameThread);
		DataBufferExecuted.AsyncTraceCompletionEvent.Reset();
	}
#endif // RUN_ASYNC_TRACE
}

void UWorld::ResetAsyncTrace()
{
	AsyncTraceData& DataBufferExecuted = AsyncTraceState.GetBufferForPreviousFrame();

	// Wait for thread
	WaitForAllAsyncTraceTasks();

	// do run delegates before starting next round
	for (int32 Idx = 0; Idx != AsyncTraceState.NextAvailableTraceIndex; ++Idx)
	{
		auto& TraceData = FBufferIndexPair(Idx).DatumLookupChecked(DataBufferExecuted.TraceData);
		TraceData.Delegate.ExecuteIfBound(FTraceHandle(TraceData.FrameNumber, Idx), TraceData);
	}
	for (int32 Idx = 0; Idx != AsyncTraceState.NextAvailableOverlapIndex; ++Idx)
	{
		auto& TraceData = FBufferIndexPair(Idx).DatumLookupChecked(DataBufferExecuted.OverlapData);
		TraceData.Delegate.ExecuteIfBound(FTraceHandle(TraceData.FrameNumber, Idx), TraceData);
	}

	// re-initialize all variables
	AsyncTraceState.GetBufferForCurrentFrame().bAsyncAllowed = true;
	AsyncTraceState.NextAvailableTraceIndex   = 0;
	AsyncTraceState.NextAvailableOverlapIndex = 0;
}

void UWorld::FinishAsyncTrace()
{
	// execute all remainder
	ExecuteAsyncTraceIfAvailable<FTraceDatum>  (AsyncTraceState, true);
	ExecuteAsyncTraceIfAvailable<FOverlapDatum>(AsyncTraceState, true);

	// this flag only needed to know I can't accept any more new request in current frame
	AsyncTraceState.GetBufferForCurrentFrame().bAsyncAllowed = false;

	// increase buffer index to next one
	++AsyncTraceState.CurrentFrame;
}
