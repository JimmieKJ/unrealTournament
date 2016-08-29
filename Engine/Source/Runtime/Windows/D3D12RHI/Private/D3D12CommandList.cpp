// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12CommandList.h"


void FD3D12CommandListHandle::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddTransition(pResource->GetResource(), Before, After, Subresource);
	CommandListData->CurrentOwningContext->numBarriers++;

	pResource->UpdateResidency(*this);
}

void FD3D12CommandListHandle::AddUAVBarrier()
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddUAV();
	CommandListData->CurrentOwningContext->numBarriers++;
}

void FD3D12CommandListHandle::Create(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
{
	check(!CommandListData);

	CommandListData = new FD3D12CommandListData(ParentDevice, CommandListType, CommandAllocator, InCommandListManager);

	CommandListData->AddRef();
}

FD3D12CommandListHandle::FD3D12CommandListData::FD3D12CommandListData(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
	: CommandListManager(InCommandListManager)
	, CurrentGeneration(1)
	, LastCompleteGeneration(0)
	, IsClosed(false)
	, PendingResourceBarriers()
	, CurrentOwningContext(nullptr)
	, CurrentCommandAllocator(&CommandAllocator)
	, CommandListType(InCommandListType)
	, ResidencySet(nullptr)
	, FD3D12DeviceChild(ParentDevice)
{
	VERIFYD3D12RESULT(ParentDevice->GetDevice()->CreateCommandList(0, CommandListType, CommandAllocator.GetCommandAllocator(), nullptr, IID_PPV_ARGS(CommandList.GetInitReference())));

	// Initially start with all lists closed.  We'll open them as we allocate them.
	Close();

	PendingResourceBarriers.Reserve(256);

	ResidencySet = D3DX12Residency::CreateResidencySet(ParentDevice->GetResidencyManager());
}

FD3D12CommandListHandle::FD3D12CommandListData::~FD3D12CommandListData()
{
	CommandList.SafeRelease();

	D3DX12Residency::DestroyResidencySet(GetParentDevice()->GetResidencyManager(), ResidencySet);
}