#include "D3D12RHIPrivate.h"
#include "Windows.h"


extern bool D3D12RHI_ShouldCreateWithD3DDebug();


FD3D12Fence::FD3D12Fence()
	: CurrentFence(-1)
	, LastCompletedFence(-1)
	, hFenceCompleteEvent(INVALID_HANDLE_VALUE)
{
	// Create an event
	hFenceCompleteEvent = CreateEvent(nullptr, false, false, nullptr);
	check(INVALID_HANDLE_VALUE != hFenceCompleteEvent);
}

FD3D12Fence::~FD3D12Fence()
{
	if (hFenceCompleteEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFenceCompleteEvent);
		hFenceCompleteEvent = INVALID_HANDLE_VALUE;
	}

}

void FD3D12Fence::CreateFence(ID3D12Device* pDirect3DDevice, uint64 InitialValue)
{
	check(Fence == nullptr);

	VERIFYD3D11RESULT(pDirect3DDevice->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetInitReference())));
	LastCompletedFence = Fence->GetCompletedValue();
	CurrentFence = LastCompletedFence + 1;
}

uint64 FD3D12Fence::Signal(ID3D12CommandQueue* pCommandQueue)
{
	check(pCommandQueue != nullptr);

	VERIFYD3D11RESULT(pCommandQueue->Signal(Fence.GetReference(), CurrentFence));

	// Save the current fence and increment it
	const uint64 SignaledFence = CurrentFence++;

	// Return the value that was signaled
	return SignaledFence;
}

bool FD3D12Fence::IsFenceFinished(uint64 FenceValue)
{
	check(Fence != nullptr);

	// Avoid repeatedly calling GetCompletedValue()
	if (FenceValue <= LastCompletedFence)
		return true;

	// Refresh the completed fence value
	LastCompletedFence = Fence->GetCompletedValue();
	return FenceValue <= LastCompletedFence;
}

uint64 FD3D12Fence::GetLastCompletedFence()
{
	LastCompletedFence = Fence->GetCompletedValue();
	return LastCompletedFence;
}

void FD3D12Fence::WaitForFence(uint64 FenceValue)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12WaitForFenceTime);

	check(Fence != nullptr);

	if (IsFenceFinished(FenceValue))
		return;

	// We must wait.  Do so with an event handler so we don't oversleep.
	VERIFYD3D11RESULT(Fence->SetEventOnCompletion(FenceValue, hFenceCompleteEvent));

	// Wait for the event to complete (the event is automatically reset afterwards)
	const uint32 WaitResult = WaitForSingleObject(hFenceCompleteEvent, INFINITE);
	check(0 == WaitResult);

	LastCompletedFence = FenceValue;
}

FD3D12CommandListManager::FD3D12CommandListManager()
	: NumCommandListsAllocated(0)
{
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
		ReadyLists.Dequeue (hList);
}

void FD3D12CommandListManager::Create(ID3D12Device* InDirect3DDevice,  D3D12_COMMAND_LIST_TYPE InCommandListType, uint32 NumCommandLists)
{
	Direct3DDevice = InDirect3DDevice;
	CommandListType = InCommandListType;

	check(D3DCommandQueue.GetReference() == nullptr);
	check(ReadyLists.IsEmpty());
	checkf(NumCommandLists <= 0xffff, TEXT("Exceeded maximum supported command lists"));

    D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
    CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CommandQueueDesc.NodeMask = 0;
    CommandQueueDesc.Priority = 0;
    CommandQueueDesc.Type = CommandListType;
    VERIFYD3D11RESULT(Direct3DDevice->CreateCommandQueue(&CommandQueueDesc, IID_PPV_ARGS(D3DCommandQueue.GetInitReference())));

	for (uint32 i = 0; i < FT_NumTypes; i++)
	{
		Fences[i].CreateFence(Direct3DDevice, 0);
	}

	for (uint32 i = 0; i < NumCommandLists; ++i)
	{
		FD3D12CommandListHandle hList = CreateCommandListHandle();
		ReadyLists.Enqueue(hList);
	}
}

