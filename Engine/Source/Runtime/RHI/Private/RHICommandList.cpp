// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "RHI.h"
#include "RHICommandList.h"

DECLARE_CYCLE_STAT(TEXT("Nonimmed. Command List Execute"), STAT_NonImmedCmdListExecuteTime, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Nonimmed. Command List memory"), STAT_NonImmedCmdListMemory, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Nonimmed. Command count"), STAT_NonImmedCmdListCount, STATGROUP_RHICMDLIST);

DECLARE_CYCLE_STAT(TEXT("All Command List Execute"), STAT_ImmedCmdListExecuteTime, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Immed. Command List memory"), STAT_ImmedCmdListMemory, STATGROUP_RHICMDLIST);
DECLARE_DWORD_COUNTER_STAT(TEXT("Immed. Command count"), STAT_ImmedCmdListCount, STATGROUP_RHICMDLIST);

#if !PLATFORM_USES_FIXED_RHI_CLASS
#include "RHICommandListCommandExecutes.inl"
#endif


static TAutoConsoleVariable<int32> CVarRHICmdBypass(
	TEXT("r.RHICmdBypass"),
	FRHICommandListExecutor::DefaultBypass,
	TEXT("Whether to bypass the rhi command list and send the rhi commands immediately.\n")
	TEXT("0: Disable (required for the multithreaded renderer)\n")
	TEXT("1: Enable (convenient for debugging low level graphics API calls, can supress artifacts from multithreaded renderer code)"));

static TAutoConsoleVariable<int32> CVarRHICmdUseParallelAlgorithms(
	TEXT("r.RHICmdUseParallelAlgorithms"),
	1,
	TEXT("True to use parallel algorithms. Ignored if r.RHICmdBypass is 1."));

TAutoConsoleVariable<int32> CVarRHICmdWidth(
	TEXT("r.RHICmdWidth"), 
	8,
	TEXT("Number of threads."));

static TAutoConsoleVariable<int32> CVarRHICmdUseDeferredContexts(
	TEXT("r.RHICmdUseDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize command list execution. Only available on some RHIs."));
static TAutoConsoleVariable<int32> CVarRHIDeferredContextWidth(
	TEXT("r.RHIDeferredContextWidth"),
	99999,
	TEXT("Number of pieces to break a parallel translate task into. A very large number is one task per original cmd list. Only available on some RHIs."));

static TAutoConsoleVariable<int32> CVarRHICmdFlushOnQueueParallelSubmit(
	TEXT("r.RHICmdFlushOnQueueParallelSubmit"),
	0,
	TEXT("Wait for completion of parallel commandlists immediately after submitting.  Debug feature. Only available on some RHIs."));

RHI_API FRHICommandListExecutor GRHICommandList;

static FGraphEventArray AllOutstandingTasks;
static FGraphEventArray WaitOutstandingTasks;
static FGraphEventRef RHIThreadTask;
static FGraphEventRef RenderThreadSublistDispatchTask;
static FGraphEventRef RenderThreadExecutionTask;

DECLARE_CYCLE_STAT(TEXT("RHI Thread Execute"), STAT_RHIThreadExecute, STATGROUP_RHICMDLIST);

static TStatId GCurrentExecuteStat;

struct FRHICommandStat : public FRHICommand<FRHICommandStat>
{
	TStatId CurrentExecuteStat;
	FORCEINLINE_DEBUGGABLE FRHICommandStat(TStatId InCurrentExecuteStat)
		: CurrentExecuteStat(InCurrentExecuteStat)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		GCurrentExecuteStat = CurrentExecuteStat;
	}
};

bool FRHICommandListImmediate::bFlushedGlobal = false;
void FRHICommandListImmediate::SetCurrentStat(TStatId Stat)
{
	new (AllocCommand<FRHICommandStat>()) FRHICommandStat(Stat);
}

FRHICommandBase* GCurrentCommand = nullptr;

void FRHICommandListExecutor::ExecuteInner_DoExecute(FRHICommandListBase& CmdList)
{
	CmdList.bExecuting = true;
	check(CmdList.Context);

	bool bDoStats = false;
	//STAT(bDoStats = FThreadStats::IsCollectingData() && (IsInRenderingThread() || IsInRHIThread());)
	FRHICommandListIterator Iter(CmdList);
	if (bDoStats)
	{
		while (Iter.HasCommandsLeft())
		{
			FRHICommandBase* Cmd = Iter.NextCommand();
			//FPlatformMisc::Prefetch(Cmd->Next);
			FScopeCycleCounter Scope(GCurrentExecuteStat);
			Cmd->CallExecuteAndDestruct(CmdList);
		}
	}
	else
	{
		while (Iter.HasCommandsLeft())
		{
			FRHICommandBase* Cmd = Iter.NextCommand();
			GCurrentCommand = Cmd;
			//FPlatformMisc::Prefetch(Cmd->Next);
			Cmd->CallExecuteAndDestruct(CmdList);
		}
	}
	CmdList.Reset();
}



