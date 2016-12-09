// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "VulkanRHIPrivate.h"
#include "Containers/ResourceArray.h"

FVulkanStructuredBuffer::FVulkanStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
	: FRHIStructuredBuffer(Stride, Size, Usage)
{
	check((Size % Stride) == 0);

	// copy any resources to the CPU address
	if (ResourceArray)
	{
// 		FMemory::Memcpy( , ResourceArray->GetResourceData(), Size);
		ResourceArray->Discard();
	}
}

FVulkanStructuredBuffer::~FVulkanStructuredBuffer()
{

}




FStructuredBufferRHIRef FVulkanDynamicRHI::RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;//new FVulkanStructuredBuffer(Stride, Size, ResourceArray, InUsage);
}

void* FVulkanDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	return nullptr;
}

void FVulkanDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{

}
