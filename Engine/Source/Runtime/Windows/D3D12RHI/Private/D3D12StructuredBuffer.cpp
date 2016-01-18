// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"

FStructuredBufferRHIRef FD3D12DynamicRHI::RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Explicitly check that the size is nonzero before allowing CreateStructuredBuffer to opaquely fail.
	check(Size > 0);
	// Check for values that will cause D3D calls to fail
	check(Size / Stride > 0 && Size % Stride == 0);

    // Describe the structured buffer.
    const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;
    D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

    if ((InUsage & BUF_ShaderResource) == 0)
    {
        Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }

    if (InUsage & BUF_UnorderedAccess)
    {
        Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

	if (FPlatformProperties::SupportsFastVRAMMemory())
	{
		if (InUsage & BUF_FastVRAM)
		{
			FFastVRAMAllocator::GetFastVRAMAllocator()->AllocUAVBuffer(Desc);
		}
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

	TRefCountPtr<FD3D12ResourceLocation> StructuredBufferResource;
    if (bIsDynamic)
	{
		// Use an upload heap for dynamic resources and map its memory for writing.
		void* pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().Alloc(Desc.Width, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, StructuredBufferResource.GetInitReference());

		// Add the lock to the lock map.
		FD3D12LockedKey LockedKey(StructuredBufferResource.GetReference());
		FD3D12LockedData LockedData;
		LockedData.SetData(pData);
		LockedData.Pitch = Desc.Width;
		OutstandingLocks.Add(LockedKey, LockedData);

		if (pInitData)
		{
			memcpy(pData, InitData.pData, Desc.Width);
		}
	}
	else
	{
#if SUB_ALLOCATED_DEFAULT_ALLOCATIONS
		// Structured buffers, non-byte address buffers, need to be aligned to their stride to ensure that they
		// can be addressed correctly with element based offsets.
		if ((InUsage & (BUF_ByteAddressBuffer | BUF_DrawIndirect)) == 0)
			GetRHIDevice()->GetDefaultBufferAllocator().AllocDefaultResource(Desc, pInitData, StructuredBufferResource.GetInitReference(), Stride);

		else
#endif
		GetRHIDevice()->GetDefaultBufferAllocator().AllocDefaultResource(Desc, pInitData, StructuredBufferResource.GetInitReference());
	}

	UpdateBufferStats(StructuredBufferResource, true, D3D12_BUFFER_TYPE_STRUCTURED);

	if (CreateInfo.ResourceArray)
	{
		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return new FD3D12StructuredBuffer(GetRHIDevice(), StructuredBufferResource, Stride, Size, InUsage);
}

FD3D12StructuredBuffer::~FD3D12StructuredBuffer()
{
	UpdateBufferStats(ResourceLocation, false, D3D12_BUFFER_TYPE_STRUCTURED);

	// Dynamic resources have a permanent entry in the OutstandingLocks that needs to be cleaned up. 
	if (GetUsage() & BUF_AnyDynamic)
	{
		FD3D12LockedKey LockedKey(ResourceLocation.GetReference());
		FD3D12DynamicRHI::GetD3DRHI()->OutstandingLocks.Remove (LockedKey);
	}
}

void* FD3D12DynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FD3D12StructuredBuffer*  StructuredBuffer = FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI);

    FD3D12CommandContext& DefaultContext = GetRHIDevice()->GetDefaultCommandContext();

	// If this resource is bound to the device, unbind it
	DefaultContext.ConditionalClearShaderResource(StructuredBuffer->ResourceLocation);

	// Determine whether the Structured buffer is dynamic or not.
	D3D12_RESOURCE_DESC const& Desc = StructuredBuffer->Resource->GetDesc();
    const uint32 BufferUsage = StructuredBuffer->GetUsage();
    const bool bIsDynamic = (BufferUsage & BUF_AnyDynamic) ? true : false;

	FD3D12LockedKey LockedKey(StructuredBuffer->Resource.GetReference());
	FD3D12LockedData LockedData;

	if (bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly);

		// The buffer is dynamic, so we've already mapped it. Just return the offset pointer.
		FD3D12LockedData* pLockedData = OutstandingLocks.Find(LockedKey);
		check(pLockedData != nullptr);
		return (void*)((uint8*)pLockedData->GetData() + Offset);
	}
	else
	{
		if (LockMode == RLM_ReadOnly)
		{
			// If the static buffer is being locked for reading, create a staging buffer.
			TRefCountPtr<FD3D12Resource> StagingStructuredBuffer;
            VERIFYD3D11RESULT(GetRHIDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_READBACK, Offset + Size, StagingStructuredBuffer.GetInitReference()));
			LockedData.StagingResource = StagingStructuredBuffer;

			// Copy the contents of the Structured buffer to the staging buffer.
			DefaultContext.numCopies++;
			DefaultContext.CommandListHandle->CopyResource(StagingStructuredBuffer->GetResource(), StructuredBuffer->Resource->GetResource());

            GetRHIDevice()->GetDefaultCommandContext().FlushCommands(true);

			// Map the staging buffer's memory for reading.
			void* pData;
			VERIFYD3D11RESULT(StagingStructuredBuffer->GetResource()->Map(0, nullptr, &pData));
			LockedData.SetData(pData);
			LockedData.Pitch = Offset + Size;
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			check(StructuredBuffer->ResourceLocation->GetEffectiveBufferSize() >= Size);

			// Use an upload heap to copy data to a default resource.
			TRefCountPtr<FD3D12ResourceLocation> UploadBuffer = new FD3D12ResourceLocation();
            void *pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().FastAlloc(Offset + Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, UploadBuffer);

			// Add the lock to the lock map.
			FD3D12LockedKey LockedKey(UploadBuffer);
			LockedData.SetData(pData);
			LockedData.Pitch = Offset + Size;

			// Keep track of the underlying resource for the upload heap so it can be referenced during Unmap.
			LockedData.UploadHeapLocation = UploadBuffer;
		}
	}

	// Add the lock to the lock map.
	OutstandingLocks.Add(LockedKey, LockedData);

	// Return the offset pointer
	return (void*)((uint8*)LockedData.GetData() + Offset);
}

