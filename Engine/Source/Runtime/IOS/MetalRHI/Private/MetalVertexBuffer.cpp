// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal vertex buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"


FMetalVertexBuffer::FMetalVertexBuffer(uint32 InSize, uint32 InUsage)
	: FRHIVertexBuffer(InSize, InUsage)
	, ZeroStrideElementSize((InUsage & BUF_ZeroStride) ? InSize : 0)
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

FMetalVertexBuffer::~FMetalVertexBuffer()
{
	FMetalManager::ReleaseObject(Buffer);
}


void* FMetalVertexBuffer::Lock(EResourceLockMode LockMode, uint32 Size)
{
	if (GetUsage() & BUF_Volatile)
	{
		Offset = FMetalManager::Get()->AllocateFromRingBuffer(Size);
	}
	return ((uint8*)[Buffer contents]) + Offset;
}

void FMetalVertexBuffer::Unlock()
{
	
}

FVertexBufferRHIRef FMetalDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// make the RHI object, which will allocate memory
	FMetalVertexBuffer* VertexBuffer = new FMetalVertexBuffer(Size, InUsage);

	if (CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockVertexBuffer(VertexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockVertexBuffer(VertexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return VertexBuffer;
}

void* FMetalDynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	// default to vertex buffer memory
	return (uint8*)VertexBuffer->Lock(LockMode, Size) + Offset;
}

void FMetalDynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	VertexBuffer->Unlock();
}

void FMetalDynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI,FVertexBufferRHIParamRef DestBufferRHI)
{
	NOT_SUPPORTED("RHICopyVertexBuffer");
}