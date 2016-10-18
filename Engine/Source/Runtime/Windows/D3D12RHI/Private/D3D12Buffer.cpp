// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Buffer.cpp: D3D Common code for buffers.
=============================================================================*/

#include "D3D12RHIPrivate.h"

// Forward declarations are required for the template functions
template void* FD3D12DynamicRHI::LockBuffer<FD3D12VertexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12VertexBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);
template void* FD3D12DynamicRHI::LockBuffer<FD3D12IndexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12IndexBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);
template void* FD3D12DynamicRHI::LockBuffer<FD3D12StructuredBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12StructuredBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);

template void FD3D12DynamicRHI::UnlockBuffer<FD3D12VertexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12VertexBuffer* Buffer);
template void FD3D12DynamicRHI::UnlockBuffer<FD3D12IndexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12IndexBuffer* Buffer);
template void FD3D12DynamicRHI::UnlockBuffer<FD3D12StructuredBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12StructuredBuffer* Buffer);

struct FRHICommandUpdateBuffer : public FRHICommand<FRHICommandUpdateBuffer>
{
	TRefCountPtr<FD3D12ResourceLocation> Source;
	TRefCountPtr<FD3D12ResourceLocation> Destination;
	uint32 NumBytes;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateBuffer(TRefCountPtr<FD3D12ResourceLocation> InDest, TRefCountPtr<FD3D12ResourceLocation> InSource, uint32 InNumBytes) :
		Source(InSource), Destination(InDest), NumBytes(InNumBytes) {}

	void Execute(FRHICommandListBase& CmdList)
	{
		FD3D12DynamicRHI::GetD3DRHI()->UpdateBuffer(Destination->GetResource(), Destination->GetOffset(), Source->GetResource(), Source->GetOffset(), NumBytes);
	}
};

// This allows us to rename resources from the RenderThread i.e. all the 'hard' work of allocating a new resource
// is done in parallel and this small function is called to switch the resource to point to the correct location
// a the correct time.
template<typename ResourceType>
struct FRHICommandRenameUploadBuffer : public FRHICommand<FRHICommandRenameUploadBuffer<ResourceType>>
{
	ResourceType* Resource;
	TRefCountPtr<FD3D12ResourceLocation> NewResource;
	FORCEINLINE_DEBUGGABLE FRHICommandRenameUploadBuffer(ResourceType* InResource, FD3D12ResourceLocation* NewResource) :
		Resource(InResource), NewResource(NewResource) {}

	void Execute(FRHICommandListBase& CmdList) { Resource->Rename(NewResource); }
};

