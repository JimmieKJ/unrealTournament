// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanVertexBuffer.cpp: Vulkan vertex buffer RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"


FVulkanVertexBuffer::FVulkanVertexBuffer(FVulkanDevice& InDevice, uint32 InSize, uint32 InUsage) 
	: FRHIVertexBuffer(InSize, InUsage)
	, FVulkanMultiBuffer(InDevice, InSize, (EBufferUsageFlags)InUsage, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
	, Flags(0)
{
}

FVertexBufferRHIRef FVulkanDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FVulkanVertexBuffer* VertexBuffer = new FVulkanVertexBuffer(*Device, Size, InUsage);

	if (CreateInfo.ResourceArray)
	{
		check(VertexBuffer->GetSize() == CreateInfo.ResourceArray->GetResourceDataSize());
		auto* Buffer = VertexBuffer->Lock(RLM_WriteOnly, Size);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		VertexBuffer->Unlock();

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}
	return VertexBuffer;
}

void* FVulkanDynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	return VertexBuffer->Lock(LockMode, Size, Offset);
}

void FVulkanDynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	VertexBuffer->Unlock();
}

void FVulkanDynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI,FVertexBufferRHIParamRef DestBufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}
