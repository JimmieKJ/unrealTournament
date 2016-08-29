// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#include "ResourcePool.h"

#if !__IPHONE_9_0 && !__MAC_10_11
typedef NS_ENUM(NSUInteger, MTLStorageMode)
{
	MTLStorageModeShared = 0
};
#endif

struct FMetalPooledBuffer
{
	FMetalPooledBuffer();
	FMetalPooledBuffer(FMetalPooledBuffer const& Other);
	FMetalPooledBuffer(id<MTLBuffer> Buf);
	~FMetalPooledBuffer();
	
	FMetalPooledBuffer& operator=(FMetalPooledBuffer const& Other);
	
	id<MTLBuffer> Buffer;
};

struct FMetalPooledBufferArgs
{
	FMetalPooledBufferArgs() : Device(nil), Size(0), Storage(MTLStorageModeShared) {}
	
	FMetalPooledBufferArgs(id<MTLDevice> InDevice, uint32 InSize, MTLStorageMode InStorage)
	: Device(InDevice)
	, Size(InSize)
	, Storage(InStorage)
	{
	}
	
	id<MTLDevice> Device;
	uint32 Size;
	MTLStorageMode Storage;
};

class FMetalBufferPoolPolicyData
{
public:
	/** Buffers are created with a simple byte size */
	typedef FMetalPooledBufferArgs CreationArguments;
	enum
	{
		NumSafeFrames = 1, /** Number of frames to leave buffers before reclaiming/reusing */
		NumPoolBucketSizes = 22, /** Number of pool bucket sizes */
		NumPoolBuckets = NumPoolBucketSizes * 3, /** Number of pool bucket sizes * 2 for Shared/Managed/Private storage */
		NumToDrainPerFrame = 65536, /** Max. number of resources to cull in a single frame */
		CullAfterFramesNum = 30 /** Resources are culled if unused for more frames than this */
	};
	
	/** Get the pool bucket index from the size
	 * @param Size the number of bytes for the resource
	 * @returns The bucket index.
	 */
	uint32 GetPoolBucketIndex(CreationArguments Args);
	
	/** Get the pool bucket size from the index
	 * @param Bucket the bucket index
	 * @returns The bucket size.
	 */
	uint32 GetPoolBucketSize(uint32 Bucket);
	
	/** Creates the resource
	 * @param Args The buffer size in bytes.
	 * @returns A suitably sized buffer or NULL on failure.
	 */
	FMetalPooledBuffer CreateResource(CreationArguments Args);
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	CreationArguments GetCreationArguments(FMetalPooledBuffer Resource);
	
	/** Frees the resource
	 * @param Resource The buffer to prepare for release from the pool permanently.
	 */
	void FreeResource(FMetalPooledBuffer Resource);
	
private:
	/** The bucket sizes */
	static uint32 BucketSizes[NumPoolBucketSizes];
};

/** A pool for metal buffers with consistent usage, bucketed for efficiency. */
class FMetalBufferPool : public TResourcePool<FMetalPooledBuffer, FMetalBufferPoolPolicyData, FMetalBufferPoolPolicyData::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FMetalBufferPool();
};

struct FMetalQueryBufferPool
{
    enum
    {
        EQueryBufferAlignment = 8,
        EQueryResultMaxSize = 8,
        EQueryBufferMaxSize = 64 * 1024
    };
    
    FMetalQueryBufferPool(class FMetalContext* InContext)
    : Context(InContext)
    {
    }
    
    void Allocate(FMetalQueryResult& NewQuery);
    FMetalQueryBuffer* GetCurrentQueryBuffer();
    void ReleaseQueryBuffer(id<MTLBuffer> Buffer);
    
    FMetalQueryBufferRef CurrentBuffer;
    TArray<id<MTLBuffer>> Buffers;
	class FMetalContext* Context;
};

struct FRingBuffer
{
	FRingBuffer(id<MTLDevice> Device, uint32 Size, uint32 InDefaultAlignment);

	uint32 GetOffset() { return Offset; }
	void SetLastRead(uint32 Read) { LastRead = Read; }
	uint32 Allocate(uint32 Size, uint32 Alignment);

	uint32 DefaultAlignment;
	id<MTLBuffer> Buffer;
	uint32 Offset;
	uint32 LastRead;
};