class FExecuteRHIThreadTask
{
	FRHICommandListBase* RHICmdList;
	FGraphEventRef ExecuteFenceTask;

public:

	FExecuteRHIThreadTask(FRHICommandListBase* InRHICmdList)
		: RHICmdList(InRHICmdList)
		, ExecuteFenceTask(nullptr)
	{
	}

	FExecuteRHIThreadTask(FRHICommandListBase* InRHICmdList, FGraphEventRef InExecuteFenceTask)
		: RHICmdList(InRHICmdList)
		, ExecuteFenceTask(InExecuteFenceTask)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FExecuteRHIThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		check(GRHIThread); // this should never be used on a platform that doesn't support the RHI thread
		return ENamedThreads::RHIThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_RHIThreadExecute);
		FRHICommandListExecutor::ExecuteInner_DoExecute(*RHICmdList);
		delete RHICmdList;

		if (ExecuteFenceTask)
		{			
			ExecuteFenceTask->DispatchSubsequents();
			ExecuteFenceTask = nullptr;
		}
	}
};

class FExecuteFenceRHITask
{	
	FGraphEventRef ExecuteFenceTask;

public:

	FExecuteFenceRHITask(FGraphEventRef InExecuteFenceTask)		
		: ExecuteFenceTask(InExecuteFenceTask)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FExecuteFenceRHITask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		check(GRHIThread); // this should never be used on a platform that doesn't support the RHI thread
		return ENamedThreads::RHIThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ExecuteFenceTask->DispatchSubsequents();
		ExecuteFenceTask = nullptr;		
	}
};

class FDispatchRHIThreadTask
{
	FRHICommandListBase* RHICmdList;
	FGraphEventRef ExecuteFenceTask;

public:

	FDispatchRHIThreadTask(FRHICommandListBase* InRHICmdList)
		: RHICmdList(InRHICmdList)
		, ExecuteFenceTask(nullptr)
	{		
	}

