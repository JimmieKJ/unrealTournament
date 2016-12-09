// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12UniformBuffer.cpp: D3D uniform buffer RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "UniformBuffer.h"

FUniformBufferRHIRef FD3D12DynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12UpdateUniformBufferTime);

	//Note: This is not overly efficient in the mGPU case (we create two+ upload locations) but the CPU savings of having no extra indirection to the resource are worth
	//      it in single node.
	// Create the uniform buffer
	FD3D12UniformBuffer* UniformBufferOut = GetAdapter().CreateLinkedObject<FD3D12UniformBuffer>([&](FD3D12Device* Device) -> FD3D12UniformBuffer*
	{
		// If NumBytesActualData == 0, this uniform buffer contains no constants, only a resource table.
		FD3D12UniformBuffer* NewUniformBuffer = new FD3D12UniformBuffer(Device, Layout);
		check(nullptr != NewUniformBuffer);

		const uint32 NumBytesActualData = Layout.ConstantBufferSize;
		if (NumBytesActualData > 0)
		{
			// Constant buffers must also be 16-byte aligned.
			const uint32 NumBytes = Align(NumBytesActualData, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);	// Allocate a size that is big enough for a multiple of 256
			check(Align(NumBytes, 16) == NumBytes);
			check(Align(Contents, 16) == Contents);
			check(NumBytes <= D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);

			void* MappedData = nullptr;
			if (Usage == EUniformBufferUsage::UniformBuffer_MultiFrame)
			{
				// Uniform buffers that live for multiple frames must use the more expensive and persistent allocation path
				FD3D12DynamicHeapAllocator& Allocator = GetAdapter().GetUploadHeapAllocator();
				MappedData = Allocator.AllocUploadResource(NumBytes, DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT, NewUniformBuffer->ResourceLocation);
			}
			else
			{
				// Uniform buffers which will live for 1 frame at the max can be allocated very efficently from a ring buffer
				FD3D12FastConstantAllocator& Allocator = GetAdapter().GetTransientUniformBufferAllocator();
				MappedData = Allocator.Allocate(NumBytes, NewUniformBuffer->ResourceLocation);
			}
			check(NewUniformBuffer->ResourceLocation.GetOffsetFromBaseOfResource() % 16 == 0);
			check(NewUniformBuffer->ResourceLocation.GetSize() == NumBytes);

			// Copy the data to the upload heap
			check(MappedData != nullptr);
			FMemory::Memcpy(MappedData, Contents, NumBytesActualData);
		}

		// The GPUVA is used to see if this uniform buffer contains constants or is just a resource table.
		check((NumBytesActualData > 0) ? (0 != NewUniformBuffer->ResourceLocation.GetGPUVirtualAddress()) : (0 == NewUniformBuffer->ResourceLocation.GetGPUVirtualAddress()));
		return NewUniformBuffer;
	});

	if (Layout.Resources.Num())
	{
		const int32 NumResources = Layout.Resources.Num();
		FRHIResource** InResources = (FRHIResource**)((uint8*)Contents + Layout.ResourceOffset);

		FD3D12UniformBuffer* CurrentBuffer = UniformBufferOut;

		while (CurrentBuffer != nullptr)
		{
			CurrentBuffer->ResourceTable.Empty(NumResources);
			CurrentBuffer->ResourceTable.AddZeroed(NumResources);
			for (int32 i = 0; i < NumResources; ++i)
			{
				check(InResources[i]);
				CurrentBuffer->ResourceTable[i] = InResources[i];
			}

			CurrentBuffer = CurrentBuffer->GetNextObject();
		}
	}

	return UniformBufferOut;
}

FD3D12UniformBuffer::~FD3D12UniformBuffer()
{
	check(!GRHISupportsRHIThread || IsInRenderingThread());
}

void FD3D12Device::ReleasePooledUniformBuffers()
{
}
