// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalViewport.h"
#include "MetalBufferPools.h"
#include "MetalCommandEncoder.h"
#include "MetalCommandQueue.h"
#include "MetalCommandList.h"
#if PLATFORM_IOS
#include "IOSView.h"
#endif
#include "LockFreeList.h"

#define NUM_SAFE_FRAMES 4

/**
 * Enumeration of features which are present only on some OS/device combinations.
 * These have to be checked at runtime as well as compile time to ensure backward compatibility.
 */
enum EMetalFeatures
{
	/** Support for separate front & back stencil ref. values */
	EMetalFeaturesSeparateStencil = 1 << 0,
	/** Support for specifying an update to the buffer offset only */
	EMetalFeaturesSetBufferOffset = 1 << 1,
	/** Support for specifying the depth clip mode */
	EMetalFeaturesDepthClipMode = 1 << 2,
	/** Support for specifying resource usage & memory options */
	EMetalFeaturesResourceOptions = 1 << 3,
	/** Supports texture->buffer blit options for depth/stencil blitting */
	EMetalFeaturesDepthStencilBlitOptions = 1 << 4
};

class FMetalRHICommandContext;

class FMetalContext
{
	friend class FMetalCommandContextContainer;
public:
	FMetalContext(FMetalCommandQueue& Queue, bool const bIsImmediate);
	virtual ~FMetalContext();
	
	static FMetalContext* GetCurrentContext();
	
	id<MTLDevice> GetDevice();
	FMetalCommandQueue& GetCommandQueue();
	FMetalCommandList& GetCommandList();
	FMetalCommandEncoder& GetCommandEncoder();
	id<MTLRenderCommandEncoder> GetRenderContext();
	id<MTLBlitCommandEncoder> GetBlitContext();
	id<MTLCommandBuffer> GetCurrentCommandBuffer();
	FMetalStateCache& GetCurrentState() { return StateCache; }
	
	/** Return an auto-released command buffer, caller will need to retain it if it needs to live awhile */
	id<MTLCommandBuffer> CreateCommandBuffer(bool bRetainReferences)
	{
		return bRetainReferences ? CommandQueue.CreateRetainedCommandBuffer() : CommandQueue.CreateUnretainedCommandBuffer();
	}
	
	/**
	 * Handle rendering thread starting/stopping
	 */
	void CreateAutoreleasePool();
	void DrainAutoreleasePool();

	/**
	 * Do anything necessary to prepare for any kind of draw call 
	 * @param PrimitiveType The UE4 primitive type for the draw call, needed to compile the correct render pipeline.
	 */
	void PrepareToDraw(uint32 PrimitiveType);
	
	/**
	 * Set the color, depth and stencil render targets, and then make the new command buffer/encoder
	 */
	void SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo);
	
	/**
	 * Allocate from a dynamic ring buffer - by default align to the allowed alignment for offset field when setting buffers
	 */
	uint32 AllocateFromRingBuffer(uint32 Size, uint32 Alignment=0);
	id<MTLBuffer> GetRingBuffer()
	{
		return RingBuffer->Buffer;
	}

	TSharedRef<FMetalQueryBufferPool, ESPMode::ThreadSafe> GetQueryBufferPool()
	{
		return QueryBuffer.ToSharedRef();
	}

    void SubmitCommandsHint(bool const bCreateNew = true);
	void SubmitCommandBufferAndWait();
	void SubmitComputeCommandBufferAndWait();
	void ResetRenderCommandEncoder();
	
	void SetShaderResourceView(EShaderFrequency ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV);

	void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);
#if PLATFORM_MAC
	void DispatchIndirect(FMetalVertexBuffer* ArgumentBuffer, uint32 ArgumentOffset);
#endif

	void StartTiming(class FMetalEventNode* EventNode);
	void EndTiming(class FMetalEventNode* EventNode);

protected:
	void InitFrame(bool const bImmediateContext);
	void FinishFrame();

	/** Create & set the current command buffer, waiting on outstanding command buffers if required. */
	void CreateCurrentCommandBuffer(bool bWait);

	/**
	 * Possibly switch from compute to graphics
	 */
	void ConditionalSwitchToGraphics();
	
	/**
	 * Switch to blitting
	 */
	void ConditionalSwitchToBlit();
	
	/** Apply the SRT before drawing */
	void CommitGraphicsResourceTables();
	
	void CommitNonComputeShaderConstants();
	
private:
	FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI, float CurrentTime);
	
	FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalShaderResourceView* RESTRICT SRV, float CurrentTime);
	
	FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState, float CurrentTime);
	
	template <typename MetalResourceType>
	inline int32 SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex);
	
	template <class ShaderType>
	void SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage);
	