	FDispatchRHIThreadTask(FRHICommandListBase* InRHICmdList, FGraphEventRef InExecuteFenceTask)
		: RHICmdList(InRHICmdList)
		, ExecuteFenceTask(InExecuteFenceTask)
	{		
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDispatchRHIThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		check(GRHIThread); // this should never be used on a platform that doesn't support the RHI thread
		return ENamedThreads::RenderThread_Local;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(IsInRenderingThread());
		FGraphEventArray Prereq;
		if (RHIThreadTask.GetReference())
		{
			Prereq.Add(RHIThreadTask);
		}
		RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(RHICmdList, ExecuteFenceTask);
		ExecuteFenceTask = nullptr;
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdUseThread(
	TEXT("r.RHICmdUseThread"),
	1,
	TEXT("Uses the RHI thread. Just for testing, should be removed. (GilG)\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);
static TAutoConsoleVariable<int32> CVarRHICmdForceRHIFlush(
	TEXT("r.RHICmdForceRHIFlush"),
	0,
	TEXT("Force a flush for every task sent to the RHI thread. Just for testing, should be removed. (GilG)\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);

void FRHICommandListExecutor::ExecuteInner(FRHICommandListBase& CmdList)
{
	check(CmdList.RTTasks.Num() == 0 && CmdList.HasCommands()); 

	bool bIsInRenderingThread = IsInRenderingThread();
	if (GRHIThread)
	{
		if (bIsInRenderingThread)
		{
			if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
			{
				RHIThreadTask = nullptr;
			}
			if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
			{
				RenderThreadSublistDispatchTask = nullptr;
			}
			if (RenderThreadExecutionTask.GetReference() && RenderThreadExecutionTask->IsComplete())
			{
				RenderThreadExecutionTask = nullptr;
			}
		}
		if (CVarRHICmdUseThread.GetValueOnRenderThread() > 0 && bIsInRenderingThread && !IsInGameThread())
		{
			FRHICommandList* SwapCmdList = new FRHICommandList;

			// Super scary stuff here, but we just want the swap command list to inherit everything and leave the immediate command list wiped.
			// we should make command lists virtual and transfer ownership rather than this devious approach
			static_assert(sizeof(FRHICommandList) == sizeof(FRHICommandListImmediate), "We are memswapping FRHICommandList and FRHICommandListImmediate; they need to be swappable.");
			SwapCmdList->ExchangeCmdList(CmdList);

			//if we use a FDispatchRHIThreadTask, we must have it pass an event along to the FExecuteRHIThreadTask it will spawn so that fences can know which event to wait on for execution completion
			//before the dispatch completes.
			//if we use a FExecuteRHIThreadTask directly we pass the same event just to keep things consistent.
			RenderThreadExecutionTask = FGraphEvent::CreateGraphEvent();
			if (AllOutstandingTasks.Num() || RenderThreadSublistDispatchTask.GetReference())
			{
				FGraphEventArray Prereq(AllOutstandingTasks);
				AllOutstandingTasks.Reset();
				if (RenderThreadSublistDispatchTask.GetReference())
				{
					Prereq.Add(RenderThreadSublistDispatchTask);
				}
				RenderThreadSublistDispatchTask = TGraphTask<FDispatchRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(SwapCmdList, RenderThreadExecutionTask);
			}
			else
			{
				FGraphEventArray Prereq;
				if (RHIThreadTask.GetReference())
				{
					Prereq.Add(RHIThreadTask);
				}
				RHIThreadTask = TGraphTask<FExecuteRHIThreadTask>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(SwapCmdList, RenderThreadExecutionTask);
			}
			if (CVarRHICmdForceRHIFlush.GetValueOnRenderThread() > 0 )
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner 2."));
				}
				if (RenderThreadSublistDispatchTask.GetReference())
				{
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
					RenderThreadSublistDispatchTask = nullptr;
				}
				while (RHIThreadTask.GetReference())
				{
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, ENamedThreads::RenderThread_Local);
					if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
					{
						RHIThreadTask = nullptr;
					}
				}
			}
			return;
		}
		if (bIsInRenderingThread)
		{
			if (RenderThreadSublistDispatchTask.GetReference())
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner 3."));
				}
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
				RenderThreadSublistDispatchTask = nullptr;
			}
			while (RHIThreadTask.GetReference())
			{
				if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
				{
					// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
					UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListExecutor::ExecuteInner 3."));
				}
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, ENamedThreads::RenderThread_Local);
				if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
				{
					RHIThreadTask = nullptr;
				}
			}
		}
	}

	ExecuteInner_DoExecute(CmdList);
}


static FORCEINLINE bool IsInRenderingOrRHIThread()
{
	return IsInRenderingThread() || IsInRHIThread();
}

void FRHICommandListExecutor::ExecuteList(FRHICommandListBase& CmdList)
{
	check(&CmdList != &GetImmediateCommandList() && (GRHISupportsParallelRHIExecute || IsInRenderingOrRHIThread()));

	if (IsInRenderingThread() && !GetImmediateCommandList().IsExecuting()) // don't flush if this is a recursive call and we are already executing the immediate command list
	{
		GetImmediateCommandList().Flush();
	}

	INC_MEMORY_STAT_BY(STAT_NonImmedCmdListMemory, CmdList.GetUsedMemory());
	INC_DWORD_STAT_BY(STAT_NonImmedCmdListCount, CmdList.NumCommands);

	SCOPE_CYCLE_COUNTER(STAT_NonImmedCmdListExecuteTime);
	ExecuteInner(CmdList);
}

void FRHICommandListExecutor::ExecuteList(FRHICommandListImmediate& CmdList)
{
	check(IsInRenderingOrRHIThread() && &CmdList == &GetImmediateCommandList());

	INC_MEMORY_STAT_BY(STAT_ImmedCmdListMemory, CmdList.GetUsedMemory());
	INC_DWORD_STAT_BY(STAT_ImmedCmdListCount, CmdList.NumCommands);
#if 0
	static TAutoConsoleVariable<int32> CVarRHICmdMemDump(
		TEXT("r.RHICmdMemDump"),
		0,
		TEXT("dumps callstacks and sizes of the immediate command lists to the console.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);
	if (CVarRHICmdMemDump.GetValueOnRenderThread() > 0)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mem %d\n"), CmdList.GetUsedMemory());
		if (CmdList.GetUsedMemory() > 300)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("big\n"));
		}
	}
