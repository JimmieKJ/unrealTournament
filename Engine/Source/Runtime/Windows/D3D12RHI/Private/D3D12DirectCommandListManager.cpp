// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "Windows.h"


extern bool D3D12RHI_ShouldCreateWithD3DDebug();


FComputeFenceRHIRef FD3D12DynamicRHI::RHICreateComputeFence(const FName& Name)
{
	FD3D12Fence* Fence = new FD3D12Fence(GetRHIDevice(),Name);

	Fence->CreateFence(0);

	return Fence;
}

FD3D12FenceCore::FD3D12FenceCore(FD3D12Device* Parent, uint64 InitialValue)
	: hFenceCompleteEvent(INVALID_HANDLE_VALUE)
	, FenceValueAvailableAt(0)
	, FD3D12DeviceChild(Parent)
{
	check(Parent);
	hFenceCompleteEvent = CreateEvent(nullptr, false, false, nullptr);
	check(INVALID_HANDLE_VALUE != hFenceCompleteEvent);

	VERIFYD3D11RESULT(Parent->GetDevice()->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetInitReference())));
}

FD3D12FenceCore::~FD3D12FenceCore()
{
	if (hFenceCompleteEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFenceCompleteEvent);
		hFenceCompleteEvent = INVALID_HANDLE_VALUE;
	}
}

FD3D12Fence::FD3D12Fence(FD3D12Device* Parent, const FName& Name)
	: FRHIComputeFence(Name)
	, CurrentFence(-1)
	, SignalFence(-1)
	, LastCompletedFence(-1)
	, FenceCore(nullptr)
	, FD3D12DeviceChild(Parent)
{
}

FD3D12Fence::~FD3D12Fence()
{
	Destroy();
}

void FD3D12Fence::Destroy()
{
	if (FenceCore)
	{
		//Return the underlying fence to the pool, store the last value signaled on this fence
		GetParentDevice()->GetFenceCorePool().ReleaseFenceCore(FenceCore, SignalFence);
		FenceCore = nullptr;
	}
}

void FD3D12Fence::CreateFence(uint64 InitialValue)
{
	check(FenceCore == nullptr);

	//Get a fence from the pool
	FenceCore = GetParentDevice()->GetFenceCorePool().ObtainFenceCore(InitialValue);
	ID3D12Fence* Fence = FenceCore->GetFence();
	check(Fence);

	SetName(Fence, *GetName().ToString());
	LastCompletedFence = Fence->GetCompletedValue();
	CurrentFence = SignalFence = LastCompletedFence + 1;
}

uint64 FD3D12Fence::Signal(ID3D12CommandQueue* pCommandQueue)
{
	check(pCommandQueue != nullptr);

	VERIFYD3D11RESULT(pCommandQueue->Signal(FenceCore->GetFence(), CurrentFence));

	// Save the current fence and increment it
	SignalFence = CurrentFence++;

	// Return the value that was signaled
	return SignalFence;
}

void FD3D12Fence::GpuWait(ID3D12CommandQueue* pCommandQueue, uint64 FenceValue)
{
	check(pCommandQueue != nullptr);

	VERIFYD3D11RESULT(pCommandQueue->Wait(FenceCore->GetFence(), FenceValue));
}

bool FD3D12Fence::IsFenceComplete(uint64 FenceValue)
{
	check(FenceCore);

	// Avoid repeatedly calling GetCompletedValue()
	if (FenceValue <= LastCompletedFence)
	{
		return true;
	}

	// Refresh the completed fence value
	LastCompletedFence = FenceCore->GetFence()->GetCompletedValue();
	return FenceValue <= LastCompletedFence;
}

uint64 FD3D12Fence::GetLastCompletedFence()
{
	LastCompletedFence = FenceCore->GetFence()->GetCompletedValue();
	return LastCompletedFence;
}

void FD3D12Fence::WaitForFence(uint64 FenceValue)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12WaitForFenceTime);

	check(FenceCore);

	if (IsFenceComplete(FenceValue))
	{
		return;
	}

	// We must wait.  Do so with an event handler so we don't oversleep.
	VERIFYD3D11RESULT(FenceCore->GetFence()->SetEventOnCompletion(FenceValue, FenceCore->GetCompleteionEvent()));

	// Wait for the event to complete (the event is automatically reset afterwards)
	const uint32 WaitResult = WaitForSingleObject(FenceCore->GetCompleteionEvent(), INFINITE);
	check(0 == WaitResult);

	LastCompletedFence = FenceValue;
}

