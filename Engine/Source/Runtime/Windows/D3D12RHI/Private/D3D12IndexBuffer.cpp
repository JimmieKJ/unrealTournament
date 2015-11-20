// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12IndexBuffer.cpp: D3D Index buffer RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

FIndexBufferRHIRef FD3D12DynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Explicitly check that the size is nonzero before allowing CreateIndexBuffer to opaquely fail.
	check(Size > 0);

	// Describe the index buffer.
    const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;
    D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if (InUsage & BUF_UnorderedAccess)
	{
        Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if ((InUsage & BUF_ShaderResource) == 0)
	{
        Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	// If a resource array was provided for the resource, create the resource pre-populated
	D3D12_SUBRESOURCE_DATA InitData = { 0 };
	D3D12_SUBRESOURCE_DATA* pInitData = nullptr;
	if (CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		InitData.pData = CreateInfo.ResourceArray->GetResourceData();
        InitData.RowPitch = Size;
        InitData.SlicePitch = 0;
		pInitData = &InitData;
	}

	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation;
    if (bIsDynamic)
	{
		// Create an empty resource location
		ResourceLocation = new FD3D12ResourceLocation(GetRHIDevice(), Size);

		if (pInitData)
		{
			// Handle initial data
			void *pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().Alloc(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, ResourceLocation);
			FMemory::Memcpy(pData, InitData.pData, Size);
		}
	}
	else
	{
        ResourceLocation = new FD3D12ResourceLocation(GetRHIDevice(), Size);
        VERIFYD3D11RESULT(GetRHIDevice()->GetDefaultBufferAllocator().AllocDefaultResource(Desc, pInitData, ResourceLocation));
	}

	if (CreateInfo.ResourceArray)
	{
		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	UpdateBufferStats(ResourceLocation, true, D3D12_BUFFER_TYPE_INDEX);

	return new FD3D12IndexBuffer(GetRHIDevice(), ResourceLocation, Stride, Size, InUsage);
}

FD3D12IndexBuffer::~FD3D12IndexBuffer()
{
	UpdateBufferStats(ResourceLocation, false, D3D12_BUFFER_TYPE_INDEX);

	// Dynamic resources have a permanent entry in the OutstandingLocks that needs to be cleaned up. 
	if (GetUsage() & BUF_AnyDynamic)
	{
		FD3D12LockedKey LockedKey(ResourceLocation.GetReference());
		FD3D12DynamicRHI::GetD3DRHI()->OutstandingLocks.Remove (LockedKey);
	}
}

void* FD3D12DynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FD3D12IndexBuffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI);
	
    FD3D12CommandContext& DefaultContext = GetRHIDevice()->GetDefaultCommandContext();

	// If this resource is bound to the device, unbind it
	DefaultContext.ConditionalClearShaderResource(IndexBuffer->ResourceLocation);

	// Determine whether the index buffer is dynamic or not.
	const bool bIsDynamic = (IndexBuffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	FD3D12LockedKey LockedKey(IndexBuffer->ResourceLocation.GetReference());
	FD3D12LockedData LockedData;
    uint32 ResourceSize = IndexBuffer->ResourceLocation->GetEffectiveBufferSize();

	if(bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly);

		// Use an upload heap for dynamic resources and map its memory for writing.
        void *pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().Alloc(Offset + Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, IndexBuffer->ResourceLocation.GetReference());
		check(pData);

		// Add the lock to the lock map.
		FD3D12LockedKey LockedKey(IndexBuffer->ResourceLocation.GetReference());
		FD3D12LockedData LockedData;
		LockedData.SetData(pData);
		LockedData.Pitch = Offset + Size;
		OutstandingLocks.Add(LockedKey, LockedData);

		return (void *) ((uint8 *) pData + Offset);
	}
	else
	{
		if(LockMode == RLM_ReadOnly)
		{
			FD3D12Resource *Resource = IndexBuffer->ResourceLocation->GetResource();

			// If the static buffer is being locked for reading, create a staging buffer.
			TRefCountPtr<FD3D12Resource> StagingIndexBuffer;
            VERIFYD3D11RESULT(GetRHIDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_READBACK, Offset + Size, StagingIndexBuffer.GetInitReference()));
			LockedData.StagingResource = StagingIndexBuffer;

			// Copy the contents of the index buffer to the staging buffer.
			DefaultContext.numCopies++;
			DefaultContext.CommandListHandle->CopyResource(StagingIndexBuffer->GetResource(), Resource->GetResource());

			// Execute the command list and wait for the contents to be ready so it can be returned to the caller
            GetRHIDevice()->GetDefaultCommandContext().FlushCommands(true);

			// Map the staging buffer's memory for reading.
			void* pData;
			VERIFYD3D11RESULT(StagingIndexBuffer->GetResource()->Map(0, nullptr, &pData));
			LockedData.SetData(pData);
			LockedData.Pitch = Offset + Size;
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.

			// Use an upload heap to copy data to a default resource.
			TRefCountPtr<FD3D12ResourceLocation> UploadBuffer = new FD3D12ResourceLocation();
            void *pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().FastAlloc(Offset + Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, UploadBuffer);

			// Add the lock to the lock map.
			LockedData.SetData(pData);
			LockedData.Pitch = Offset + Size;

			// Keep track of the underlying resource for the upload heap so it can be referenced during Unmap.
			LockedData.UploadHeapLocation = UploadBuffer;
		}
	}
	
	// Add the lock to the lock map.
	OutstandingLocks.Add(LockedKey,LockedData);

	// Return the offset pointer
	return (void*)((uint8*)LockedData.GetData() + Offset);
}