#endif
	{
		SCOPE_CYCLE_COUNTER(STAT_ImmedCmdListExecuteTime);
		ExecuteInner(CmdList);
	}
}


void FRHICommandListExecutor::LatchBypass()
{
#if !UE_BUILD_SHIPPING
	if (GRHIThread)
	{
		if (bLatchedBypass)
		{
			check((GRHICommandList.OutstandingCmdListCount.GetValue() == 1 && !GRHICommandList.GetImmediateCommandList().HasCommands()));
			bLatchedBypass = false;
		}
	}
	else
	{
		GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

		static bool bOnce = false;
		if (!bOnce)
		{
			bOnce = true;
			if (FParse::Param(FCommandLine::Get(),TEXT("parallelrendering")) && CVarRHICmdBypass.GetValueOnRenderThread() >= 1)
			{
				IConsoleVariable* BypassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmdBypass"));
				BypassVar->Set(0, ECVF_SetByCommandline);
			}
		}

		check((GRHICommandList.OutstandingCmdListCount.GetValue() == 1 && !GRHICommandList.GetImmediateCommandList().HasCommands()));
		bool NewBypass = (CVarRHICmdBypass.GetValueOnAnyThread() >= 1);
		if (NewBypass && !bLatchedBypass)
		{
			FRHIResource::FlushPendingDeletes();
		}
		bLatchedBypass = NewBypass;
	}
#endif
	if (!bLatchedBypass)
	{
		bLatchedUseParallelAlgorithms = FApp::ShouldUseThreadingForPerformance() 
#if !UE_BUILD_SHIPPING
			&& (CVarRHICmdUseParallelAlgorithms.GetValueOnAnyThread() >= 1)
#endif
			;
	}
	else
	{
		bLatchedUseParallelAlgorithms = false;
	}
}

void FRHICommandListExecutor::CheckNoOutstandingCmdLists()
{
	check(GRHICommandList.OutstandingCmdListCount.GetValue() == 1); // else we are attempting to delete resources while there is still a live cmdlist (other than the immediate cmd list) somewhere.
}

bool FRHICommandListExecutor::IsRHIThreadActive()
{
	checkSlow(IsInRenderingThread());
	if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
	{
		RHIThreadTask = nullptr;
	}
	return !!RHIThreadTask.GetReference();
}

DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.RHIThreadFence"), STAT_RHIThreadFence, STATGROUP_TaskGraphTasks);

FGraphEventRef FRHICommandListExecutor::RHIThreadFence()
{
	check(IsInRenderingThread() && GRHIThread);
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIThreadFence_Dispatch);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}

	// every task dispatched or executed through the flush creates a new RenderThreadExecutionTask that represents the 'execution' of the last dispatched
	// set of RHI commands.
	if (RenderThreadExecutionTask.GetReference() && RenderThreadExecutionTask->IsComplete())
	{
		RenderThreadExecutionTask = nullptr;
	}		
	
	return RenderThreadExecutionTask;
}
void FRHICommandListExecutor::WaitOnRHIThreadFence(FGraphEventRef& Fence)
{
	check(IsInRenderingThread() && GRHIThread);
	if (Fence.GetReference() && !Fence->IsComplete())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitOnRHIThreadFence_Wait);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
		{
			// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
			UE_LOG(LogRHI, Fatal, TEXT("Deadlock in WaitOnRHIThreadFence."));
		}
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Fence, ENamedThreads::RenderThread_Local);
	}
}


FRHICommandListBase::FRHICommandListBase()
	: MemManager(0)
{
	GRHICommandList.OutstandingCmdListCount.Increment();
	Reset();
}

FRHICommandListBase::~FRHICommandListBase()
{
	Flush();
	GRHICommandList.OutstandingCmdListCount.Decrement();
}

const int32 FRHICommandListBase::GetUsedMemory() const
{
	return MemManager.GetByteCount();
}

void FRHICommandListBase::Reset()
{
	bExecuting = false;
	check(!RTTasks.Num());
	MemManager.Flush();
	NumCommands = 0;
	Root = nullptr;
	CommandLink = &Root;
	static_assert(USE_DYNAMIC_RHI, "static RHIs are not supported.");
	Context = GDynamicRHI ? RHIGetDefaultContext() : nullptr;
#if USE_RHICOMMAND_STATE_REDUCTION
	StateCache = nullptr;
#endif
	UID = GRHICommandList.UIDCounter.Increment();
}


