// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalBufferPools.h"
#include "MetalProfiler.h"

FMetalPooledBuffer::FMetalPooledBuffer()
: Buffer(nil)
{
	INC_DWORD_STAT(STAT_MetalFreePooledBufferCount);
}

FMetalPooledBuffer::FMetalPooledBuffer(FMetalPooledBuffer const& Other)
: Buffer(nil)
{
	INC_DWORD_STAT(STAT_MetalFreePooledBufferCount);
	operator=(Other);
}

FMetalPooledBuffer::FMetalPooledBuffer(id<MTLBuffer> Buf)
: Buffer(Buf)
{
	INC_DWORD_STAT(STAT_MetalFreePooledBufferCount);
	[Buffer retain];
}

FMetalPooledBuffer::~FMetalPooledBuffer()
{
	[Buffer release];
	Buffer = nil;
	DEC_DWORD_STAT(STAT_MetalFreePooledBufferCount);
}

FMetalPooledBuffer& FMetalPooledBuffer::operator=(FMetalPooledBuffer const& Other)
{
	if(this != &Other)
	{
		Buffer = [Other.Buffer retain];
	}
	return *this;
}

/** Get the pool bucket index from the size
 * @param Size the number of bytes for the resource
 * @returns The bucket index.
 */
uint32 FMetalBufferPoolPolicyData::GetPoolBucketIndex(CreationArguments Args)
{
	// Only BUFFER_CACHE_MODE cached memory is acceptable
	// Auto & Private memory is also forbidden in this pool
#if PLATFORM_MAC
	check(Args.Storage == MTLStorageModeShared || Args.Storage == MTLStorageModeManaged || Args.Storage == MTLStorageModePrivate);
#endif
	
	uint32 Size = Args.Size;
	
	unsigned long Lower = 0;
	unsigned long Upper = NumPoolBucketSizes;
	unsigned long Middle;
	
	do
	{
		Middle = ( Upper + Lower ) >> 1;
		if( Size <= BucketSizes[Middle-1] )
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while( Upper - Lower > 1 );
	
	check( Size <= BucketSizes[Lower] );
	check( (Lower == 0 ) || ( Size > BucketSizes[Lower-1] ) );
	
	// Managed buffers are placed into a different buffer than shared.
	Lower += (NumPoolBucketSizes * Args.Storage);
	
	return Lower;
}

/** Get the pool bucket size from the index
 * @param Bucket the bucket index
 * @returns The bucket size.
 */
uint32 FMetalBufferPoolPolicyData::GetPoolBucketSize(uint32 Bucket)
{
	check(Bucket < NumPoolBuckets);
	uint32 Index = Bucket;
	if(Index >= NumPoolBucketSizes)
	{
		Index -= NumPoolBucketSizes;
	}
	checkf(Index < NumPoolBucketSizes, TEXT("%d %d"), Index, NumPoolBucketSizes);
	return BucketSizes[Index];
}

/** Creates the resource
 * @param Args The buffer size in bytes.
 * @returns A suitably sized buffer or NULL on failure.
 */
FMetalPooledBuffer FMetalBufferPoolPolicyData::CreateResource(CreationArguments Args)
{
	// This manual construction is deliberate as newBuffer creates a resource with ref-cout == 1, so no need to retain
	check(Args.Device);
	FMetalPooledBuffer NewBuf;
	uint32 BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
	NewBuf.Buffer = [Args.Device newBufferWithLength: BufferSize options: BUFFER_CACHE_MODE
#if METAL_API_1_1
					 | (Args.Storage << MTLResourceStorageModeShift)
#endif
					 ];
	TRACK_OBJECT(STAT_MetalBufferCount, NewBuf.Buffer);
	INC_DWORD_STAT(STAT_MetalPooledBufferCount);
	INC_MEMORY_STAT_BY(STAT_MetalPooledBufferMem, BufferSize);
	INC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, BufferSize);
	INC_DWORD_STAT(STAT_MetalBufferNativeAlloctations);
	INC_DWORD_STAT_BY(STAT_MetalBufferNativeMemAlloc, BufferSize);
	return NewBuf;
}

/** Gets the arguments used to create resource
 * @param Resource The buffer to get data for.
 * @returns The arguments used to create the buffer.
 */
FMetalBufferPoolPolicyData::CreationArguments FMetalBufferPoolPolicyData::GetCreationArguments(FMetalPooledBuffer Resource)
{
#if PLATFORM_MAC
	return FMetalBufferPoolPolicyData::CreationArguments(Resource.Buffer.device, Resource.Buffer.length, Resource.Buffer.storageMode);
#else
	return FMetalBufferPoolPolicyData::CreationArguments(Resource.Buffer.device, Resource.Buffer.length, MTLStorageModeShared);
#endif
}