FD3D12CommandAllocator* FD3D12CommandAllocatorManager::ObtainCommandAllocator()
{
	FScopeLock Lock(&CS);

	// See if the first command allocator in the queue is ready to be reset (will check associated fence)
	FD3D12CommandAllocator* pCommandAllocator = nullptr;
	if (CommandAllocatorQueue.Peek(pCommandAllocator) && pCommandAllocator->IsReady())
	{
		// Reset the allocator and remove it from the queue.
		pCommandAllocator->Reset();
		CommandAllocatorQueue.Dequeue(pCommandAllocator);
	}
	else
	{
		// The queue was empty, or no command allocators were ready, so create a new command allocator.
		pCommandAllocator = new FD3D12CommandAllocator(GetParentDevice()->GetDevice(), Type);
		check(pCommandAllocator);
		CommandAllocators.Add(pCommandAllocator);	// The command allocator's lifetime is managed by this manager
	}

	check(pCommandAllocator->IsReady());
	return pCommandAllocator;
}

void FD3D12CommandAllocatorManager::ReleaseCommandAllocator(FD3D12CommandAllocator* pCommandAllocator)
{
	FScopeLock Lock(&CS);
	CommandAllocatorQueue.Enqueue(pCommandAllocator);
}

FD3D12CommandListManager::FD3D12CommandListManager(FD3D12Device* InParent, D3D12_COMMAND_LIST_TYPE CommandListType)
	: NumCommandListsAllocated(0)
	, CommandListType(CommandListType)
	, FD3D12DeviceChild(InParent)
	, ResourceBarrierCommandAllocator(nullptr)
	, ResourceBarrierCommandAllocatorManager(InParent, D3D12_COMMAND_LIST_TYPE_DIRECT)
{
	for (FD3D12Fence& Fence : Fences)
	{
		Fence.SetParentDevice(InParent);
	}
}

FD3D12CommandListManager::~FD3D12CommandListManager()
{
	Destroy();
}

void FD3D12CommandListManager::Destroy()
{
	// Wait for the queue to empty
	WaitForCommandQueueFlush();

	D3DCommandQueue.SafeRelease();

	FD3D12CommandListHandle hList;
	while (!ReadyLists.IsEmpty())
	{
		ReadyLists.Dequeue(hList);
	}

	for (FD3D12Fence& Fence : Fences)
	{
		Fence.Destroy();
	}
}

void FD3D12CommandListManager::Create(uint32 NumCommandLists)
{
	check(D3DCommandQueue.GetReference() == nullptr);
	check(ReadyLists.IsEmpty());
	checkf(NumCommandLists <= 0xffff, TEXT("Exceeded maximum supported command lists"));

	ID3D12Device* Direct3DDevice = GetParentDevice()->GetDevice();
	D3D12_COMMAND_QUEUE_DESC CommandQueueDesc ={};
	CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CommandQueueDesc.NodeMask = 0;
	CommandQueueDesc.Priority = 0;
	CommandQueueDesc.Type = CommandListType;
	VERIFYD3D11RESULT(Direct3DDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(D3DCommandQueue.GetInitReference())));

	for (uint32 i = 0; i < FT_NumTypes; i++)
	{
		Fences[i].CreateFence(0);
	}

	if (NumCommandLists > 0)
	{
		// Create a temp command allocator for command list creation.
		FD3D12CommandAllocator TempCommandAllocator(Direct3DDevice, CommandListType);
		for (uint32 i = 0; i < NumCommandLists; ++i)
		{
			FD3D12CommandListHandle hList = CreateCommandListHandle(TempCommandAllocator);
			ReadyLists.Enqueue(hList);
		}
	}
}

FD3D12CommandListHandle FD3D12CommandListManager::ObtainCommandList(FD3D12CommandAllocator& CommandAllocator)
{
	FD3D12CommandListHandle List;
	if (!ReadyLists.Dequeue(List))
	{
		// Create a command list if there are none available.
		List = CreateCommandListHandle(CommandAllocator);
	}

	List.Reset(CommandAllocator);
	return List;
}

void FD3D12CommandListManager::ReleaseCommandList(FD3D12CommandListHandle& hList)
{
	check(hList.IsClosed());
	ReadyLists.Enqueue(hList);
}

