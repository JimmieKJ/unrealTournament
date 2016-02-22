// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12VertexBuffer.cpp: D3D vertex buffer RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"

FVertexBufferRHIRef FD3D12DynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Explicitly check that the size is nonzero before allowing CreateVertexBuffer to opaquely fail.
	check(Size > 0);

	// Describe the vertex buffer.
	const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if (InUsage & BUF_UnorderedAccess)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		static bool bRequiresRawView = (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5);
		if (bRequiresRawView)
		{
			// Force the buffer to be a raw, byte address buffer
			InUsage |= BUF_ByteAddressBuffer;
		}
	}

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (FPlatformProperties::SupportsFastVRAMMemory())
	{
		if (InUsage & BUF_FastVRAM)
		{
			FFastVRAMAllocator::GetFastVRAMAllocator()->AllocUAVBuffer(Desc);
		}
	}

	// If a resource array was provided for the resource, create the resource pre-populated
	D3D12_SUBRESOURCE_DATA InitData ={0};
	D3D12_SUBRESOURCE_DATA* pInitData = nullptr;
	if (CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		InitData.pData = CreateInfo.ResourceArray->GetResourceData();
		InitData.RowPitch = Size;
		InitData.SlicePitch = 0;
		pInitData = &InitData;
	}

	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation = new FD3D12ResourceLocation(GetRHIDevice());
	if (bIsDynamic)
	{
		void* pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().AllocUploadResource(Size, DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT, ResourceLocation);

		if (pInitData)
		{
			// Handle initial data
			FMemory::Memcpy(pData, InitData.pData, Size);
		}
		check(ResourceLocation->GetEffectiveBufferSize() == Size);
	}
	else
	{
		// Create an empty resource location
		VERIFYD3D11RESULT(GetRHIDevice()->GetDefaultBufferAllocator().AllocDefaultResource(Desc, pInitData, ResourceLocation.GetInitReference()));
		check(ResourceLocation->GetEffectiveBufferSize() == Size);
	}

	if (CreateInfo.ResourceArray)
	{
		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return new FD3D12VertexBuffer(GetRHIDevice(), ResourceLocation, Size, InUsage);
}

FD3D12VertexBuffer::~FD3D12VertexBuffer()
{
	if (ResourceLocation->GetResource() != nullptr)
	{
		UpdateBufferStats(ResourceLocation, false, D3D12_BUFFER_TYPE_VERTEX);
	}

	// Dynamic resources have a permanente entry in the OutstandingLocks that needs to be cleaned up. 
	if (GetUsage() & BUF_AnyDynamic)
	{
		FD3D12LockedKey LockedKey(ResourceLocation.GetReference());
		FD3D12DynamicRHI::GetD3DRHI()->OutstandingLocks.Remove(LockedKey);
	}
}

void* FD3D12VertexBuffer::DynamicLock()
{
	void* pData = nullptr;
	check((GetUsage() & BUF_AnyDynamic) ? true : false);

	FD3D12DynamicHeapAllocator &Allocator = GetParentDevice()->GetDefaultUploadHeapAllocator();
	pData = Allocator.AllocUploadResource(ResourceLocation->GetEffectiveBufferSize(), DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT, ResourceLocation);

	if (DynamicSRV != nullptr)
	{
		// This will force a new descriptor to be created
		CD3DX12_CPU_DESCRIPTOR_HANDLE Desc;
		Desc.ptr = 0;
		DynamicSRV->Rename(ResourceLocation, Desc, 0);
	}

	check(pData);

	return pData;
}