FD3D12CommandListHandle FD3D12CommandListManager::BeginCommandList(ID3D12PipelineState* InitialPSO)
{
    FD3D12CommandListHandle List;

	bool FoundFreeList = false;
	{
		// Need to lock in case of race conditions between the peek and dequeue
		FScopeLock Lock(&PeekReadyListCS);
		FD3D12Fence& Fence = Fences[FT_CommandList];
		FoundFreeList = ReadyLists.Peek(List);
		if (FoundFreeList && Fence.IsFenceFinished(List.FenceIndex()))
		{
			ReadyLists.Dequeue(List);
		}
		else
		{
			// Command List in the ReadyList is still pending, just allocate another command list instead of waiting
			FoundFreeList = false;
		}
	}

	if (!FoundFreeList)
	{
		List = CreateCommandListHandle();
	}

	List.Reset(InitialPSO);

	return List;
}

void FD3D12CommandListManager::SignalFrameComplete(bool WaitForCompletion)
{
	FD3D12Fence& Fence = Fences[FT_Frame];
	const uint64 SignaledFence = Fence.Signal(D3DCommandQueue);

	if (WaitForCompletion)
	{
		Fence.WaitForFence(SignaledFence);
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
	uint64 SignaledFence;
	int32 commandListIndex = 0;
	int32 barrierCommandListIndex = 0;

	// Close the resource barrier lists, get the raw command list pointers, and enqueue the command list handles
	// Note: All command lists will share the same fence
	check(Lists.Num() <= 128);
	ID3D12CommandList* pD3DCommandLists[128];
	FD3D12CommandListHandle BarrierCommandList[128];
	if (NeedsResourceBarriers)
	{
#if UE_BUILD_DEBUG	
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

			FD3D12CommandListHandle barrierCommandList = {};
			const uint32 numBarriers = GetResourceBarrierCommandList(commandList, barrierCommandList);
			if (numBarriers)
			{
				// TODO: Unnecessary assignment here, but fixing this will require refactoring GetResourceBarrierCommandList
				BarrierCommandList[barrierCommandListIndex] = barrierCommandList;
				barrierCommandListIndex++;

				check(barrierCommandList.FenceIndex() == (uint64)-1);
				barrierCommandList.Close();

				pD3DCommandLists[commandListIndex] = barrierCommandList.CommandList();
				commandListIndex++;

				check(commandListIndex < 127);
			}

			check(commandList.FenceIndex() == (uint64)-1);

			pD3DCommandLists[commandListIndex] = commandList.CommandList();
			commandListIndex++;
		}
		SignaledFence = ExecuteAndIncrementFence(pD3DCommandLists, commandListIndex, Fence);
	}
	else
	{
		for (int32 i = 0; i < Lists.Num(); i++)
		{
			FD3D12CommandListHandle& commandList = Lists[i];
			check(commandList.FenceIndex() == (uint64)-1);

			pD3DCommandLists[commandListIndex] = commandList.CommandList();
			commandListIndex++;
		}
		SignaledFence = ExecuteAndIncrementFence(pD3DCommandLists, commandListIndex, Fence);
	}

	for (int32 i = 0; i < Lists.Num(); i++)
	{
		FD3D12CommandListHandle& commandList = Lists[i];
		commandList.SetFenceIndex(SignaledFence);
		ReadyLists.Enqueue(commandList);
	}

	for (int32 i = 0; i < barrierCommandListIndex; i++)
	{
		FD3D12CommandListHandle& commandList = BarrierCommandList[i];
		commandList.SetFenceIndex(SignaledFence);
		ReadyLists.Enqueue(commandList);
	}

	if (WaitForCompletion)
	{
		Fence.WaitForFence(SignaledFence);
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
		D3D12_RESOURCE_BARRIER desc = {};
		desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		for (uint32 i = 0; i < NumPendingResourceBarriers; ++i)
		{
			const FD3D12PendingResourceBarrier& PRB = PendingResourceBarriers[i];

			// Should only be doing this for the few resources that need state tracking
			check(PRB.Resource->RequiresResourceStateTracking());

			CResourceState* pResourceState = PRB.Resource->GetResourceState();
			check(pResourceState);

			desc.Transition.Subresource = PRB.SubResource;
			const D3D12_RESOURCE_STATES before = pResourceState->GetSubresourceState(desc.Transition.Subresource);
			const D3D12_RESOURCE_STATES after = PRB.State;

			check(before != D3D12_RESOURCE_STATE_TBD && before != D3D12_RESOURCE_STATE_CORRUPT);
			if (before != after)
			{
				desc.Transition.pResource = PRB.Resource->GetResource();
				desc.Transition.StateBefore = before;
				desc.Transition.StateAfter = after;

				// Add the desc
				BarrierDescs.Add(desc);
			}

			// Update the state to the what it will be after hList executes
			const D3D12_RESOURCE_STATES LastState = hList.GetResourceState(PRB.Resource).GetSubresourceState(desc.Transition.Subresource);
			if (before != LastState)
			{
				pResourceState->SetSubresourceState(desc.Transition.Subresource, LastState);
			}
		}

		// Remove all pendering barriers from the command list
		PendingResourceBarriers.Reset();

		// Empty tracked resource state for this command list
		hList.EmptyTrackedResourceState();

		if (BarrierDescs.Num() > 0)
		{
			// MSFT: TODO: Use a special pool of "smaller" command lists+allocators, specifically for resource barriers. Makes the cost
			// of reseting the allocator less. Also consider opening the resource barrier command list while it's being
			// recorded on another thread.
			hResourceBarrierList = BeginCommandList(nullptr);

#if DEBUG_RESOURCE_STATES
			LogResourceBarriers(BarrierDescs.Num(), BarrierDescs.GetData(), hResourceBarrierList.CommandList());
#endif

			hResourceBarrierList->ResourceBarrier(BarrierDescs.Num(), BarrierDescs.GetData());
		}

		return BarrierDescs.Num();
	}

	return 0;
}