FD3D12ResourceLocation* FD3D12DynamicRHI::CreateBuffer(FRHICommandListImmediate* RHICmdList, const D3D12_RESOURCE_DESC InDesc, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, uint32 Alignment)
{
	// Explicitly check that the size is nonzero before allowing CreateBuffer to opaquely fail.
	check(Size > 0);

	const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;

	FD3D12ResourceLocation* ResourceLocation = new FD3D12ResourceLocation(GetRHIDevice(), Size);

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

	if (bIsDynamic)
	{
		void* pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().AllocUploadResource(Size, Alignment, ResourceLocation);
		check(ResourceLocation->GetEffectiveBufferSize() == Size);

		if (pInitData)
		{
			// Handle initial data
			FMemory::Memcpy(pData, InitData.pData, Size);
		}
	}
	else
	{
		if (RHICmdList && pInitData)
		{
			// We only need to synchronize when creating default resource buffers (because we need a command list to initialize them)
			FScopedRHIThreadStaller StallRHIThread(*RHICmdList);

			VERIFYD3D12RESULT(GetRHIDevice()->GetDefaultBufferAllocator().AllocDefaultResource(InDesc, pInitData, ResourceLocation, Alignment));
		}
		else
		{
			VERIFYD3D12RESULT(GetRHIDevice()->GetDefaultBufferAllocator().AllocDefaultResource(InDesc, pInitData, ResourceLocation, Alignment));
		}
		check(ResourceLocation->GetEffectiveBufferSize() == Size);
	}

	if (CreateInfo.ResourceArray)
	{
		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return ResourceLocation;
}

template<class BufferType>
void* FD3D12DynamicRHI::LockBuffer(FRHICommandListImmediate* RHICmdList, BufferType* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FD3D12LockedData LockedData;

	// Determine whether the buffer is dynamic or not.
	const bool bIsDynamic = (Buffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	void* Data = nullptr;

	if (bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly);

		TRefCountPtr<FD3D12ResourceLocation> NewLocation = new FD3D12ResourceLocation(GetRHIDevice(), Size);

		// Allocate a new resource
		Data = GetRHIDevice()->GetDefaultUploadHeapAllocator().AllocUploadResource(Buffer->GetSize(), Buffer->BufferAlignment, NewLocation);

		// If on the RenderThread, queue up a command on the RHIThread to rename this buffer at the correct time
		if (ShouldDeferBufferLockOperation(RHICmdList))
		{
			new (RHICmdList->AllocCommand<FRHICommandRenameUploadBuffer<BufferType>>()) FRHICommandRenameUploadBuffer<BufferType>(Buffer, NewLocation);
		}
		else
		{
			Buffer->Rename(NewLocation);
		}
	}
	else
	{
		FD3D12Resource* pResource = Buffer->ResourceLocation->GetResource();

		// Locking for read must occur immediately so we can't queue up the operations later.
		if (LockMode == RLM_ReadOnly)
		{
			// If the static buffer is being locked for reading, create a staging buffer.
			TRefCountPtr<FD3D12Resource> StagingBuffer;
			VERIFYD3D12RESULT(GetRHIDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_READBACK, Offset + Size, StagingBuffer.GetInitReference()));
			LockedData.StagingResource = StagingBuffer;

			// Copy the contents of the buffer to the staging buffer.
			{
				const auto& pfnCopyContents = [&]()
				{
					FD3D12CommandContext& DefaultContext = GetRHIDevice()->GetDefaultCommandContext();

					FD3D12CommandListHandle& hCommandList = DefaultContext.CommandListHandle;
					FScopeResourceBarrier ScopeResourceBarrierSource(hCommandList, pResource, pResource->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
					// Don't need to transition upload heaps

					DefaultContext.numCopies++;
					hCommandList->CopyBufferRegion(
						StagingBuffer->GetResource(),
						0,
						pResource->GetResource(),
						Offset, Size);

					hCommandList.UpdateResidency(StagingBuffer);
					hCommandList.UpdateResidency(pResource);

					DefaultContext.FlushCommands(true);
				};

				if (ShouldDeferBufferLockOperation(RHICmdList))
				{
					// Sync when in the render thread implementation
					check(IsInRHIThread() == false);

					RHICmdList->ImmediateFlush(EImmediateFlushType::FlushRHIThread);
					pfnCopyContents();
				}
				else
				{
					check(IsInRHIThread());
					pfnCopyContents();
				}
			}

			// Map the staging buffer's memory for reading.
			VERIFYD3D12RESULT(StagingBuffer->GetResource()->Map(0, nullptr, &Data));
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			TRefCountPtr<FD3D12ResourceLocation> UploadBufferLocation = new FD3D12ResourceLocation(GetRHIDevice());
			Data = GetRHIDevice()->GetDefaultFastAllocator().Allocate(Offset + Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, UploadBufferLocation);

			// Keep track of the underlying resource for the upload heap so it can be referenced during Unmap.
			LockedData.UploadHeapLocation = UploadBufferLocation;
		}
	}

	check(Data);
	// Add the lock to the lock map.
	LockedData.SetData(Data);
	LockedData.Pitch = Offset + Size;

	// Add the lock to the lock map.
	FD3D12LockedKey LockedKey(Buffer);
	AddToOutstandingLocks(LockedKey, LockedData);

	// Return the offset pointer
	return (void*)((uint8*)LockedData.GetData() + Offset);
}

template<class BufferType>
void FD3D12DynamicRHI::UnlockBuffer(FRHICommandListImmediate* RHICmdList, BufferType* Buffer)
{
	// Find the outstanding lock for this Buffer.
	FD3D12LockedKey LockedKey(Buffer);
	FD3D12LockedData* LockedData = FindInOutstandingLocks(LockedKey);
	check(LockedData);

	// Determine whether the buffer is dynamic or not.
	const bool bIsDynamic = (Buffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	if (bIsDynamic)
	{
		// If the Buffer is dynamic, its upload heap memory can always stay mapped. Don't do anything.
	}
	else
	{
		// If the static Buffer lock involved a staging resource, it was locked for reading.
		if (LockedData->StagingResource)
		{
			// Unmap the staging buffer's memory.
			ID3D12Resource* StagingBuffer = LockedData->StagingResource.GetReference()->GetResource();
			StagingBuffer->Unmap(0, nullptr);
		}
		else
		{
			// Copy the contents of the temporary memory buffer allocated for writing into the Buffer.
			FD3D12ResourceLocation* UploadHeapLocation = LockedData->UploadHeapLocation.GetReference();

			// If we are on the render thread, queue up the copy on the RHIThread so it happens at the correct time.
			if (ShouldDeferBufferLockOperation(RHICmdList))
			{
				new (RHICmdList->AllocCommand<FRHICommandUpdateBuffer>()) FRHICommandUpdateBuffer(Buffer->ResourceLocation, UploadHeapLocation, LockedData->Pitch);
			}
			else
			{
				UpdateBuffer(Buffer->ResourceLocation->GetResource(),
					Buffer->ResourceLocation->GetOffset(), UploadHeapLocation->GetResource(), UploadHeapLocation->GetOffset(), LockedData->Pitch);
			}
		}
	}

	// Remove the FD3D12LockedData from the lock map.
	// If the lock involved a staging resource, this releases it.
	RemoveFromOutstandingLocks(LockedKey);
}