void* FD3D12DynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	// TEMP: Disable check due to engine bug where it passes Size = 0 when doing parallel rendering.
	//check(Size > 0);

	FD3D12VertexBuffer*  VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);

	FD3D12CommandContext& DefaultContext = GetRHIDevice()->GetDefaultCommandContext();
	FD3D12CommandListHandle& hCommandList = DefaultContext.CommandListHandle;

	// If this resource is bound to the device, unbind it
	DefaultContext.ConditionalClearShaderResource(VertexBuffer->ResourceLocation);

	// Determine whether the vertex buffer is dynamic or not.
	const bool bIsDynamic = (VertexBuffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	FD3D12LockedKey LockedKey(VertexBuffer->ResourceLocation.GetReference());
	FD3D12LockedData LockedData;

	if (bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly);

		void* pData = VertexBuffer->DynamicLock();

		// Add the lock to the lock map.
		LockedData.SetData(pData);
		LockedData.Pitch = Size;
		OutstandingLocks.Add(LockedKey, LockedData);

		return (void*)((uint8*)pData + Offset);
	}
	else
	{
		FD3D12Resource* pResource = VertexBuffer->ResourceLocation->GetResource();
		uint32 ResourceSize = VertexBuffer->ResourceLocation->GetEffectiveBufferSize();

		if (LockMode == RLM_ReadOnly)
		{
			// If the static buffer is being locked for reading, create a staging buffer.
			TRefCountPtr<FD3D12Resource> StagingVertexBuffer;
			VERIFYD3D11RESULT(GetRHIDevice()->GetResourceHelper().CreateBuffer(D3D12_HEAP_TYPE_READBACK, Offset + Size, StagingVertexBuffer.GetInitReference()));
			LockedData.StagingResource = StagingVertexBuffer;

			// Copy the contents of the vertex buffer to the staging buffer.
			{
				FScopeResourceBarrier ScopeResourceBarrierSource(hCommandList, pResource, pResource->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
				// Don't need to transition upload heaps

				DefaultContext.numCopies++;
				hCommandList->CopyBufferRegion(
					StagingVertexBuffer->GetResource(),
					0,
					pResource->GetResource(),
					Offset, Size);
			}

			GetRHIDevice()->GetDefaultCommandContext().FlushCommands(true);

			// Map the staging buffer's memory for reading.
			void* pData;
			VERIFYD3D11RESULT(StagingVertexBuffer->GetResource()->Map(0, nullptr, &pData));
			LockedData.SetData(pData);
			LockedData.Pitch = Offset + Size;
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.

			// Use an upload heap to copy data to a default resource.

			// Use an upload heap for dynamic resources and map its memory for writing.
			TRefCountPtr<FD3D12ResourceLocation> pUploadBufferLocation = new FD3D12ResourceLocation(GetRHIDevice());
			void* pData = GetRHIDevice()->GetDefaultFastAllocator().Allocate(Offset + Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, pUploadBufferLocation);

			if (nullptr == pData)
			{
				return nullptr;
			}


			// Add the lock to the lock map.
			FD3D12LockedData LockedData;
			LockedData.SetData(pData);
			LockedData.Pitch = Offset + Size;

			// Keep track of the underlying resource for the upload heap so it can be referenced during Unmap.
			LockedData.UploadHeapLocation = pUploadBufferLocation;
			OutstandingLocks.Add(LockedKey, LockedData);

			return (void*)((uint8*)pData + Offset);
		}
	}

	// Add the lock to the lock map.
	OutstandingLocks.Add(LockedKey, LockedData);

	// Return the offset pointer
	return (void*)((uint8*)LockedData.GetData() + Offset);
}

void FD3D12DynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	FD3D12VertexBuffer*  VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);

	// Determine whether the vertex buffer is dynamic or not.
	const bool bIsDynamic = (VertexBuffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	// Find the outstanding lock for this VB.
	FD3D12LockedKey LockedKey(VertexBuffer->ResourceLocation);
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
			FD3D12Resource* pResource = VertexBuffer->ResourceLocation->GetResource();

			// Copy the contents of the temporary memory buffer allocated for writing into the IB.
			FD3D12ResourceLocation* pUploadHeapLocation = LockedData->UploadHeapLocation.GetReference();
			FD3D12Resource* UploadHeap = pUploadHeapLocation->GetResource();

			FD3D12CommandContext& defaultContext = GetRHIDevice()->GetDefaultCommandContext();
			FD3D12CommandListHandle& hCommandList = defaultContext.CommandListHandle;

			{
				FScopeResourceBarrier ScopeResourceBarrierDest(hCommandList, pResource, pResource->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_DEST, 0);
				// Don't need to transition upload heaps

				defaultContext.numCopies++;
				hCommandList->CopyBufferRegion(
					pResource->GetResource(),
					VertexBuffer->ResourceLocation->GetOffset(),
					UploadHeap->GetResource(),
					pUploadHeapLocation->GetOffset(), LockedData->Pitch);
			}

			DEBUG_RHI_EXECUTE_COMMAND_LIST(this);
		}
	}

	// Remove the FD3D12LockedData from the lock map.
	// If the lock involved a staging resource, this releases it.
	OutstandingLocks.Remove(LockedKey);
}

void FD3D12DynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI, FVertexBufferRHIParamRef DestBufferRHI)
{
	FD3D12VertexBuffer*  SourceBuffer = FD3D12DynamicRHI::ResourceCast(SourceBufferRHI);
	FD3D12VertexBuffer*  DestBuffer = FD3D12DynamicRHI::ResourceCast(DestBufferRHI);

	FD3D12Resource* pSourceResource = SourceBuffer->ResourceLocation->GetResource();
	D3D12_RESOURCE_DESC const& SourceBufferDesc = pSourceResource->GetDesc();

	FD3D12Resource* pDestResource = DestBuffer->ResourceLocation->GetResource();
	D3D12_RESOURCE_DESC const& DestBufferDesc = pDestResource->GetDesc();

	check(SourceBufferDesc.Width == DestBufferDesc.Width);
	check(SourceBuffer->GetSize() == DestBuffer->GetSize());

	GetRHIDevice()->GetDefaultCommandContext().numCopies++;
	GetRHIDevice()->GetDefaultCommandContext().CommandListHandle->CopyResource(pDestResource->GetResource(), pSourceResource->GetResource());
	DEBUG_RHI_EXECUTE_COMMAND_LIST(this);

	GPUProfilingData.RegisterGPUWork(1);
}