void FD3D12CommandListManager::WaitForCompletion(const FD3D12CLSyncPoint& hSyncPoint)
{
	// First compare the list handle's generation to the managed list's current generation.  If it's
	// an older generation, then we know it was completed a while ago.
	if ((hSyncPoint.Generation) < hSyncPoint.CommandList.CurrentGeneration())
		return;

	const uint64 ListFenceValue = hSyncPoint.CommandList.FenceIndex();
	checkf(ListFenceValue != (uint64)-1, TEXT("You can't wait for an unsubmitted command list to complete.  Kick first!"));

	FD3D12Fence& Fence = Fences[FT_CommandList];
	Fence.WaitForFence(ListFenceValue);
}

bool FD3D12CommandListManager::IsFinished(const FD3D12CLSyncPoint& hSyncPoint, uint64 FenceOffset)
{
	if (!hSyncPoint)
		return false;

    if (FenceOffset == 0 && hSyncPoint.Generation < hSyncPoint.CommandList.CurrentGeneration())
		return true;

	const uint64 ListFenceValue = hSyncPoint.CommandList.FenceIndex() + FenceOffset;
	FD3D12Fence& Fence = Fences[FT_CommandList];
	return Fence.IsFenceFinished(ListFenceValue);
}

CommandListState FD3D12CommandListManager::GetCommandListState(const FD3D12CLSyncPoint& hSyncPoint)
{
	check(hSyncPoint);

	// First compare the list handle's generation to the managed list's current generation.  If it's
	// an older generation, then we know it was completed a while ago.
	if (hSyncPoint.Generation < hSyncPoint.CommandList.CurrentGeneration())
		return CommandListState::kFinished;

	uint64 ListFenceValue = hSyncPoint.CommandList.FenceIndex();
	if (ListFenceValue == (uint64)-1)
		return CommandListState::kOpen;

	FD3D12Fence& Fence = Fences[FT_CommandList];
	return Fence.IsFenceFinished(ListFenceValue) ? CommandListState::kFinished : CommandListState::kQueued;
}

bool FD3D12CommandListManager::IsFinished(const FD3D12FrameSyncPoint& hSyncPoint)
{
	FD3D12Fence& Fence = Fences[FT_Frame];
	const uint64 FrameFenceValue = hSyncPoint.FrameFence;
	return Fence.IsFenceFinished(FrameFenceValue);
}

void FD3D12CommandListManager::WaitForCompletion(const FD3D12FrameSyncPoint& hSyncPoint)
{
	FD3D12Fence& Fence = Fences[FT_Frame];
	const uint64 FrameFenceValue = hSyncPoint.FrameFence;
	Fence.WaitForFence(FrameFenceValue);
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

FD3D12CommandListHandle FD3D12CommandListManager::CreateCommandListHandle()
{
	FD3D12CommandListHandle List = FD3D12CommandListHandle();
	List.Create(Direct3DDevice, CommandListType, this);
	const int64 CommandListCount = FPlatformAtomics::InterlockedIncrement(&NumCommandListsAllocated);

	// If we hit this, we should really evaluate why we need so many command lists, especially since
	// each command list is paired with an allocator. This can consume a lot of memory even if the command lists
	// are empty
	check(CommandListCount < MAX_ALLOCATED_COMMAND_LISTS);
	return List;
}