protected:
	/** The underlying Metal device */
	id<MTLDevice> Device;
	
	/** The wrapper around the device command-queue for creating & committing command buffers to */
	FMetalCommandQueue& CommandQueue;
	
	/** The wrapper around commabd buffers for ensuring correct parallel execution order */
	FMetalCommandList CommandList;
	
	/** The wrapper for encoding commands into the current command buffer. */
	FMetalCommandEncoder CommandEncoder;
	
	/** The cache of all tracked & accessible state. */
	FMetalStateCache StateCache;
	
	/** The current command buffer that receives new commands. */
	id<MTLCommandBuffer> CurrentCommandBuffer;
	
	/** A sempahore used to ensure that wait for previous frames to complete if more are in flight than we permit */
	dispatch_semaphore_t CommandBufferSemaphore;
	
	/** A simple fixed-size ring buffer for dynamic data */
	TSharedPtr<FRingBuffer, ESPMode::ThreadSafe> RingBuffer;
	
	/** A pool of buffers for writing visibility query results. */
	TSharedPtr<FMetalQueryBufferPool, ESPMode::ThreadSafe> QueryBuffer;
	
	/** the slot to store a per-thread autorelease pool */
	static uint32 AutoReleasePoolTLSSlot;
	
	/** the slot to store a per-thread context ref */
	static uint32 CurrentContextTLSSlot;
	
	/**
	 * Internal counter used for resource table caching.
	 * INDEX_NONE means caching is not allowed.
	 */
	uint32 ResourceTableFrameCounter;
};


class FMetalDeviceContext : public FMetalContext
{
public:
	static FMetalDeviceContext* CreateDeviceContext();
	virtual ~FMetalDeviceContext();
	
	bool SupportsFeature(EMetalFeatures InFeature) { return ((Features & InFeature) != 0); }
	
	FMetalPooledBuffer CreatePooledBuffer(FMetalPooledBufferArgs const& Args);
	void ReleasePooledBuffer(FMetalPooledBuffer Buf);
	void ReleaseObject(id Object);
	
	void BeginFrame();
	void EndFrame();
	
	/** RHIBeginScene helper */
	void BeginScene();
	/** RHIEndScene helper */
	void EndScene();
	
	void BeginDrawingViewport(FMetalViewport* Viewport);
	void EndDrawingViewport(FMetalViewport* Viewport, bool bPresent);
	
	/** Take a parallel FMetalContext from the free-list or allocate a new one if required */
	FMetalRHICommandContext* AcquireContext();
	
	/** Release a parallel FMetalContext back into the free-list */
	void ReleaseContext(FMetalRHICommandContext* Context);
	
	/** Returns the number of concurrent contexts encoding commands, including the device context. */
	uint32 GetNumActiveContexts(void) const;
	
	/** Get the index of the bound Metal device in the global list of rendering devices. */
	uint32 GetDeviceIndex(void) const;
	
private:
	FMetalDeviceContext(id<MTLDevice> MetalDevice, uint32 DeviceIndex, FMetalCommandQueue* Queue);
	
private:
	/** The chosen Metal device */
	id<MTLDevice> Device;
	
	/** The index into the GPU device list for the selected Metal device */
	uint32 DeviceIndex;
	
	/** Mutex for access to the unsafe buffer pool */
	FCriticalSection PoolMutex;
	
	/** Dynamic buffer pool */
	FMetalBufferPool BufferPool;
	
	/** Free lists for releasing objects only once it is safe to do so */
	TSet<id> FreeList;
	struct FMetalDelayedFreeList
	{
		FEvent* Signal;
		TSet<id> FreeList;
	};
	TArray<FMetalDelayedFreeList*> DelayedFreeLists;
	
	/** Free-list of contexts for parallel encoding */
	TLockFreePointerListLIFO<FMetalRHICommandContext> ParallelContexts;
	
	/** Critical section for FreeList */
	FCriticalSection FreeListMutex;
	
	/** Event for coordinating pausing of render thread to keep inline with the ios display link. */
	FEvent* FrameReadyEvent;
	
	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;
	
	/** Internal frame counter, used to ensure that we only drain the buffer pool one after each frame within RHIEndFrame. */
	uint32 FrameCounter;
	
	/** Bitfield of supported Metal features with varying availability depending on OS/device */
	uint32 Features;
	
	/** Count of concurrent contexts encoding commands. */
	uint32 ActiveContexts;
};