DECLARE_CYCLE_STAT(TEXT("Parallel Async Chain Translate"), STAT_ParallelChainTranslate, STATGROUP_RHICMDLIST);

class FParallelTranslateCommandList
{
	FRHICommandListBase** RHICmdLists;
	int32 NumCommandLists;
	IRHICommandContextContainer* ContextContainer;
public:

	FParallelTranslateCommandList(FRHICommandListBase** InRHICmdLists, int32 InNumCommandLists, IRHICommandContextContainer* InContextContainer)
		: RHICmdLists(InRHICmdLists)
		, NumCommandLists(InNumCommandLists)
		, ContextContainer(InContextContainer)
	{
		check(RHICmdLists && ContextContainer && NumCommandLists);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelTranslateCommandList, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_ParallelChainTranslate);
		check(ContextContainer && RHICmdLists);
		IRHICommandContext* Context = ContextContainer->GetContext();
		check(Context);
		for (int32 Index = 0; Index < NumCommandLists; Index++)
		{
			RHICmdLists[Index]->SetContext(Context);
			delete RHICmdLists[Index];
		}
		ContextContainer->FinishContext();
	}
};

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Parallel Async Chains Links"), STAT_ParallelChainLinkCount, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for Parallel Async CmdList"), STAT_ParallelChainWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Parallel Async Chain Execute"), STAT_ParallelChainExecute, STATGROUP_RHICMDLIST);

struct FRHICommandWaitForAndSubmitSubListParallel : public FRHICommand<FRHICommandWaitForAndSubmitSubListParallel>
{
	FGraphEventRef TranslateCompletionEvent;
	IRHICommandContextContainer* ContextContainer;
	int32 Num;
	int32 Index;

	FORCEINLINE_DEBUGGABLE FRHICommandWaitForAndSubmitSubListParallel(FGraphEventRef& InTranslateCompletionEvent, IRHICommandContextContainer* InContextContainer, int32 InNum, int32 InIndex)
		: TranslateCompletionEvent(InTranslateCompletionEvent)
		, ContextContainer(InContextContainer)
		, Num(InNum)
		, Index(InIndex)
	{
		check(ContextContainer && Num);
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		check(ContextContainer && Num && IsInRHIThread());
		INC_DWORD_STAT_BY(STAT_ParallelChainLinkCount, 1);

		if (TranslateCompletionEvent.GetReference() && !TranslateCompletionEvent->IsComplete())
		{
			SCOPE_CYCLE_COUNTER(STAT_ParallelChainWait);
			if (IsInRenderingThread())
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(TranslateCompletionEvent, ENamedThreads::RenderThread_Local);
			}
			else if (IsInRHIThread())
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(TranslateCompletionEvent, ENamedThreads::RHIThread);
			}
			else
			{
				check(0);
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_ParallelChainExecute);
			ContextContainer->SubmitAndFreeContextContainer(Index, Num);
		}
	}
};



DECLARE_DWORD_COUNTER_STAT(TEXT("Num Async Chains Links"), STAT_ChainLinkCount, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for Async CmdList"), STAT_ChainWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Async Chain Execute"), STAT_ChainExecute, STATGROUP_RHICMDLIST);

struct FRHICommandWaitForAndSubmitSubList : public FRHICommand<FRHICommandWaitForAndSubmitSubList>
{
	FGraphEventRef EventToWaitFor;
	FRHICommandList* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHICommandWaitForAndSubmitSubList(FGraphEventRef& InEventToWaitFor, FRHICommandList* InRHICmdList)
		: EventToWaitFor(InEventToWaitFor)
		, RHICmdList(InRHICmdList)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_ChainLinkCount, 1);
		if (EventToWaitFor.GetReference() && !EventToWaitFor->IsComplete())
		{
			check(!GRHIThread || !IsInRHIThread()); // things should not be dispatched if they can't complete without further waits
			SCOPE_CYCLE_COUNTER(STAT_ChainWait);
			if (IsInRenderingThread())
			{
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(EventToWaitFor, ENamedThreads::RenderThread_Local);
			}
			else
			{
				check(0);
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_ChainExecute);
			RHICmdList->CopyContext(CmdList);
			delete RHICmdList;
		}
	}
};

void FRHICommandListBase::QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, FRHICommandList** CmdLists, int32 Num)
{
	check(IsInRenderingThread() && IsImmediate() && Num);
	if (GRHIThread)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we should start on the stuff before this async list
	}
