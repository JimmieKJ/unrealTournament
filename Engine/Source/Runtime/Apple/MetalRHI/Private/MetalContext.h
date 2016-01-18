// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "MetalViewport.h"
#include "MetalBufferPools.h"
#include "MetalCommandEncoder.h"
#include "MetalCommandQueue.h"
#if PLATFORM_IOS
#include "IOSView.h"
#endif

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
	EMetalFeaturesResourceOptions = 1 << 3
};

class FMetalContext
{
public:
	FMetalContext();
	~FMetalContext();
	
	id<MTLDevice> GetDevice();
	FMetalCommandQueue& GetCommandQueue();
	FMetalCommandEncoder& GetCommandEncoder();
	id<MTLRenderCommandEncoder> GetRenderContext();
	id<MTLBlitCommandEncoder> GetBlitContext();
	id<MTLCommandBuffer> GetCurrentCommandBuffer();
	FMetalStateCache& GetCurrentState() { return StateCache; }
	bool SupportsFeature(EMetalFeatures InFeature) { return ((Features & InFeature) != 0); }
	
	/** Return an auto-released command buffer, caller will need to retain it if it needs to live awhile */
	id<MTLCommandBuffer> CreateCommandBuffer(bool bRetainReferences)
	{
		return bRetainReferences ? CommandQueue.CreateRetainedCommandBuffer() : CommandQueue.CreateUnretainedCommandBuffer();
	}
	
	FMetalBufferPool* GetBufferPool();
	FMetalPooledBuffer CreatePooledBuffer(FMetalPooledBufferArgs const& Args);
	void ReleasePooledBuffer(FMetalPooledBuffer Buf);
	void ReleaseObject(id Object);
	
	/**
	 * Handle rendering thread starting/stopping
	 */
	void CreateAutoreleasePool();
	void DrainAutoreleasePool();
	
	void BeginFrame();
	void EndFrame();
	
	/** RHIBeginScene helper */
	void BeginScene();
	/** RHIEndScene helper */
	void EndScene();

	void BeginDrawingViewport(FMetalViewport* Viewport);
	void EndDrawingViewport(FMetalViewport* Viewport, bool bPresent);

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
		return RingBuffer.Buffer;
	}

	FMetalQueryBufferPool& GetQueryBufferPool()
	{
		return QueryBuffer;
	}

    void SubmitCommandsHint();
	void SubmitCommandBufferAndWait();
	void SubmitComputeCommandBufferAndWait();
	void ResetRenderCommandEncoder();

	void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);

	void StartTiming(class FMetalEventNode* EventNode);
	void EndTiming(class FMetalEventNode* EventNode);

protected:
	void InitFrame();

	/** Create & set the current command buffer, waiting on outstanding command buffers if required. */
	void CreateCurrentCommandBuffer(bool bWait);

	/**
	 * Possibly switch from compute to graphics
	 */
	void ConditionalSwitchToGraphics();
	
	/**
	 * Possibly switch from graphics to compute
	 */
	void ConditionalSwitchToCompute();
	
	/**
	 * Switch to blitting
	 */
	void ConditionalSwitchToBlit();
	
	/** Apply the SRT before drawing */
	void CommitGraphicsResourceTables();
	
	void CommitNonComputeShaderConstants();
	
private:
	FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI);
	
	FORCEINLINE void SetResource(uint32 ShaderStage, uint32 BindIndex, FMetalSamplerState* RESTRICT SamplerState);
	
	template <typename MetalResourceType>
	inline int32 SetShaderResourcesFromBuffer(uint32 ShaderStage, FMetalUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex);
	
	template <class ShaderType>
	void SetResourcesFromTables(ShaderType Shader, uint32 ShaderStage);
	
protected:
	/** Dynamic buffer pool */
	FMetalBufferPool BufferPool;
	
	/** The underlying Metal device */
	id<MTLDevice> Device;
	
	/** The wrapper around the device command-queue for creating & committing command buffers to */
	FMetalCommandQueue CommandQueue;
	
	/** The wrapper for encoding commands into the current command buffer. */
	FMetalCommandEncoder CommandEncoder;
	
	/** The cache of all tracked & accessible state. */
	FMetalStateCache StateCache;
	
	/** The current command buffer that receives new commands. */
	id<MTLCommandBuffer> CurrentCommandBuffer;
	
	/** A sempahore used to ensure that wait for previous frames to complete if more are in flight than we permit */
	dispatch_semaphore_t CommandBufferSemaphore;
	
	/** Free lists for releasing objects only once it is safe to do so */
	TSet<id> FreeList;
	struct FMetalDelayedFreeList
	{
		FEvent* Signal;
		TSet<id> FreeList;
	};
	TArray<FMetalDelayedFreeList*> DelayedFreeLists;
	
	/** A simple fixed-size ring buffer for dynamic data */
	FRingBuffer RingBuffer;
	
	/** A pool of buffers for writing visibility query results. */
	FMetalQueryBufferPool QueryBuffer;
	
	/** the slot to store a per-thread autorelease pool */
	uint32 AutoReleasePoolTLSSlot;
	
	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;
	
	/** Internal frame counter, used to ensure that we only drain the buffer pool one after each frame within RHIEndFrame. */
	uint32 FrameCounter;
	
	/**
	 * Internal counter used for resource table caching.
	 * INDEX_NONE means caching is not allowed.
	 */
	uint32 ResourceTableFrameCounter;
	
	/** Event for coordinating pausing of render thread to keep inline with the ios display link. */
	FEvent* FrameReadyEvent;
	
	/** Bitfield of supported Metal features with varying availability depending on OS/device */
	uint32 Features;
};