void FD3D12DynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FD3D12IndexBuffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI);

	// Determine whether the index buffer is dynamic or not.
	const bool bIsDynamic = (IndexBuffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	// Find the outstanding lock for this IB.
	FD3D12LockedKey LockedKey(IndexBuffer->ResourceLocation.GetReference());
	FD3D12LockedData* LockedData = OutstandingLocks.Find(LockedKey);
	check(LockedData);

	FD3D12Resource *Resource = IndexBuffer->ResourceLocation->GetResource();

	if(bIsDynamic)
	{
		// If the IB is dynamic, its upload heap memory can always stay mapped. Don't do anything.
	}
	else
	{
		// If the static IB lock involved a staging resource, it was locked for reading.
		if(LockedData->StagingResource)
		{
			// Unmap the staging buffer's memory.
			FD3D12Resource* StagingBuffer = LockedData->StagingResource.GetReference();
			StagingBuffer->GetResource()->Unmap(0, nullptr);
		}
		else 
		{
			check(LockedData->UploadHeapLocation != nullptr);

			// Copy the contents of the temporary memory buffer allocated for writing into the IB.
			FD3D12ResourceLocation* UploadHeap = LockedData->UploadHeapLocation;

			FD3D12CommandListHandle& hCommandList = GetRHIDevice()->GetDefaultCommandContext().CommandListHandle;

			{
				FScopeResourceBarrier ScopeResourceBarrier(hCommandList, Resource, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST, 0);
				// Don't need to transition upload heaps

				GetRHIDevice()->GetDefaultCommandContext().numCopies++;
				hCommandList->CopyBufferRegion(
					Resource->GetResource(), IndexBuffer->ResourceLocation->GetOffset(),
					UploadHeap->GetResource()->GetResource(), UploadHeap->GetOffset(),
					LockedData->Pitch);
			}

			
			DEBUG_RHI_EXECUTE_COMMAND_LIST(this);
		}
	}

	// Remove the FD3D12LockedData from the lock map.
	// If the lock involved a staging resource, this releases it.
	// If the lock involved an upload heap resource, this releases it.
	OutstandingLocks.Remove(LockedKey);
}
