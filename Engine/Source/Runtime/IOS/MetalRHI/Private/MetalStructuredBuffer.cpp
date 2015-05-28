// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"


FMetalStructuredBuffer::FMetalStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
	: FRHIStructuredBuffer(Stride, Size, Usage)
{
	check((Size % Stride) == 0);

	Buffer = [FMetalManager::GetDevice() newBufferWithLength:Size options:BUFFER_CACHE_MODE];

	if (ResourceArray)
	{
		// copy any resources to the CPU address
		void* LockedMemory = RHILockStructuredBuffer(this, 0, Size, RLM_WriteOnly);
 		FMemory::Memcpy(LockedMemory, ResourceArray->GetResourceData(), Size);
		ResourceArray->Discard();
	}

	TRACK_OBJECT(Buffer);
}

FMetalStructuredBuffer::~FMetalStructuredBuffer()
{
	FMetalManager::ReleaseObject(Buffer);
}




FStructuredBufferRHIRef FMetalDynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalStructuredBuffer(Stride, Size, CreateInfo.ResourceArray, InUsage);
}

void* FMetalDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	// just return the memory plus the offset
	return ((uint8*)[StructuredBuffer->Buffer contents]) + Offset;
}

void FMetalDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	// nothing to do
}