void FMetalBufferPoolPolicyData::FreeResource(FMetalPooledBuffer Resource)
{
	UNTRACK_OBJECT(STAT_MetalBufferCount, Resource.Buffer);
	DEC_MEMORY_STAT_BY(STAT_MetalPooledBufferMem, Resource.Buffer.length);
	DEC_DWORD_STAT(STAT_MetalPooledBufferCount);
	DEC_MEMORY_STAT_BY(STAT_MetalFreePooledBufferMem, Resource.Buffer.length);
	INC_DWORD_STAT(STAT_MetalBufferNativeFreed);
	INC_DWORD_STAT_BY(STAT_MetalBufferNativeMemFreed, Resource.Buffer.length);
}

/** The bucket sizes */
uint32 FMetalBufferPoolPolicyData::BucketSizes[NumPoolBucketSizes] = {
	256, 512, 768, 1024, 2048, 4096, 8192, 16384,
	32768, 65536, 131072, 262144, 524288, 1048576, 2097152,
	4194304, 8388608, 16777216, 33554432, 67108864, 134217728, 268435456
};

/** Destructor */
FMetalBufferPool::~FMetalBufferPool()
{
}

void FMetalQueryBufferPool::Allocate(FMetalQueryResult& NewQuery)
{
    FMetalQueryBuffer* QB = IsValidRef(CurrentBuffer) ? CurrentBuffer.GetReference() : GetCurrentQueryBuffer();
    if(Align(QB->WriteOffset, EQueryBufferAlignment) + EQueryResultMaxSize <= EQueryBufferMaxSize)
    {
		if(!QB->CommandBuffer)
		{
			QB->CommandBuffer = Context->GetCurrentCommandBuffer();
			[QB->CommandBuffer retain];
		}
		NewQuery.SourceBuffer = QB;
        NewQuery.Offset = Align(QB->WriteOffset, EQueryBufferAlignment);
        FMemory::Memzero((((uint8*)[QB->Buffer contents]) + NewQuery.Offset), EQueryResultMaxSize);
        QB->WriteOffset = Align(QB->WriteOffset, EQueryBufferAlignment) + EQueryResultMaxSize;
	}
	else
	{
		UE_LOG(LogRHI, Warning, TEXT("Performance: Resetting render command encoder as query buffer offset: %d exceeds the maximum allowed: %d."), QB->WriteOffset, EQueryBufferMaxSize);
		Context->ResetRenderCommandEncoder();
		Allocate(NewQuery);
	}
}

FMetalQueryBuffer* FMetalQueryBufferPool::GetCurrentQueryBuffer()
{
    if(!IsValidRef(CurrentBuffer) || CurrentBuffer->WriteOffset > 0)
    {
        id<MTLBuffer> Buffer;
        if(Buffers.Num())
        {
            Buffer = Buffers.Pop();
        }
        else
        {
            Buffer = [Context->GetDevice() newBufferWithLength:EQueryBufferMaxSize options: BUFFER_CACHE_MODE
#if METAL_API_1_1
					  | MTLResourceStorageModeShared
#endif
					  ];
			TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
        }
        CurrentBuffer = new FMetalQueryBuffer(Context, Buffer);
    }

	return CurrentBuffer.GetReference();
}

void FMetalQueryBufferPool::ReleaseQueryBuffer(id<MTLBuffer> Buffer)
{
    Buffers.Add(Buffer);
}

FRingBuffer::FRingBuffer(id<MTLDevice> Device, uint32 Size, uint32 InDefaultAlignment)
{
	DefaultAlignment = InDefaultAlignment;
	Buffer = [Device newBufferWithLength:Size options:BUFFER_CACHE_MODE];
	Offset = 0;
	LastRead = Size;
	TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
}

uint32 FRingBuffer::Allocate(uint32 Size, uint32 Alignment)
{
	if (Alignment == 0)
	{
		Alignment = DefaultAlignment;
	}
	
	if(LastRead <= Offset)
	{
		// align the offset
		Offset = Align(Offset, Alignment);
		
		// wrap if needed
		if (Offset + Size <= Buffer.length)
		{
			// get current location
			uint32 ReturnOffset = Offset;
			Offset += Size;
			return ReturnOffset;
		}
		else // wrap
		{
			Offset = 0;
		}
	}
	
	// align the offset
	Offset = Align(Offset, Alignment);
	if(Offset + Size < LastRead)
	{
		// get current location
		uint32 ReturnOffset = Offset;
		Offset += Size;
		return ReturnOffset;
	}
	else
	{
		uint32 BufferSize = Buffer.length;
		UE_LOG(LogMetal, Warning, TEXT("Reallocating ring-buffer from %d to %d to avoid wrapping write at offset %d into outstanding buffer region %d at frame %lld]"), BufferSize, BufferSize * 2, Offset, LastRead, GFrameCounter);
		SafeReleaseMetalResource(Buffer);
		Buffer = [GetMetalDeviceContext().GetDevice() newBufferWithLength:BufferSize * 2 options:BUFFER_CACHE_MODE];
		TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
		Offset = 0;
		LastRead = Size;

		// get current location
		uint32 ReturnOffset = Offset;
		Offset += Size;
		return ReturnOffset;
	}
}
