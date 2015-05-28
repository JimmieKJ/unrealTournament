// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "IOSView.h"


struct FPipelineShadow
{
	FPipelineShadow()
	{
		// zero out our memory
		FMemory::Memzero(this, sizeof(*this));

		for (int Index = 0; Index < MaxMetalRenderTargets; Index++)
		{
			RenderTargets[Index] = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];
		}
	}

	/**
	 * @return the hash of the current pipeline state
	 */
	uint64 GetHash() const
	{
		return Hash;
	}

// 	/**
// 	 * FOrce a pipeline state for a known hash
// 	 */
// 	void SetHash(uint64 InHash);
	
	/**
	 * @return an pipeline state object that matches the current state and the BSS
	 */
	id<MTLRenderPipelineState> CreatePipelineStateForBoundShaderState(FMetalBoundShaderState* BSS) const;

	MTLRenderPipelineColorAttachmentDescriptor* RenderTargets[MaxMetalRenderTargets];
	MTLPixelFormat DepthTargetFormat;
	MTLPixelFormat StencilTargetFormat;
	uint32 SampleCount;

	// running hash of the pipeline state
	uint64 Hash;
};

struct FRingBuffer
{
	FRingBuffer(id<MTLDevice> Device, uint32 Size, uint32 InDefaultAlignment)
	{
		DefaultAlignment = InDefaultAlignment;
		Buffer = [Device newBufferWithLength:Size options:BUFFER_CACHE_MODE];
		Offset = 0;
	}

	uint32 Allocate(uint32 Size, uint32 Alignment);

	uint32 DefaultAlignment;
	id<MTLBuffer> Buffer;
	uint32 Offset;
};

class FMetalManager
{
public:
	static FMetalManager* Get();
	static id<MTLDevice> GetDevice();
	static id<MTLRenderCommandEncoder> GetContext();
	static id<MTLComputeCommandEncoder> GetComputeContext();
	static void ReleaseObject(id Object);
	
	/** RHIBeginScene helper */
	void BeginScene();
	/** RHIEndScene helper */
	void EndScene();

	void BeginFrame();
	void EndFrame(bool bPresent);
	
	/**
	 * Return a texture object that wraps the backbuffer
	 */
	FMetalTexture2D* GetBackBuffer();

//	/**
//	 * Sets a pending stream source, which will be hooked up to the vertex decl in PrepareToDraw
//	 */
//	void SetPendingStream(uint32 StreamIndex, TRefCountPtr<FMetalVertexBuffer> VertexBuffer, uint32 Offset, uint32 Stride);

	/**
	 * Do anything necessary to prepare for any kind of draw call 
	 */
	void PrepareToDraw(uint32 NumVertices);
	
	/**
	 * Functions to update the pipeline descriptor and/or context
	 */
	void SetBlendState(FMetalBlendState* BlendState);
	void SetBoundShaderState(FMetalBoundShaderState* BoundShaderState);
	void SetCurrentRenderTarget(FMetalSurface* RenderSurface, int32 RenderTargetIndex, uint32 MipIndex, uint32 ArraySliceIndex, MTLLoadAction LoadAction, MTLStoreAction StoreAction, int32 TotalNumRenderTargets);
	void SetCurrentDepthStencilTarget(FMetalSurface* RenderSurface, MTLLoadAction DepthLoadAction=MTLLoadActionDontCare, MTLStoreAction DepthStoreAction=MTLStoreActionDontCare, float ClearDepthValue=0, 
		MTLLoadAction StencilLoadAction=MTLLoadActionDontCare, MTLStoreAction InStencilStoreAction=MTLStoreActionDontCare, uint8 ClearStencilValue=0);
	
	/**
	 * Set the color, depth and stencil render targets, and then make the new command buffer/encoder
	 */
	void SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& RenderTargetsInfo);

	/**
	 * Update the context with the current render targets
	 */
	void UpdateContext();
	
	/**
	 * Allocate from a dynamic ring buffer - by default align to the allowed alignment for offset field when setting buffers
	 */
	uint32 AllocateFromRingBuffer(uint32 Size, uint32 Alignment=0);
	id<MTLBuffer> GetRingBuffer()
	{
		return RingBuffer.Buffer;
	}

	uint32 AllocateFromQueryBuffer();
	id<MTLBuffer> GetQueryBuffer()
	{
		return QueryBuffer.Buffer;
	}

	FEvent* GetCurrentQueryCompleteEvent()
	{
		checkf(CurrentQueryEventIndex < QueryEvents[WhichFreeList].Num(), TEXT("Attempted to get the current query event, but it doesn't exist"));
		return QueryEvents[WhichFreeList][CurrentQueryEventIndex];
	}

	void SubmitCommandBufferAndWait();
	void SubmitComputeCommandBufferAndWait();

	void SetRasterizerState(const FRasterizerStateInitializerRHI& State);

	FMetalShaderParameterCache& GetShaderParameters(uint32 Stage)
	{
		return ShaderParameters[Stage];
	}

	/**
	 * Return an auto-released command buffer, caller will need to retain it if it needs to live awhile
	 */
	id<MTLCommandBuffer> CreateTempCommandBuffer(bool bRetainReferences)
	{
		return bRetainReferences ? [CommandQueue commandBuffer] : [CommandQueue commandBufferWithUnretainedReferences];
	}

	/**
	 * Handle rendering thread starting/stopping
	 */
	void CreateAutoreleasePool();
	void DrainAutoreleasePool();

	void SetComputeShader(FMetalComputeShader* InComputeShader);
	void Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ);

	void ResizeBackBuffer(uint32 InSizeX, uint32 InSizeY);
	FIntPoint GetBoundRenderTargetDimensions() { return BoundRenderTargetDimensions; }