void FD3D12CommandListManager::SignalFrameComplete(bool WaitForCompletion)
{
	FD3D12Fence& Fence = Fences[FT_Frame];
	const uint64 SignaledFence = Fence.Signal(D3DCommandQueue);

	if (WaitForCompletion)
	{
		Fence.WaitForFence(SignaledFence);
	}

	// Release the resource barrier command allocator.
	if (ResourceBarrierCommandAllocator != nullptr)
	{
		ResourceBarrierCommandAllocatorManager.ReleaseCommandAllocator(ResourceBarrierCommandAllocator);
		ResourceBarrierCommandAllocator = nullptr;
	}
}

void FD3D12CommandListManager::ExecuteCommandList(FD3D12CommandListHandle& hList, bool WaitForCompletion)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ExecuteCommandListTime);

	TArray<FD3D12CommandListHandle> Lists;
	Lists.Add(hList);

	ExecuteCommandLists(Lists, WaitForCompletion);
}

uint64 FD3D12CommandListManager::ExecuteAndIncrementFence(ID3D12CommandList* pD3DCommandLists[], uint64 NumCommandLists, FD3D12Fence &Fence)
{
	FScopeLock Lock(&FenceCS);

	// Execute, signal, and wait (if requested)
#if UE_BUILD_DEBUG
	if (D3D12RHI_ShouldCreateWithD3DDebug())
	{
		// Debug layer will break when a command list does bad stuff. Helps identify the command list in question.
		for (int32 i = 0; i < NumCommandLists; i++)
		{
			D3DCommandQueue->ExecuteCommandLists(1, &(pD3DCommandLists[i]));

#if LOG_EXECUTE_COMMAND_LISTS
			LogExecuteCommandLists(1, &(pD3DCommandLists[i]));
#endif
		}
	}
	else
#endif
	{
		D3DCommandQueue->ExecuteCommandLists(NumCommandLists, pD3DCommandLists);

#if LOG_EXECUTE_COMMAND_LISTS
		LogExecuteCommandLists(NumCommandLists, pD3DCommandLists);
#endif
	}

	return Fence.Signal(D3DCommandQueue);
}

