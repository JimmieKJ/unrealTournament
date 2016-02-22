// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

bool FD3D12Fence::IsFenceComplete(uint64 FenceValue)
{
	check(Fence != nullptr);

	// Avoid repeatedly calling GetCompletedValue()
	if (FenceValue <= LastCompletedFence)
	{
		return true;
	}

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

	if (IsFenceComplete(FenceValue))
	{
		return;
	}

	// We must wait.  Do so with an event handler so we don't oversleep.
	VERIFYD3D11RESULT(Fence->SetEventOnCompletion(FenceValue, hFenceCompleteEvent));

	// Wait for the event to complete (the event is automatically reset afterwards)
	const uint32 WaitResult = WaitForSingleObject(hFenceCompleteEvent, INFINITE);
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
		Fences[i].CreateFence(Direct3DDevice, 0);
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
			const uint32 numBarriers = GetResourceBarrierCommandList(commandList, barrierCommandList);
			if (numBarriers)
			{
				// TODO: Unnecessary assignment here, but fixing this will require refactoring GetResourceBarrierCommandList
				BarrierCommandList[barrierCommandListIndex] = barrierCommandList;
				barrierCommandListIndex++;

				barrierCommandList.Close();

				pD3DCommandLists[commandListIndex] = barrierCommandList.CommandList();
				commandListIndex++;

				check(commandListIndex < 127);
			}

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
			pD3DCommandLists[commandListIndex] = commandList.CommandList();
			commandListIndex++;
		}
		SignaledFence = ExecuteAndIncrementFence(pD3DCommandLists, commandListIndex, Fence);
	}

	FD3D12SyncPoint SyncPoint(&Fence, SignaledFence);

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
		commandList.SetSyncPoint(SyncPoint);
		ReleaseCommandList(commandList);
	}

	if (WaitForCompletion)
	{
		Fence.WaitForFence(SignaledFence);
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

void FD3D12CommandListManager::WaitForCompletion(const FD3D12CLSyncPoint& hSyncPoint)
{
	hSyncPoint.WaitForCompletion();
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
