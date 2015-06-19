// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalConstantBuffer.cpp: Metal Constant buffer implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"


#define NUM_POOL_BUCKETS 45
#define NUM_SAFE_FRAMES 3

static const uint32 RequestedUniformBufferSizeBuckets[NUM_POOL_BUCKETS] =
{
	16, 32, 48, 64, 80, 96, 112, 128,	// 16-byte increments
	160, 192, 224, 256,			// 32-byte increments
	320, 384, 448, 512,			// 64-byte increments
	640, 768, 896, 1024,			// 128-byte increments
	1280, 1536, 1792, 2048,		// 256-byte increments
	2560, 3072, 3584, 4096,		// 512-byte increments
	5120, 6144, 7168, 8192,		// 1024-byte increments
	10240, 12288, 14336, 16384,	// 2048-byte increments
	20480, 24576, 28672, 32768,	// 4096-byte increments
	40960, 49152, 57344, 65536,	// 8192-byte increments

	// 65536 is current max uniform buffer size for Mac OS X.

	0xFFFFFFFF
};

// Maps desired size buckets to alignment actually 
static TArray<uint32> UniformBufferSizeBuckets;

static uint32 GetUBPoolSize()
{
	return 512 * 1024;
}

// Convert bucket sizes to be compatible with present device
static void RemapBuckets()
{
	const uint32 Alignment = 256;

	for (int32 Count = 0; Count < NUM_POOL_BUCKETS; Count++)
	{
		uint32 AlignedSize = ((RequestedUniformBufferSizeBuckets[Count] + Alignment - 1) / Alignment ) * Alignment;
		if (!UniformBufferSizeBuckets.Contains(AlignedSize))
		{
			UniformBufferSizeBuckets.Push(AlignedSize);
		}
	}
}

static uint32 GetPoolBucketIndex(uint32 NumBytes)
{
	if (UniformBufferSizeBuckets.Num() == 0)
	{
		RemapBuckets();
	}

	unsigned long Lower = 0;
	unsigned long Upper = UniformBufferSizeBuckets.Num();

	do
	{
		unsigned long Middle = (Upper + Lower) >> 1;
		if (NumBytes <= UniformBufferSizeBuckets[Middle - 1])
		{
			Upper = Middle;
		}
		else
		{
			Lower = Middle;
		}
	}
	while (Upper - Lower > 1);

	check(NumBytes <= UniformBufferSizeBuckets[Lower]);
	check((Lower == 0) || (NumBytes > UniformBufferSizeBuckets[Lower - 1]));

	return Lower;
}


// Describes a uniform buffer in the free pool.
struct FPooledUniformBuffer
{
	id<MTLBuffer> Buffer;
	uint32 CreatedSize;
	uint32 FrameFreed;
	uint32 Offset;
};

// Pool of free uniform buffers, indexed by bucket for constant size search time.
TArray<FPooledUniformBuffer> UniformBufferPool[NUM_POOL_BUCKETS];

// Uniform buffers that have been freed more recently than NumSafeFrames ago.
TArray<FPooledUniformBuffer> SafeUniformBufferPools[NUM_SAFE_FRAMES][NUM_POOL_BUCKETS];

// Does per-frame global updating for the uniform buffer pool.
void InitFrame_UniformBufferPoolCleanup()
{
	check(IsInRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_MetalUniformBufferCleanupTime);

	// Index of the bucket that is now old enough to be reused
	const int32 SafeFrameIndex = GFrameNumberRenderThread % NUM_SAFE_FRAMES;

	// Merge the bucket into the free pool array
	for (int32 BucketIndex = 0; BucketIndex < NUM_POOL_BUCKETS; BucketIndex++)
	{
		UniformBufferPool[BucketIndex].Append(SafeUniformBufferPools[SafeFrameIndex][BucketIndex]);
		SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Reset();
	}
}

struct TUBPoolBuffer
{
	id<MTLBuffer> Buffer;
	uint32 ConsumedSpace;
	uint32 AllocatedSpace;
};

TArray<TUBPoolBuffer> UBPool;

void AddNewlyFreedBufferToUniformBufferPool(id<MTLBuffer> Buffer, uint32 Offset, uint32 Size)
{
	check(Buffer);
	FPooledUniformBuffer NewEntry;
	NewEntry.Buffer = Buffer;
	NewEntry.FrameFreed = GFrameNumberRenderThread;
	NewEntry.CreatedSize = Size;
	NewEntry.Offset = Offset;

	// Add to this frame's array of free uniform buffers
	const int32 SafeFrameIndex = (GFrameNumberRenderThread - 1) % NUM_SAFE_FRAMES;
	const uint32 BucketIndex = GetPoolBucketIndex(Size);

	SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Add(NewEntry);
	INC_DWORD_STAT(STAT_MetalNumFreeUniformBuffers);
	INC_MEMORY_STAT_BY(STAT_MetalFreeUniformBufferMemory, Buffer.length);
}


