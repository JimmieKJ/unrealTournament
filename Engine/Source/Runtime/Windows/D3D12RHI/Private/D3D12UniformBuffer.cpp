// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12UniformBuffer.cpp: D3D uniform buffer RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "UniformBuffer.h"

namespace D3D12RHI
{
	/** Describes a uniform buffer in the free pool. */
	struct FPooledUniformBuffer
	{
		TRefCountPtr<FD3D12Resource> Buffer;
		uint32 CreatedSize;
		uint32 FrameFreed;
	};

	/**
	 * Number of size buckets to use for the uniform buffer free pool.
	 * This needs to be enough to cover the valid uniform buffer size range combined with the heuristic used to map sizes to buckets.
	 */
	const int32 NumPoolBuckets = 17;

	/**
	 * Number of frames that a uniform buffer will not be re-used for, after being freed.
	 * This is done as a workaround for what appears to be an AMD driver bug with 11.10 drivers and a 6970 HD,
	 * Where reusing a constant buffer with D3D11_MAP_WRITE_DISCARD still in use by the GPU will result in incorrect contents randomly.
	 */
	const int32 NumSafeFrames = 3;

	/** Returns the size in bytes of the bucket that the given size fits into. */
	uint32 GetPoolBucketSize(uint32 NumBytes)
	{
		return FMath::RoundUpToPowerOfTwo(NumBytes);
	}

	/** Returns the index of the bucket that the given size fits into. */
	uint32 GetPoolBucketIndex(uint32 NumBytes)
	{
		return FMath::CeilLogTwo(NumBytes);
	}

	/** Does per-frame global updating for the uniform buffer pool. */
	void UniformBufferBeginFrame()
	{
	}
}

// 0 is reserved to mean "not a uniform buffer"
LONGLONG D3D12UniformBufferSequenceNumber = 1;

FUniformBufferRHIRef FD3D12DynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	uint64 SequenceNumber = InterlockedIncrement64(&D3D12UniformBufferSequenceNumber);

	FD3D12UniformBuffer* NewUniformBuffer = nullptr;
	const uint32 NumBytesActualData = Layout.ConstantBufferSize;
	const uint32 NumBytes = Align(NumBytesActualData, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);	// Allocate a size that is big enough for a multiple of 256
	if (NumBytes > 0)
	{
		// Constant buffers must also be 16-byte aligned.
		check(Align(NumBytes, 16) == NumBytes);
		check(Align(Contents, 16) == Contents);
		check(NumBytes <= D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);
		check(NumBytes < (1 << NumPoolBuckets));

		SCOPE_CYCLE_COUNTER(STAT_D3D12UpdateUniformBufferTime);

		// Use an upload heap
		TRefCountPtr<FD3D12ResourceLocation> ResourceLocation = new FD3D12ResourceLocation(GetRHIDevice());
		void* pData = GetRHIDevice()->GetDefaultUploadHeapAllocator().AllocUploadResource(NumBytes, DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT, ResourceLocation);

		check(pData != nullptr);

		// Copy the data to the upload heap
		FMemory::Memcpy(pData, Contents, NumBytesActualData);

		check(ResourceLocation->GetOffset() % 16 == 0);
		check(ResourceLocation->GetEffectiveBufferSize() == NumBytes);
		NewUniformBuffer = new FD3D12UniformBuffer(GetRHIDevice(), Layout, ResourceLocation, FRingAllocation(), SequenceNumber);

		// Create an offline CBV descriptor
		NewUniformBuffer->OfflineDescriptorHandle =
			GetRHIDevice()->GetViewDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>().AllocateHeapSlot(NewUniformBuffer->OfflineHeapIndex);

		D3D12_CONSTANT_BUFFER_VIEW_DESC CBView;
		CBView.BufferLocation = ResourceLocation->GetGPUVirtualAddress();
		CBView.SizeInBytes = NumBytes;
		check(ResourceLocation->GetOffset() % 256 == 0);
		check(CBView.SizeInBytes <= 4096 * 16);
		check(CBView.SizeInBytes % 256 == 0);

		GetRHIDevice()->GetDevice()->CreateConstantBufferView(&CBView, NewUniformBuffer->OfflineDescriptorHandle);
	}
	else
	{
		// This uniform buffer contains no constants, only a resource table.
		NewUniformBuffer = new FD3D12UniformBuffer(GetRHIDevice(), Layout, nullptr, FRingAllocation(), SequenceNumber);

		check(0 == NewUniformBuffer->OfflineDescriptorHandle.ptr);
	}

	if (Layout.Resources.Num())
	{
		int32 NumResources = Layout.Resources.Num();
		FRHIResource** InResources = (FRHIResource**)((uint8*)Contents + Layout.ResourceOffset);
		NewUniformBuffer->ResourceTable.Empty(NumResources);
		NewUniformBuffer->ResourceTable.AddZeroed(NumResources);
		for (int32 i = 0; i < NumResources; ++i)
		{
			check(InResources[i]);
			NewUniformBuffer->ResourceTable[i] = InResources[i];
		}
	}

	return NewUniformBuffer;
}

FD3D12UniformBuffer::~FD3D12UniformBuffer()
{
	check(!GRHISupportsRHIThread || IsInRenderingThread());

	if (OfflineDescriptorHandle.ptr != 0)
	{
		FDescriptorHeapManager& HeapManager = GetParentDevice()->GetViewDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>();

		HeapManager.FreeHeapSlot(OfflineDescriptorHandle, OfflineHeapIndex);
	}
}

void FD3D12Device::ReleasePooledUniformBuffers()
{
}
