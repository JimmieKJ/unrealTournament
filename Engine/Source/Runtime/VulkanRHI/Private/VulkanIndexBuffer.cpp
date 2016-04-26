// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanIndexBuffer.cpp: Vulkan Index buffer RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"

/** Constructor */
FVulkanIndexBuffer::FVulkanIndexBuffer(FVulkanDevice& InDevice, uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
	, FVulkanMultiBuffer(InDevice, InSize, (EBufferUsageFlags)InUsage, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
	, IndexType(InStride == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16)
{
}

FIndexBufferRHIRef FVulkanDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FVulkanIndexBuffer* IndexBuffer = new FVulkanIndexBuffer(*Device, Stride, Size, InUsage);
	if (CreateInfo.ResourceArray)
	{
		check(IndexBuffer->GetSize() == CreateInfo.ResourceArray->GetResourceDataSize());
		auto* Buffer = IndexBuffer->Lock(RLM_WriteOnly, Size);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		IndexBuffer->Unlock();

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return IndexBuffer;
}

void* FVulkanDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	return IndexBuffer->Lock(LockMode, Size, Offset);
}

void FVulkanDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	IndexBuffer->Unlock();
}


FVulkanDynamicPooledBuffer::FPoolElement::FPoolElement() :
	NextFreeOffset(0),
	LastLockSize(0),
	Size(0)
{
}

inline FVulkanDynamicLockInfo FVulkanDynamicPooledBuffer::FPoolElement::Lock(VkDeviceSize InSize)
{
	check(LastLockSize == 0);
	int32 BufferIndex = GFrameNumberRenderThread % NUM_FRAMES;

	auto* Buffer = Buffers[BufferIndex];

	FVulkanDynamicLockInfo Info;
	Info.Buffer = Buffer;
	Info.Data = Buffer->Lock(InSize, NextFreeOffset);
	Info.Offset = NextFreeOffset;

	NextFreeOffset += InSize;
	LastLockSize = InSize;

	return Info;
}

void FVulkanDynamicPooledBuffer::FPoolElement::Unlock(FVulkanDynamicLockInfo LockInfo)
{
	check(LastLockSize != 0);

	int32 BufferIndex = GFrameNumberRenderThread % NUM_FRAMES;
	Buffers[BufferIndex]->Unlock();

	LastLockSize = 0;
}

FVulkanDynamicPooledBuffer::FVulkanDynamicPooledBuffer(FVulkanDevice& Device, uint32 NumPoolElements, const VkDeviceSize* PoolElements, VkBufferUsageFlagBits UsageFlags)
{
	for (uint32 PoolIndex = 0; PoolIndex < NumPoolElements; ++PoolIndex)
	{
		auto* Element = new(Elements) FPoolElement;
		VkDeviceSize Size = PoolElements[PoolIndex];
		Element->Size = Size;
		for (int32 Index = 0; Index < NUM_FRAMES; ++Index)
		{
			Element->Buffers[Index] = new FVulkanBuffer(Device, Size, UsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false, __FILE__, __LINE__);
		}
	}
}

FVulkanDynamicPooledBuffer::~FVulkanDynamicPooledBuffer()
{
	for (auto& Element : Elements)
	{
		for (int32 Index = 0; Index < NUM_FRAMES; ++Index)
		{
			delete Element.Buffers[Index];
		}
	}

	Elements.Reset(0);
}

FVulkanDynamicLockInfo FVulkanDynamicPooledBuffer::Lock(VkDeviceSize InSize)
{
	check(InSize > 0);

	FPoolElement* Element = nullptr;
	int32 PoolIndex = 0;
	for ( ; PoolIndex < Elements.Num(); ++PoolIndex)
	{
		if (Elements[PoolIndex].CanLock(InSize))
		{
			Element = &Elements[PoolIndex];
			break;
		}
	}

	checkf(Element, TEXT("Out of memory allocating %d bytes from DynamicIndex buffer!"), InSize);
	FVulkanDynamicLockInfo Info = Element->Lock(InSize);
	Info.PoolElementIndex = PoolIndex;
	return Info;
}

void FVulkanDynamicPooledBuffer::EndFrame()
{
	for (int32 PoolIndex = 0; PoolIndex < Elements.Num(); ++PoolIndex)
	{
		Elements[PoolIndex].NextFreeOffset = 0;
		Elements[PoolIndex].LastLockSize = 0;
	}
}

static const VkDeviceSize GDynamicIndexBufferPoolSizes[] =
{
	1 * 1024,
	16 * 1024,
	64 * 1024,
	1024 * 1024,
};

FVulkanDynamicIndexBuffer::FVulkanDynamicIndexBuffer(FVulkanDevice& Device) :
	FVulkanDynamicPooledBuffer(Device, ARRAY_COUNT(GDynamicIndexBufferPoolSizes), GDynamicIndexBufferPoolSizes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
{
}

static const VkDeviceSize GDynamicVertexBufferPoolSizes[] =
{
	4 * 1024,
	64 * 1024,
	256 * 1024,
	1024 * 1024,
};

FVulkanDynamicVertexBuffer::FVulkanDynamicVertexBuffer(FVulkanDevice& Device) :
	FVulkanDynamicPooledBuffer(Device, ARRAY_COUNT(GDynamicVertexBufferPoolSizes), GDynamicVertexBufferPoolSizes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
{
}