void FD3D12CommandListManager::ExecuteCommandLists(TArray<FD3D12CommandListHandle>& Lists, bool WaitForCompletion)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ExecuteCommandListTime);

	bool NeedsResourceBarriers = false;
	for (int32 i = 0; i < Lists.Num(); i++)
	{
		FD3D12CommandListHandle& commandList = Lists[i];
		if (commandList.PendingResourceBarriers().Num() > 0)
		{
			NeedsResourceBarriers = true;
			break;
		}
	}

	FD3D12Fence& Fence = Fences[FT_CommandList];

	uint64 SignaledFenceValue = -1;
	uint64 BarrierFenceValue = -1;
	FD3D12SyncPoint SyncPoint;
	FD3D12SyncPoint BarrierSyncPoint;

	FD3D12CommandListManager& DirectCommandListManager = GetParentDevice()->GetCommandListManager();
	FD3D12Fence& DirectFence = DirectCommandListManager.GetFence(FT_CommandList);

	int32 commandListIndex = 0;
	int32 barrierCommandListIndex = 0;

	// Close the resource barrier lists, get the raw command list pointers, and enqueue the command list handles
	// Note: All command lists will share the same fence
	check(Lists.Num() <= 128);
	ID3D12CommandList* pD3DCommandLists[128];
	FD3D12CommandListHandle BarrierCommandList[128];
	if (NeedsResourceBarriers)
	{
#if !USE_D3D12RHI_RESOURCE_STATE_TRACKING
		// If we're using the engine's resource state tracking and barriers, then we should never have pending resource barriers.
		check(false);
#endif // !USE_D3D12RHI_RESOURCE_STATE_TRACKING

		//#todo-rco: Need verification from MS
#if 0//UE_BUILD_DEBUG	
		if (!ResourceStateCS.TryLock())
		{
			FD3D12DynamicRHI::GetD3DRHI()->SubmissionLockStalls++;
			// We don't think this will get hit but it's possible. If we do see this happen,
			// we should evaluate how often and why this is happening
			check(0);
		}
#endif
		FScopeLock Lock(&ResourceStateCS);

		for (int32 i = 0; i < Lists.Num(); i++)
		{
			FD3D12CommandListHandle& commandList = Lists[i];

			FD3D12CommandListHandle barrierCommandList ={};
			// Async compute cannot perform all resource transitions, and so it uses the direct context
			const uint32 numBarriers = DirectCommandListManager.GetResourceBarrierCommandList(commandList, barrierCommandList);
			if (numBarriers)
			{
				// TODO: Unnecessary assignment here, but fixing this will require refactoring GetResourceBarrierCommandList
				BarrierCommandList[barrierCommandListIndex] = barrierCommandList;
				barrierCommandListIndex++;

				barrierCommandList.Close();

				if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
				{
					ID3D12CommandList *SingleBarrierCommandList = barrierCommandList.CommandList();
					BarrierFenceValue = DirectCommandListManager.ExecuteAndIncrementFence(&SingleBarrierCommandList, 1, DirectFence);
					DirectFence.GpuWait(D3DCommandQueue, BarrierFenceValue);
				}
				else
				{
					pD3DCommandLists[commandListIndex] = barrierCommandList.CommandList();
					commandListIndex++;
				}

				check(commandListIndex < 127);
			}

			pD3DCommandLists[commandListIndex] = commandList.CommandList();
			commandListIndex++;
		}
		SignaledFenceValue = ExecuteAndIncrementFence(pD3DCommandLists, commandListIndex, Fence);
		SyncPoint = FD3D12SyncPoint(&Fence, SignaledFenceValue);
		if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
		{
			BarrierSyncPoint = FD3D12SyncPoint(&DirectFence, BarrierFenceValue);
		}
		else
		{
			BarrierSyncPoint = SyncPoint;
		}
	}
	else
	{
		for (int32 i = 0; i < Lists.Num(); i++)
		{
			FD3D12CommandListHandle& commandList = Lists[i];
			pD3DCommandLists[commandListIndex] = commandList.CommandList();
			commandListIndex++;
		}
		SignaledFenceValue = ExecuteAndIncrementFence(pD3DCommandLists, commandListIndex, Fence);
	}

	for (int32 i = 0; i < Lists.Num(); i++)
	{
		FD3D12CommandListHandle& commandList = Lists[i];

		// Set a sync point on the command list so we know when it's current generation is complete on the GPU, then release it so it can be reused later.
		// Note this also updates the command list's command allocator
		commandList.SetSyncPoint(SyncPoint);
		ReleaseCommandList(commandList);
	}

	for (int32 i = 0; i < barrierCommandListIndex; i++)
	{
		FD3D12CommandListHandle& commandList = BarrierCommandList[i];

		// Set a sync point on the command list so we know when it's current generation is complete on the GPU, then release it so it can be reused later.
		// Note this also updates the command list's command allocator
		commandList.SetSyncPoint(BarrierSyncPoint);
		DirectCommandListManager.ReleaseCommandList(commandList);
	}

	if (WaitForCompletion)
	{
		Fence.WaitForFence(SignaledFenceValue);
		check(SyncPoint.IsComplete());
	}
}

