// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalIndexBuffer.cpp: Metal Index buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"


/** Constructor */
FMetalIndexBuffer::FMetalIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
	, IndexType((InStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32)
{
	// volatile buffers can come right from the ring buffer
	if (InUsage & BUF_Volatile)
	{
		Buffer = FMetalManager::Get()->GetRingBuffer();
		// retaining to ease the destructor logic
		[Buffer retain];
	}
	else
	{
		Buffer = [FMetalManager::GetDevice() newBufferWithLength:InSize options:BUFFER_CACHE_MODE];
		Offset = 0;
	}
	TRACK_OBJECT(Buffer);
}

FMetalIndexBuffer::~FMetalIndexBuffer()
{
	FMetalManager::ReleaseObject(Buffer);
}

void* FMetalIndexBuffer::Lock(EResourceLockMode LockMode, uint32 Size)
{
	if (GetUsage() & BUF_Volatile)
	{
		Offset = FMetalManager::Get()->AllocateFromRingBuffer(Size);
	}
	return ((uint8*)[Buffer contents]) + Offset;
}

void FMetalIndexBuffer::Unlock()
{

}

FIndexBufferRHIRef FMetalDynamicRHI::RHICreateIndexBuffer(uint32 Stride,uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FMetalIndexBuffer* IndexBuffer = new FMetalIndexBuffer(Stride, Size, InUsage);
	
	if (CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return IndexBuffer;
}

void* FMetalDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	return (uint8*)IndexBuffer->Lock(LockMode, Size) + Offset;
}

void FMetalDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	IndexBuffer->Unlock();
}
