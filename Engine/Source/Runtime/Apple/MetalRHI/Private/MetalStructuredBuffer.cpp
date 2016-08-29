// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

FMetalStructuredBuffer::FMetalStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 Usage)
	: FRHIStructuredBuffer(Stride, Size, Usage)
	, LockOffset(0)
	, LockSize(0)
{
	check((Size % Stride) == 0);

	MTLStorageMode Mode = BUFFER_STORAGE_MODE;
	FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), Size, Mode));
	Buffer = [Buf.Buffer retain];

	if (ResourceArray)
	{
		// copy any resources to the CPU address
		void* LockedMemory = RHILockStructuredBuffer(this, 0, Size, RLM_WriteOnly);
 		FMemory::Memcpy(LockedMemory, ResourceArray->GetResourceData(), Size);
		ResourceArray->Discard();
	}
	INC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buffer.length - GetSize());

	TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
}

FMetalStructuredBuffer::~FMetalStructuredBuffer()
{
	DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buffer.length - GetSize());
	SafeReleasePooledBuffer(Buffer);
	[Buffer release];
}

void* FMetalStructuredBuffer::Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size)
{
	check(LockSize == 0 && LockOffset == 0);
	
	// In order to properly synchronise the buffer access, when a dynamic buffer is locked for writing, discard the old buffer & create a new one. This prevents writing to a buffer while it is being read by the GPU & thus causing corruption. This matches the logic of other RHIs.
	if (GetUsage() & BUF_AnyDynamic && LockMode == RLM_WriteOnly)
	{
		id<MTLBuffer> OldBuffer = Buffer;
		MTLStorageMode Mode = BUFFER_STORAGE_MODE;
		FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), GetSize(), Mode));
		Buffer = [Buf.Buffer retain];
		GetMetalDeviceContext().ReleasePooledBuffer(OldBuffer);
		[OldBuffer release];
	}
	
	if(LockMode != RLM_ReadOnly)
	{
		LockOffset = Offset;
		LockSize = Size;
	}
#if PLATFORM_MAC
	else if(Buffer.storageMode == MTLStorageModeManaged)
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
		
		// Synchronise the buffer with the CPU
		id<MTLBlitCommandEncoder> Blitter = GetMetalDeviceContext().GetBlitContext();
		METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG((&GetMetalDeviceContext()), @"SynchronizeResource(StructuredBuffer %p)", this);
		[Blitter synchronizeResource:Buffer];
		
		//kick the current command buffer.
		GetMetalDeviceContext().SubmitCommandBufferAndWait();
	}
#endif
	
	return ((uint8*)[Buffer contents]);
}

void FMetalStructuredBuffer::Unlock()
{
#if PLATFORM_MAC
	if(LockSize && Buffer.storageMode == MTLStorageModeManaged)
	{
		[Buffer didModifyRange:NSMakeRange(LockOffset, LockSize)];
	}
#endif
	LockOffset = 0;
	LockSize = 0;
}


FStructuredBufferRHIRef FMetalDynamicRHI::RHICreateStructuredBuffer(uint32 Stride,uint32 Size,uint32 InUsage,FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalStructuredBuffer(Stride, Size, CreateInfo.ResourceArray, InUsage);
}

void* FMetalDynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	
	// just return the memory plus the offset
	return (uint8*)StructuredBuffer->Lock(LockMode, Offset, Size);
}

void FMetalDynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	FMetalStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);
	StructuredBuffer->Unlock();
}
