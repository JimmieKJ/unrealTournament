// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParllelFor.h: TaskGraph library
=============================================================================*/

#pragma once
#include "TaskGraphInterfaces.h"
#include "Function.h"

/** 
	*	General purpose parallel for that uses the taskgraph
	*	@param Num; number of calls of Body; Body(0), Body(1)....Body(Num - 1)
	*	@param Body; Function to call from multiple threads
	*	@param bForceSingleThread; Mostly used for testing, if true, run single threaded instead.
	*	Notes: Please add stats around to calls to parallel for and within your lambda as appropriate. Do not clog the task graph with long running tasks or tasks that block.
**/
void ParallelFor(int32 Num, TFunctionRef<void(int32)> Body, bool bForceSingleThread = false)
{
	// struct to hold the working data; this outlives the ParallelFor call; lifetime is controlled by a shared pointer
	struct FParallelForData
	{
		int32 Num;
		TFunctionRef<void(int32)> Body;
		FScopedEvent& DoneEvent;
		FThreadSafeCounter IndexToDo;
		FThreadSafeCounter NumCompleted;
		FParallelForData(int32 InNum, TFunctionRef<void(int32)> InBody, FScopedEvent& InDoneEvent)
			: Num(InNum)
			, Body(InBody)
			, DoneEvent(InDoneEvent)
		{
		}
		void Process()
		{
			while (true)
			{
				int32 MyIndex = IndexToDo.Increment() - 1;
				if (MyIndex < Num)
				{
					Body(MyIndex);
					if (NumCompleted.Increment() == Num)
					{
						DoneEvent.Trigger(); // I was the last one; let parallelfor exit
					}
				}
				else
				{
					break;
				}
			}
		}
	};
	class FParallelForTask
	{
		TSharedRef<FParallelForData, ESPMode::ThreadSafe> Data;
	public:
		FParallelForTask(TSharedRef<FParallelForData, ESPMode::ThreadSafe>& InData)
			: Data(InData) 
		{
		}
		static FORCEINLINE TStatId GetStatId()
		{
			return GET_STATID(STAT_ParallelForTask);
		}
		static FORCEINLINE ENamedThreads::Type GetDesiredThread()
		{
			return ENamedThreads::AnyThread;
		}
		static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode() 
		{ 
			return ESubsequentsMode::FireAndForget; 
		}
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			Data->Process();
		}
	};

	SCOPE_CYCLE_COUNTER(STAT_ParallelFor);
	check(Num >= 0);

	int32 AnyThreadTasks = 0;
	if (Num > 1 && !bForceSingleThread && FApp::ShouldUseThreadingForPerformance())
	{
		AnyThreadTasks = FMath::Min<int32>(FTaskGraphInterface::Get().GetNumWorkerThreads(), Num - 1);
	}
	if (!AnyThreadTasks)
	{
		// no threads, just do it and return
		for (int32 Index = 0; Index < Num; Index++)
		{
			Body(Index);
		}
		return;
	}

	FScopedEvent DoneEvent;
	TSharedRef<FParallelForData, ESPMode::ThreadSafe> Data = MakeShareable(new FParallelForData(Num, Body, DoneEvent));
	for (int32 Task = 0; Task < AnyThreadTasks; Task++)
	{
		TGraphTask<FParallelForTask>::CreateTask().ConstructAndDispatchWhenReady(Data);		
	}
	// this thread can help too and this is important to prevent deadlock on recursion 
	Data->Process();
	// DoneEvent waits here if some other thread finishes the last item
	// Data must live on until all of the tasks are cleared which might be long after this function exits
}


