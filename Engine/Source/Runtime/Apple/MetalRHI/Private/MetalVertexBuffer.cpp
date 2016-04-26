// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal vertex buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"


FMetalVertexBuffer::FMetalVertexBuffer(uint32 InSize, uint32 InUsage)
	: FRHIVertexBuffer(InSize, InUsage)
	, LockOffset(0)
	, LockSize(0)
	, ZeroStrideElementSize((InUsage & BUF_ZeroStride) ? InSize : 0)
{
	checkf(InSize <= 256 * 1024 * 1024, TEXT("Metal doesn't support buffers > 256 MB"));
	// Zero-stride buffers must be separate in order to wrap appropriately
	if (InUsage & BUF_Volatile && !(InUsage & BUF_ZeroStride))
	{
		FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), InSize, MTLStorageModeShared));
		Buffer = [Buf.Buffer retain];
	}
	else
	{
		if(!(InUsage & BUF_ZeroStride))
		{
			FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), InSize, BUFFER_STORAGE_MODE));
			Buffer = [Buf.Buffer retain];
		}
		else
		{
			Buffer = [GetMetalDeviceContext().GetDevice() newBufferWithLength:InSize options:BUFFER_CACHE_MODE|BUFFER_MANAGED_MEM];
		}
	}
	TRACK_OBJECT(Buffer);
}

FMetalVertexBuffer::~FMetalVertexBuffer()
{
	if(!(GetUsage() & BUF_ZeroStride))
	{
		SafeReleasePooledBuffer(Buffer);
		[Buffer release];
	}
	else
	{
		SafeReleaseMetalResource(Buffer);
	}
}


void* FMetalVertexBuffer::Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size)
{
	check(LockSize == 0 && LockOffset == 0);
	
	// In order to properly synchronise the buffer access, when a dynamic buffer is locked for writing, discard the old buffer & create a new one. This prevents writing to a buffer while it is being read by the GPU & thus causing corruption. This matches the logic of other RHIs.
	if ((GetUsage() & BUFFER_DYNAMIC_REALLOC) && LockMode == RLM_WriteOnly)
	{
		id<MTLBuffer> OldBuffer = Buffer;
		if(!(GetUsage() & BUF_ZeroStride))
		{
			MTLStorageMode Mode = (GetUsage() & BUF_Volatile) ? MTLStorageModeShared : BUFFER_STORAGE_MODE;
			FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), GetSize(), Mode));
			Buffer = [Buf.Buffer retain];
			GetMetalDeviceContext().ReleasePooledBuffer(OldBuffer);
			[OldBuffer release];
		}
		else
		{
			Buffer = [GetMetalDeviceContext().GetDevice() newBufferWithLength:Buffer.length options:BUFFER_CACHE_MODE|BUFFER_MANAGED_MEM];
			GetMetalDeviceContext().ReleaseObject(OldBuffer);
		}
	}
	
	if(LockMode != RLM_ReadOnly)
	{
		LockSize = Size;
		LockOffset = Offset;
	}
#if PLATFORM_MAC
	else if(Buffer.storageMode == MTLStorageModeManaged)
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
		
		// Synchronise the buffer with the CPU
		id<MTLBlitCommandEncoder> Blitter = GetMetalDeviceContext().GetBlitContext();
		[Blitter synchronizeResource:Buffer];
		
		//kick the current command buffer.
		GetMetalDeviceContext().SubmitCommandBufferAndWait();
	}
#endif
	
	return ((uint8*)[Buffer contents]) + Offset;
}

void FMetalVertexBuffer::Unlock()
{
#if PLATFORM_MAC
	if(LockSize && Buffer.storageMode == MTLStorageModeManaged)
	{
		[Buffer didModifyRange:NSMakeRange(LockOffset, LockSize)];
	}
#endif
	LockSize = 0;
	LockOffset = 0;
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
	return (uint8*)VertexBuffer->Lock(LockMode, Offset, Size);
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