#if !UE_BUILD_SHIPPING
	// do a flush before hand so we can tell if it was this parallel set that broke something, or what came before.
	if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
#endif

	if (Num && GRHIThread)
	{
		IRHICommandContextContainer* ContextContainer = nullptr;
		if (GRHISupportsParallelRHIExecute && CVarRHICmdUseDeferredContexts.GetValueOnAnyThread() >= 1)
		{
			ContextContainer = RHIGetCommandContextContainer();
		}
		if (ContextContainer)
		{
			int32 EffectiveThreads = FMath::Min<int32>(Num, CVarRHIDeferredContextWidth.GetValueOnRenderThread());

			int32 Start = 0;
			if (EffectiveThreads)
			{

				int32 NumPer = Num / EffectiveThreads;
				int32 Extra = Num - NumPer * EffectiveThreads;

				for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
				{
					int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
					check(Last >= Start);

					if (!ContextContainer)
					{
						ContextContainer = RHIGetCommandContextContainer();
					}
					check(ContextContainer);

					FGraphEventArray Prereq;
					FRHICommandListBase** RHICmdLists = (FRHICommandListBase**)Alloc(sizeof(FRHICommandListBase*) * (1 + Last - Start), ALIGNOF(FRHICommandListBase*));
					for (int32 Index = Start; Index <= Last; Index++)
					{
						FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
						FRHICommandList* CmdList = CmdLists[Index];
						RHICmdLists[Index - Start] = CmdList;
						if (AnyThreadCompletionEvent.GetReference())
						{
							Prereq.Add(AnyThreadCompletionEvent);
							AllOutstandingTasks.Add(AnyThreadCompletionEvent);
							WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
						}
					}
					FGraphEventRef TranslateCompletionEvent = TGraphTask<FParallelTranslateCommandList>::CreateTask(&Prereq, ENamedThreads::RenderThread).ConstructAndDispatchWhenReady(&RHICmdLists[0], 1 + Last - Start, ContextContainer);

					AllOutstandingTasks.Add(TranslateCompletionEvent);
					new (AllocCommand<FRHICommandWaitForAndSubmitSubListParallel>()) FRHICommandWaitForAndSubmitSubListParallel(TranslateCompletionEvent, ContextContainer, EffectiveThreads, ThreadIndex);
					if (GRHIThread)
					{
						FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
					}

					ContextContainer = nullptr;
					Start = Last + 1;
				}

#if !UE_BUILD_SHIPPING
				if (CVarRHICmdFlushOnQueueParallelSubmit.GetValueOnRenderThread())
				{
					FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
				}
#endif
				return;
			}
		}
	}
	for (int32 Index = 0; Index < Num; Index++)
	{
		FGraphEventRef& AnyThreadCompletionEvent = AnyThreadCompletionEvents[Index];
		FRHICommandList* CmdList = CmdLists[Index];
		if (AnyThreadCompletionEvent.GetReference())
		{
			if (GRHIThread)
			{
				AllOutstandingTasks.Add(AnyThreadCompletionEvent);
			}
			WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
		}
		new (AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(AnyThreadCompletionEvent, CmdList);
	}
	if (GRHIThread)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
	}
}

void FRHICommandListBase::QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHICommandList* CmdList)
{
	check(IsInRenderingThread() && IsImmediate());
	if (GRHIThread)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we should start on the stuff before this async list
	}
	if (AnyThreadCompletionEvent.GetReference())
	{
		if (GRHIThread)
		{
			AllOutstandingTasks.Add(AnyThreadCompletionEvent);
		}
		WaitOutstandingTasks.Add(AnyThreadCompletionEvent);
	}
	new (AllocCommand<FRHICommandWaitForAndSubmitSubList>()) FRHICommandWaitForAndSubmitSubList(AnyThreadCompletionEvent, CmdList);
	if (GRHIThread)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread); // we don't want stuff after the async cmd list to be bundled with it
	}
}