uint32 FD3D12CommandListManager::GetResourceBarrierCommandList(FD3D12CommandListHandle& hList, FD3D12CommandListHandle& hResourceBarrierList)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ExecuteCommandListTime);

	TArray<FD3D12PendingResourceBarrier>& PendingResourceBarriers = hList.PendingResourceBarriers();
	const uint32 NumPendingResourceBarriers = PendingResourceBarriers.Num();
	if (NumPendingResourceBarriers)
	{
		// Reserve space for the descs
		TArray<D3D12_RESOURCE_BARRIER> BarrierDescs;
		BarrierDescs.Reserve(NumPendingResourceBarriers);

		// Fill out the descs
		D3D12_RESOURCE_BARRIER Desc ={};
		Desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		for (uint32 i = 0; i < NumPendingResourceBarriers; ++i)
		{
			const FD3D12PendingResourceBarrier& PRB = PendingResourceBarriers[i];

			// Should only be doing this for the few resources that need state tracking
			check(PRB.Resource->RequiresResourceStateTracking());

			CResourceState* pResourceState = PRB.Resource->GetResourceState();
			check(pResourceState);

			Desc.Transition.Subresource = PRB.SubResource;
			const D3D12_RESOURCE_STATES Before = pResourceState->GetSubresourceState(Desc.Transition.Subresource);
			const D3D12_RESOURCE_STATES After = PRB.State;

			check(Before != D3D12_RESOURCE_STATE_TBD && Before != D3D12_RESOURCE_STATE_CORRUPT);
			if (Before != After)
			{
				Desc.Transition.pResource = PRB.Resource->GetResource();
				Desc.Transition.StateBefore = Before;
				Desc.Transition.StateAfter = After;

				// Add the desc
				BarrierDescs.Add(Desc);
			}

			// Update the state to the what it will be after hList executes
			const D3D12_RESOURCE_STATES LastState = hList.GetResourceState(PRB.Resource).GetSubresourceState(Desc.Transition.Subresource);
			if (Before != LastState)
			{
				pResourceState->SetSubresourceState(Desc.Transition.Subresource, LastState);
			}
		}

		if (BarrierDescs.Num() > 0)
		{
			// Get a new resource barrier command allocator if we don't already have one.
			if (ResourceBarrierCommandAllocator == nullptr)
			{
				ResourceBarrierCommandAllocator = ResourceBarrierCommandAllocatorManager.ObtainCommandAllocator();
			}

			hResourceBarrierList = ObtainCommandList(*ResourceBarrierCommandAllocator);

#if DEBUG_RESOURCE_STATES
			LogResourceBarriers(BarrierDescs.Num(), BarrierDescs.GetData(), hResourceBarrierList.CommandList());
#endif

			hResourceBarrierList->ResourceBarrier(BarrierDescs.Num(), BarrierDescs.GetData());
		}

		return BarrierDescs.Num();
	}

	return 0;
}

bool FD3D12CommandListManager::IsComplete(const FD3D12CLSyncPoint& hSyncPoint, uint64 FenceOffset)
{
	if (!hSyncPoint)
	{
		return false;
	}

	checkf(FenceOffset == 0, TEXT("This currently doesn't support offsetting fence values."));
	return hSyncPoint.IsComplete();
}

CommandListState FD3D12CommandListManager::GetCommandListState(const FD3D12CLSyncPoint& hSyncPoint)
{
	check(hSyncPoint);
	if (hSyncPoint.IsComplete())
	{
		return CommandListState::kFinished;
	}
	else if (hSyncPoint.Generation == hSyncPoint.CommandList.CurrentGeneration())
	{
		return CommandListState::kOpen;
	}
	else
	{
		return CommandListState::kQueued;
	}
}

void FD3D12CommandListManager::WaitForCommandQueueFlush()
{
	if (D3DCommandQueue)
	{
		FD3D12Fence& Fence = Fences[FT_Frame];
		const uint64 SignaledFence = Fence.Signal(D3DCommandQueue);
		Fence.WaitForFence(SignaledFence);
	}
}

FD3D12CommandListHandle FD3D12CommandListManager::CreateCommandListHandle(FD3D12CommandAllocator& CommandAllocator)
{
	FD3D12CommandListHandle List;
	List.Create(GetParentDevice()->GetDevice(), CommandListType, CommandAllocator, this);
	const int64 CommandListCount = FPlatformAtomics::InterlockedIncrement(&NumCommandListsAllocated);

	// If we hit this, we should really evaluate why we need so many command lists, especially since
	// each command list is paired with an allocator. This can consume a lot of memory even if the command lists
	// are empty
	check(CommandListCount < MAX_ALLOCATED_COMMAND_LISTS);
	return List;
}

FD3D12FenceCore* FD3D12FenceCorePool::ObtainFenceCore(uint64 InitialValue)
{
	{
		FScopeLock Lock(&CS);
		FD3D12FenceCore* Fence = nullptr;
		if (AvailableFences.Peek(Fence))
		{
			if (Fence->IsAvailable())
			{
				AvailableFences.Dequeue(Fence);

				//Reset the fence value
				Fence->GetFence()->Signal(InitialValue);

				return Fence;
			}
		}
	}

	return new FD3D12FenceCore(GetParentDevice(), InitialValue);
}

void FD3D12FenceCorePool::ReleaseFenceCore(FD3D12FenceCore* Fence, uint64 CurrentFenceValue)
{
	FScopeLock Lock(&CS);
	Fence->FenceValueAvailableAt = CurrentFenceValue;
	AvailableFences.Enqueue(Fence);
}

void FD3D12FenceCorePool::Destroy()
{
	FD3D12FenceCore* Fence = nullptr;
	while (AvailableFences.Dequeue(Fence))
	{
		delete(Fence);
	}
}
