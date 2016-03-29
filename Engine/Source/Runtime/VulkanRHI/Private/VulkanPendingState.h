// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.h: Private VulkanPendingState definitions.
=============================================================================*/

#pragma once

// Dependencies
#include "VulkanRHI.h"
#include "VulkanPipeline.h"

struct FVulkanPendingState
{
	typedef TMap<FStateKey, FVulkanRenderPass*> FMapRenderRass;
	typedef TMap<FStateKey, TArray<FVulkanFramebuffer*> > FMapFrameBufferArray;

	FVulkanPendingState(FVulkanDevice* InDevice);

	~FVulkanPendingState();

	FVulkanDescriptorSets* AllocateDescriptorSet(const FVulkanBoundShaderState* BoundShaderState);
	void DeallocateDescriptorSet(FVulkanDescriptorSets*& DescriptorSet, const FVulkanBoundShaderState* BoundShaderState);

    FVulkanGlobalUniformPool& GetGlobalUniformPool();

	void SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& InRTInfo);

	void Reset();

	// Needs to be called before "PrepareDraw" and also starts writing in to the command buffer
	void RenderPassBegin();

	// Needs to be called before any calling a draw-call
	// Binds graphics pipeline
	void PrepareDraw(VkPrimitiveTopology Topology);

	// Is currently linked to the command buffer
	// Will get called in RHIEndDrawingViewport through SubmitPendingCommandBuffers
	void RenderPassEnd();

	inline bool IsRenderPassActive() const
	{
		return bBeginRenderPass;
	}
	
	void SetBoundShaderState(TRefCountPtr<FVulkanBoundShaderState> InBoundShaderState);

	//#todo-rco: Temp hack...
	typedef void (TCallback)(void*);
	void SubmitPendingCommandBuffers(TCallback* Callback, void* CallbackUserData);

	void SubmitPendingCommandBuffersBlockingNoRenderPass();

	// Pipeline states
	void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ);
	void SetScissor(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY);
	void SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height);
	void SetBlendState(FVulkanBlendState* NewState);
	void SetDepthStencilState(FVulkanDepthStencilState* NewState);
	void SetRasterizerState(FVulkanRasterizerState* NewState);

	// Returns shader state
	// TODO: Move binding/layout functionality to the shader (who owns what)?
	FVulkanBoundShaderState& GetBoundShaderState();

	// Retuns constructed render pass
	FVulkanRenderPass& GetRenderPass();

	// Returns currently bound command buffer
	VkCommandBuffer& GetCommandBuffer();
	const VkBool32 GetIsCommandBufferEmpty() const;

	FVulkanFramebuffer* GetFrameBuffer();

	inline void UpdateRenderPass()
	{
		//#todo-rco: Don't test here, move earlier to SetRenderTarget
		if (bChangeRenderTarget)
		{
			if (bBeginRenderPass)
			{
				RenderPassEnd();
			}

			RenderPassBegin();
			bChangeRenderTarget = false;
		}
	}

	inline const FVulkanPipelineGraphicsKey& GetCurrentKey() const
	{
		return CurrentKey;
	}

	void SetStreamSource(uint32 StreamIndex, FVulkanBuffer* VertexBuffer, uint32 Stride, uint32 Offset)
	{
		PendingStreams[StreamIndex].Stream  = VertexBuffer;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		if (CurrentState.Shader)
		{
			CurrentState.Shader->MarkDirtyVertexStreams();
		}
	}

	struct FVertexStream
	{
		FVertexStream() : Stream(nullptr), BufferOffset(0) {}
		FVulkanBuffer* Stream;
		uint32 BufferOffset;
	};

	inline const FVertexStream* GetVertexStreams() const
	{
		return PendingStreams;
	}

	void InitFrame();

	inline FVulkanCmdBuffer& GetCommandBuffer(uint32 Index)
	{
		check(Index<VULKAN_NUM_COMMAND_BUFFERS);
		check(CmdBuffers[Index]);
		return *CmdBuffers[Index];
	}

	inline uint32 GetCurrentCommandBufferIndex() const
	{
		return CurrentCmdBufferIndex;
	}

	// Returns current frame command buffer
	// The index of the command buffer changes after "Reset()" is called
	inline FVulkanCmdBuffer& GetCurrentCommandBuffer()
	{
		check(CmdBuffers[CurrentCmdBufferIndex]);
		return *CmdBuffers[CurrentCmdBufferIndex];
	}

	// Returns current frame command buffer
	// The index of the command buffer changes after "Reset()" is called
	inline const FVulkanCmdBuffer& GetCurrentCommandBuffer() const
	{
		check(CmdBuffers[CurrentCmdBufferIndex]);
		return *CmdBuffers[CurrentCmdBufferIndex];
	}

private:
	FVulkanRenderPass* GetOrCreateRenderPass(const FVulkanRenderTargetLayout& RTLayout);
	FVulkanFramebuffer* GetOrCreateFramebuffer(const FRHISetRenderTargetsInfo& RHIRTInfo,
		const FVulkanRenderTargetLayout& RTInfo, const FVulkanRenderPass& RenderPass);

	bool NeedsToSetRenderTarget(const FRHISetRenderTargetsInfo& InRTInfo);

private:
	FVulkanDevice* Device;

	bool bBeginRenderPass;
	bool bChangeRenderTarget;
	bool bScissorEnable;

	FVulkanGlobalUniformPool* GlobalUniformPool;

	FVulkanPipelineState CurrentState;

	//@TODO: probably needs to go somewhere else
	FRHISetRenderTargetsInfo PrevRenderTargetsInfo;

	//#todo-rco: FIX ME! ASAP!!!
	FVulkanCmdBuffer* CmdBuffers[VULKAN_NUM_COMMAND_BUFFERS];
	uint32	CurrentCmdBufferIndex;
	
	FRHISetRenderTargetsInfo RTInfo;

	// Resources caching
	FMapRenderRass RenderPassMap;
	FMapFrameBufferArray FrameBufferMap;

	FVertexStream PendingStreams[MaxVertexElementCount];

	// running key of the current pipeline state
	FVulkanPipelineGraphicsKey CurrentKey;
};
