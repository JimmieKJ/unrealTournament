// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Resources.cpp: D3D RHI utility implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "EngineModule.h"


FD3D12DeferredDeletionQueue::FD3D12DeferredDeletionQueue(FD3D12Device* InParent) :
    FD3D12DeviceChild(InParent){}

bool FD3D12DeferredDeletionQueue::ReleaseResources()
{
	struct FDequeueFenceObject
	{
		FDequeueFenceObject(FD3D12CommandListManager* InCommandListManager)
			: CommandListManager(InCommandListManager)
		{
		}

		bool operator() (FencedObjectType FenceObject)
		{
			return CommandListManager->GetFence(FT_Frame).IsFenceFinished(FenceObject.Value);
		}

		FD3D12CommandListManager* CommandListManager;
	};

	FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();

	FencedObjectType FenceObject;
	FDequeueFenceObject DequeueFenceObject(&CommandListManager);

	const uint64 LastCompletedFrameFence = CommandListManager.GetFence(EFenceType::FT_Frame).GetLastCompletedFence();
	while (DeferredReleaseQueue.Dequeue(FenceObject, DequeueFenceObject))
	{
		FenceObject.Key->Release();
	}

	return DeferredReleaseQueue.IsEmpty();
}

#if UE_BUILD_DEBUG
int64 FD3D12Resource::TotalResourceCount = 0;
int64 FD3D12Resource::NoStateTrackingResourceCount = 0;
#endif

FD3D12Resource::~FD3D12Resource()
{
#if SUPPORTS_MEMORY_RESIDENCY
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT)
		FD3D12DynamicRHI::GetD3DRHI()->GetResourceResidencyManager().ResourceFreed (this);
#endif

	if (pResourceState)
	{
		delete pResourceState;
		pResourceState = nullptr;
	}
}

#if SUPPORTS_MEMORY_RESIDENCY
void FD3D12Resource::UpdateResourceSize()
{
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT)
		ResourceSize = (uint32)GetRequiredIntermediateSize(GetResource(), 0, Desc.MipLevels);
}

void FD3D12Resource::UpdateResidency()
{
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT)
		FD3D12DynamicRHI::GetD3DRHI()->GetResourceResidencyManager().UpdateResidency (this);
}
#endif

void inline FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState)
{
	// If there is no entry, all subresources should be in the resource's TBD state.
	// This means we need to have pending resource barrier(s).
	if (!ResourceState.CheckResourceStateInitalized())
	{
		ResourceState.Initialize(pResource->GetSubresourceCount());
		check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	}

	check(ResourceState.CheckResourceStateInitalized());
}

CResourceState& FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::GetResourceState(FD3D12Resource* pResource)
{
	// Only certain resources should use this
	check(pResource->RequiresResourceStateTracking());

	CResourceState& ResourceState = ResourceStates.FindOrAdd(pResource);
	ConditionalInitalize(pResource, ResourceState);
	return ResourceState;
}

void FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::Empty()
{
	ResourceStates.Empty();
}

void FD3D12CommandListHandle::Execute (bool WaitForCompletion)
{
	check (CommandListData);
	CommandListData->CommandListManager->ExecuteCommandList(*this, WaitForCompletion);
}