id<MTLBuffer> SuballocateUB(uint32 Size, uint32& OutOffset)
{
	check(Size <= GetUBPoolSize());

	// Find space in previously allocated pool buffers
	for ( int32 Buffer = 0; Buffer < UBPool.Num(); Buffer++)
	{
		TUBPoolBuffer &Pool = UBPool[Buffer];
		if ( Size < (Pool.AllocatedSpace - Pool.ConsumedSpace))
		{
			OutOffset = Pool.ConsumedSpace;
			Pool.ConsumedSpace += Size;
			return Pool.Buffer;
		}
	}

	// No space was found to use, create a new Pool buffer
	uint32 TotalSize = GetUBPoolSize();
	//NSLog(@"New Metal Buffer Size %d", TotalSize);
	id<MTLBuffer> Buffer = [FMetalManager::GetDevice() newBufferWithLength:TotalSize options:BUFFER_CACHE_MODE];
	TRACK_OBJECT(Buffer);

	OutOffset = 0;

	TUBPoolBuffer Pool;
	Pool.Buffer = Buffer;
	Pool.ConsumedSpace = Size;
	Pool.AllocatedSpace = GetUBPoolSize();
	UBPool.Push(Pool);

	return Buffer;
}


FMetalUniformBuffer::FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
	: FRHIUniformBuffer(Layout)
	, Buffer(nil)
	, Offset(0)
	, LastCachedFrame(INDEX_NONE)
{
	if (Layout.ConstantBufferSize > 0)
	{
		// for single use buffers, allocate from the ring buffer to avoid thrashing memory
		if (Usage == UniformBuffer_SingleDraw)
		{
			// use a bit of the ring buffer
			Offset = FMetalManager::Get()->AllocateFromRingBuffer(Layout.ConstantBufferSize);
			Buffer = FMetalManager::Get()->GetRingBuffer();
		}
		else
		{
			// Find the appropriate bucket based on size
			const uint32 BucketIndex = GetPoolBucketIndex(Layout.ConstantBufferSize);
			TArray<FPooledUniformBuffer>& PoolBucket = UniformBufferPool[BucketIndex];
			if (PoolBucket.Num() > 0)
			{
				// Reuse the last entry in this size bucket
				FPooledUniformBuffer FreeBufferEntry = PoolBucket.Pop();
				DEC_DWORD_STAT(STAT_MetalNumFreeUniformBuffers);
				DEC_MEMORY_STAT_BY(STAT_MetalFreeUniformBufferMemory, FreeBufferEntry.CreatedSize);

				// reuse the one
				Buffer = FreeBufferEntry.Buffer;
				Offset = FreeBufferEntry.Offset;
			}
			else
			{
				// Nothing usable was found in the free pool, create a new uniform buffer (full size, not NumBytes)
				uint32 BufferSize = UniformBufferSizeBuckets[BucketIndex];
				Buffer = SuballocateUB(BufferSize, Offset);
			}
		}

		// copy the contents
		FMemory::Memcpy(((uint8*)[Buffer contents]) + Offset, Contents, Layout.ConstantBufferSize);
	}

	// set up an SRT-style uniform buffer
	if (Layout.Resources.Num())
	{
		int32 NumResources = Layout.Resources.Num();
		FRHIResource** InResources = (FRHIResource**)((uint8*)Contents + Layout.ResourceOffset);
		ResourceTable.Empty(NumResources);
		ResourceTable.AddZeroed(NumResources);
		for (int32 i = 0; i < NumResources; ++i)
		{
			check(InResources[i]);
			ResourceTable[i] = InResources[i];
		}
		RawResourceTable.Empty(NumResources);
		RawResourceTable.AddZeroed(NumResources);
	}
}

FMetalUniformBuffer::~FMetalUniformBuffer()
{
	// don't need to free the ring buffer!
	if (Buffer != nil && Buffer != FMetalManager::Get()->GetRingBuffer())
	{
		AddNewlyFreedBufferToUniformBufferPool(Buffer, Offset, GetSize());
	}
}

void FMetalUniformBuffer::CacheResourcesInternal()
{
	const FRHIUniformBufferLayout& Layout = GetLayout();
	int32 NumResources = Layout.Resources.Num();
	const uint8* RESTRICT ResourceTypes = Layout.Resources.GetData();
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = ResourceTable.GetData();
	void** RESTRICT RawResources = RawResourceTable.GetData();
	float CurrentTime = FApp::GetCurrentTime();

	// todo: Immutable resources, i.e. not textures, can be safely cached across frames.
	// Texture streaming makes textures complicated :)
	for (int32 i = 0; i < NumResources; ++i)
	{
		switch (ResourceTypes[i])
		{
			case UBMT_SRV:
				{
					check(0);
					FMetalShaderResourceView* SRV = (FMetalShaderResourceView*)Resources[i].GetReference();
					if (IsValidRef(SRV->SourceTexture))
					{
						FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(SRV->SourceTexture);
						RawResources[i] = Surface;
					}
					else
					{
						check(0);
						RawResources[i] = &SRV->SourceVertexBuffer->Buffer;
					}
				}
				break;

			case UBMT_TEXTURE:
				{
					// todo: this does multiple virtual function calls to find the right type to cast to
					// this is due to multiple inheritance nastiness, NEEDS CLEANUP
					FRHITexture* TextureRHI = (FRHITexture*)Resources[i].GetReference();
					TextureRHI->SetLastRenderTime(CurrentTime);
					FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
					RawResources[i] = Surface;
				}
				break;

			case UBMT_UAV:
				RawResources[i] = 0;
				break;

			case UBMT_SAMPLER:
				RawResources[i] = (FMetalSamplerState*)Resources[i].GetReference();
				break;

			default:
				check(0);
				break;
		}
	}
}



FUniformBufferRHIRef FMetalDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	check(IsInRenderingThread());
	return new FMetalUniformBuffer(Contents, Layout, Usage);
}