DECLARE_DWORD_COUNTER_STAT(TEXT("Num RT Chains Links"), STAT_RTChainLinkCount, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Wait for RT CmdList"), STAT_RTChainWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("RT Chain Execute"), STAT_RTChainExecute, STATGROUP_RHICMDLIST);

struct FRHICommandWaitForAndSubmitRTSubList : public FRHICommand<FRHICommandWaitForAndSubmitRTSubList>
{
	FGraphEventRef EventToWaitFor;
	FRHICommandList* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHICommandWaitForAndSubmitRTSubList(FGraphEventRef& InEventToWaitFor, FRHICommandList* InRHICmdList)
		: EventToWaitFor(InEventToWaitFor)
		, RHICmdList(InRHICmdList)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_RTChainLinkCount, 1);
		{
			SCOPE_CYCLE_COUNTER(STAT_RTChainWait);
			if (!EventToWaitFor->IsComplete())
			{
				check(!GRHIThread || !IsInRHIThread()); // things should not be dispatched if they can't complete without further waits
				if (IsInRenderingThread())
				{
					if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
					{
						// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
						UE_LOG(LogRHI, Fatal, TEXT("Deadlock in command list processing."));
					}
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(EventToWaitFor, ENamedThreads::RenderThread_Local);
				}
				else
				{
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(EventToWaitFor);
				}
			}
		}
		{
			SCOPE_CYCLE_COUNTER(STAT_RTChainExecute);
			RHICmdList->CopyContext(CmdList);
			delete RHICmdList;
		}
	}
};

void FRHICommandListBase::QueueRenderThreadCommandListSubmit(FGraphEventRef& RenderThreadCompletionEvent, class FRHICommandList* CmdList)
{
	check(!IsInRenderingThread() && !IsImmediate() && !IsInRHIThread());
	RTTasks.Add(RenderThreadCompletionEvent);
	new (AllocCommand<FRHICommandWaitForAndSubmitRTSubList>()) FRHICommandWaitForAndSubmitRTSubList(RenderThreadCompletionEvent, CmdList);
}

struct FRHICommandSubmitSubList : public FRHICommand<FRHICommandSubmitSubList>
{
	FRHICommandList* RHICmdList;
	FORCEINLINE_DEBUGGABLE FRHICommandSubmitSubList(FRHICommandList* InRHICmdList)
		: RHICmdList(InRHICmdList)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		INC_DWORD_STAT_BY(STAT_ChainLinkCount, 1);
		SCOPE_CYCLE_COUNTER(STAT_ChainExecute);
		RHICmdList->CopyContext(CmdList);
		delete RHICmdList;
	}
};

void FRHICommandListBase::QueueCommandListSubmit(class FRHICommandList* CmdList)
{
	new (AllocCommand<FRHICommandSubmitSubList>()) FRHICommandSubmitSubList(CmdList);
}


void FRHICommandList::BeginScene()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(BeginScene)();
		return;
	}
	new (AllocCommand<FRHICommandBeginScene>()) FRHICommandBeginScene();
	if (!GRHIThread)
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginScene_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}
void FRHICommandList::EndScene()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(EndScene)();
		return;
	}
	new (AllocCommand<FRHICommandEndScene>()) FRHICommandEndScene();
	if (!GRHIThread)
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(EndScene_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}


void FRHICommandList::BeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI)
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(BeginDrawingViewport)(Viewport, RenderTargetRHI);
		return;
	}
	new (AllocCommand<FRHICommandBeginDrawingViewport>()) FRHICommandBeginDrawingViewport(Viewport, RenderTargetRHI);
	if (!GRHIThread)
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginDrawingViewport_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandList::EndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync)
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(EndDrawingViewport)(Viewport, bPresent, bLockToVsync);
	}
	else
	{
		new (AllocCommand<FRHICommandEndDrawingViewport>()) FRHICommandEndDrawingViewport(Viewport, bPresent, bLockToVsync);
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_EndDrawingViewport_Dispatch);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}
	RHIAdvanceFrameForGetViewportBackBuffer();
}