void FD3D12DynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FD3D12StructuredBuffer*  StructuredBuffer = FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI);

	// Determine whether the Structured buffer is dynamic or not.
	D3D12_RESOURCE_DESC const& Desc = StructuredBuffer->Resource->GetDesc();
    const uint32 BufferUsage = StructuredBuffer->GetUsage();
    const bool bIsDynamic = (BufferUsage & BUF_AnyDynamic) ? true : false;

	// Find the outstanding lock for this VB.
	FD3D12LockedKey LockedKey(StructuredBuffer->Resource.GetReference());
	FD3D12LockedData* LockedData = OutstandingLocks.Find(LockedKey);
	check(LockedData);

	if (bIsDynamic)
	{
		// If the VB is dynamic, its upload heap memory can always stay mapped. Don't do anything.
	}
	else
	{
		// If the static VB lock involved a staging resource, it was locked for reading.
		if (LockedData->StagingResource)
		{
			// Unmap the staging buffer's memory.
			ID3D12Resource* StagingBuffer = LockedData->StagingResource.GetReference()->GetResource();
			StagingBuffer->Unmap(0, nullptr);
		}
		else
		{
			check(LockedData->UploadHeapLocation != nullptr);

			// Copy the contents of the temporary memory buffer allocated for writing into the VB.
			FD3D12ResourceLocation* UploadHeap = LockedData->UploadHeapLocation;

			FD3D12CommandListHandle& hCommandList = GetRHIDevice()->GetDefaultCommandContext().CommandListHandle;

			// Writable structured bufferes are sometimes unlocked which means they sometimes need tracking.
			FConditionalScopeResourceBarrier ConditionalScopeResourceBarrier(hCommandList, StructuredBuffer->Resource, D3D12_RESOURCE_STATE_COPY_DEST, 0);
			// Don't need to transition upload heaps

			GetRHIDevice()->GetDefaultCommandContext().numCopies++;
			hCommandList->CopyBufferRegion(
                StructuredBuffer->Resource->GetResource(),
				StructuredBuffer->ResourceLocation->GetOffset(),
				UploadHeap->GetResource()->GetResource(),
				UploadHeap->GetOffset(),
				LockedData->Pitch);
		}
	}

	// Remove the FD3D12LockedData from the lock map.
	// If the lock involved a staging resource, this releases it.
	// If the lock involved an upload heap resource, this releases it.
	OutstandingLocks.Remove(LockedKey);
}
