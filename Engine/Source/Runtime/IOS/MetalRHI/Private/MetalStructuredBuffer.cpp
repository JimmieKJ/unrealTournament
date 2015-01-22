// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"


FMetalStructuredBuffer::FMetalStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
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

FMetalStructuredBuffer::~FMetalStructuredBuffer()
{

}




FStructuredBufferRHIRef FMetalDynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalStructuredBuffer(Stride, Size, CreateInfo.ResourceArray, InUsage);
}

void* FMetalDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	DYNAMIC_CAST_METALRESOURCE(StructuredBuffer,StructuredBuffer);

	return NULL;
}

void FMetalDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{

}