void FRHICommandList::BeginFrame()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(BeginFrame)();
		return;
	}
	new (AllocCommand<FRHICommandBeginFrame>()) FRHICommandBeginFrame();
	if (!GRHIThread)
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(BeginFrame_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

void FRHICommandList::EndFrame()
{
	check(IsImmediate() && IsInRenderingThread());
	if (Bypass())
	{
		CMD_CONTEXT(EndFrame)();
		return;
	}
	new (AllocCommand<FRHICommandEndFrame>()) FRHICommandEndFrame();
	if (!GRHIThread)
	{
		// if we aren't running an RHIThread, there is no good reason to buffer this frame advance stuff and that complicates state management, so flush everything out now
		QUICK_SCOPE_CYCLE_COUNTER(EndFrame_Flush);
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
}

DECLARE_CYCLE_STAT(TEXT("Explicit wait for tasks"), STAT_ExplicitWait, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Prewait dispatch"), STAT_PrewaitDispatch, STATGROUP_RHICMDLIST);
void FRHICommandListBase::WaitForTasks(bool bKnownToBeComplete)
{
	check(IsImmediate() && IsInRenderingThread());
	if (WaitOutstandingTasks.Num())
	{
		bool bAny = false;
		for (int32 Index = 0; Index < WaitOutstandingTasks.Num(); Index++)
		{
			if (!WaitOutstandingTasks[Index]->IsComplete())
			{
				check(!bKnownToBeComplete);
				bAny = true;
				break;
			}
		}
		if (bAny)
		{
			SCOPE_CYCLE_COUNTER(STAT_ExplicitWait);
			FTaskGraphInterface::Get().WaitUntilTasksComplete(WaitOutstandingTasks, ENamedThreads::RenderThread_Local);
		}
		WaitOutstandingTasks.Reset();
	}
}

DECLARE_CYCLE_STAT(TEXT("Explicit wait for dispatch"), STAT_ExplicitWaitDispatch, STATGROUP_RHICMDLIST);
void FRHICommandListBase::WaitForDispatch()
{
	check(IsImmediate() && IsInRenderingThread());
	check(!AllOutstandingTasks.Num()); // dispatch before you get here
	if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
	{
		RenderThreadSublistDispatchTask = nullptr;
	}
	while (RenderThreadSublistDispatchTask.GetReference())
	{
		SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitDispatch);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
		{
			// this is a deadlock. RT tasks must be done by now or they won't be done. We could add a third queue...
			UE_LOG(LogRHI, Fatal, TEXT("Deadlock in FRHICommandListBase::WaitForDispatch."));
		}
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(RenderThreadSublistDispatchTask, ENamedThreads::RenderThread_Local);
		if (RenderThreadSublistDispatchTask.GetReference() && RenderThreadSublistDispatchTask->IsComplete())
		{
			RenderThreadSublistDispatchTask = nullptr;
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Explicit wait for RHI thread"), STAT_ExplicitWaitRHIThread, STATGROUP_RHICMDLIST);
DECLARE_CYCLE_STAT(TEXT("Deep spin for stray resource init."), STAT_SpinWaitRHIThread, STATGROUP_RHICMDLIST);
void FRHICommandListBase::WaitForRHIThreadTasks()
{
	check(IsImmediate() && IsInRenderingThread());
	if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
	{
		RHIThreadTask = nullptr;
	}
	while (RHIThreadTask.GetReference())
	{
		SCOPE_CYCLE_COUNTER(STAT_ExplicitWaitRHIThread);
		if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RenderThread_Local))
		{
			// we have to spin here because all task threads might be stalled, meaning the fire event anythread task might not be hit.
			// todo, add a third queue
			SCOPE_CYCLE_COUNTER(STAT_SpinWaitRHIThread);
			while (!RHIThreadTask->IsComplete())
			{
				FPlatformProcess::Sleep(0);
			}
		}
		else
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHIThreadTask, ENamedThreads::RenderThread_Local);
		}
		if (RHIThreadTask.GetReference() && RHIThreadTask->IsComplete())
		{
			RHIThreadTask = nullptr;
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("RTTask completion join"), STAT_HandleRTThreadTaskCompletion_Join, STATGROUP_RHICMDLIST);
void FRHICommandListBase::HandleRTThreadTaskCompletion(const FGraphEventRef& MyCompletionGraphEvent)
{
	check(!IsImmediate() && !IsInRenderingThread() && !IsInRHIThread())
	for (int32 Index = 0; Index < RTTasks.Num(); Index++)
	{
		if (!RTTasks[Index]->IsComplete())
		{
			MyCompletionGraphEvent->DontCompleteUntil(RTTasks[Index]);
		}
	}
	RTTasks.Empty();
}

static TLockFreeFixedSizeAllocator<sizeof(FRHICommandList), FThreadSafeCounter> RHICommandListAllocator;

void* FRHICommandList::operator new(size_t Size)
{
	// doesn't support derived classes with a different size
	check(Size == sizeof(FRHICommandList));
	return RHICommandListAllocator.Allocate();
	//return FMemory::Malloc(Size);
}

/**
 * Custom delete
 */
void FRHICommandList::operator delete(void *RawMemory)
{
	check(RawMemory != (void*) &GRHICommandList.GetImmediateCommandList());
	RHICommandListAllocator.Free(RawMemory);
	//FMemory::Free(RawMemory);
}	