protected:
	FMetalManager();
	void InitFrame();
	void GenerateFetchShader();

	void CreateCurrentCommandBuffer(bool bWait);

	/**
	 * If the given surface is rendering to the back buffer, make sure it's fixed up
	 */
	void ConditionalUpdateBackBuffer(FMetalSurface& Surface);

	/**
	 * Check to see if the new RenderTargetInfo needs to create an entire new encoder/command buffer
	 */
	bool NeedsToSetRenderTarget(const FRHISetRenderTargetsInfo& RenderTargetsInfo);

	/**
	 * Possibly switch from compute to graphics
	 */
	void ConditionalSwitchToGraphics();
	
	/**
	 * Possibly switch from graphics to compute
	 */
	void ConditionalSwitchToCompute();
	

	id<MTLDevice> Device;

	id<MTLCommandQueue> CommandQueue;
	
	uint32 NumDrawCalls;
	id<MTLCommandBuffer> CurrentCommandBuffer;
	id<CAMetalDrawable> CurrentDrawable;

	dispatch_semaphore_t CommandBufferSemaphore;

	id<MTLRenderCommandEncoder> CurrentContext;
	id<MTLComputeCommandEncoder> CurrentComputeContext;

	struct FRenderTargetViewInfo
	{
		uint32 MipIndex;
		uint32 ArraySliceIndex;
		MTLLoadAction LoadAction;
		MTLStoreAction StoreAction;
	};

	struct FDepthRenderTargetViewInfo
	{
		float ClearDepthValue;
		MTLLoadAction LoadAction;
		MTLStoreAction StoreAction;
	};

	struct FStencilRenderTargetViewInfo
	{
		uint8 ClearStencilValue;
		MTLLoadAction LoadAction;
		MTLStoreAction StoreAction;
	};

	FRHISetRenderTargetsInfo PreviousRenderTargetsInfo;

	FRenderTargetViewInfo CurrentRenderTargetsViewInfo[MaxMetalRenderTargets], PreviousRenderTargetsViewInfo[MaxMetalRenderTargets];
	uint32 CurrentNumRenderTargets, PreviousNumRenderTargets;
	id<MTLTexture> CurrentColorRenderTextures[MaxMetalRenderTargets], PreviousColorRenderTextures[MaxMetalRenderTargets];
	FDepthRenderTargetViewInfo CurrentDepthViewInfo, PreviousDepthViewInfo;
	FStencilRenderTargetViewInfo CurrentStencilViewInfo, PreviousStencilViewInfo;
	id<MTLTexture> CurrentDepthRenderTexture, PreviousDepthRenderTexture;
	id<MTLTexture> CurrentStencilRenderTexture, PreviousStencilRenderTexture;
	id<MTLTexture> CurrentMSAARenderTexture;
	
	TRefCountPtr<FMetalTexture2D> BackBuffer;
	TRefCountPtr<FMetalBoundShaderState> CurrentBoundShaderState;
	
	class FMetalShaderParameterCache*	ShaderParameters;

	TRefCountPtr<FMetalComputeShader> CurrentComputeShader;

	// the running pipeline state descriptor object
	FPipelineShadow Pipeline;

	MTLRenderPipelineDescriptor* CurrentPipelineDescriptor;
	// a parallel descriptor for no depth buffer (since we can't reset the depth format to invalid YET)
	MTLRenderPipelineDescriptor* CurrentPipelineDescriptor2;
	

	// the amazing ring buffer for dynamic data
	FRingBuffer RingBuffer, QueryBuffer;

	TArray<id> DelayedFreeLists[4];
	uint32 WhichFreeList;
	TArray<FEvent*> QueryEvents[4];
	int32 CurrentQueryEventIndex;

	// the slot to store a per-thread autorelease pool
	uint32 AutoReleasePoolTLSSlot;

	// always growing command buffer index
	uint64 CommandBufferIndex;
	uint64 CompletedCommandBufferIndex;

	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;

	/**
	 * Internal counter used for resource table caching.
	 * INDEX_NONE means caching is not allowed.
	 */
	uint32 ResourceTableFrameCounter;

	// shadow the rasterizer state to reduce some render encoder pressure
	FRasterizerStateInitializerRHI ShadowRasterizerState;
	bool bFirstRasterizerState;

	/** Apply the SRT before drawing */
	void CommitGraphicsResourceTables();

	void CommitNonComputeShaderConstants();

	/** The dimensions of the currently bound RT */
	FIntPoint BoundRenderTargetDimensions;

private:
	
	/** Event for coordinating pausing of render thread to keep inline with the ios display link. */
	FEvent* FrameReadyEvent;
};

// Stats
DECLARE_CYCLE_STAT_EXTERN(TEXT("MakeDrawable time"),STAT_MetalMakeDrawableTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call time"),STAT_MetalDrawCallTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PrepareDraw time"),STAT_MetalPrepareDrawTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PipelineState time"),STAT_MetalPipelineStateTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BpoundShaderState time"),STAT_MetalBoundShaderStateTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("VertexDeclaration time"),STAT_MetalVertexDeclarationTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Uniform buffer pool cleanup time"), STAT_MetalUniformBufferCleanupTime, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Uniform buffer pool memory"), STAT_MetalFreeUniformBufferMemory, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Uniform buffer pool num free"), STAT_MetalNumFreeUniformBuffers, STATGROUP_MetalRHI, );
