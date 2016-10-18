// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.h: Private VulkanPendingState definitions.
=============================================================================*/

#pragma once

// Dependencies
#include "VulkanRHI.h"
#include "VulkanPipeline.h"

typedef uint32 FStateKey;

class FVulkanPendingState
{
public:
	typedef TMap<FStateKey, FVulkanRenderPass*> FMapRenderPass;
	typedef TMap<FStateKey, TArray<FVulkanFramebuffer*> > FMapFrameBufferArray;

	FVulkanPendingState(FVulkanDevice* InDevice);

	~FVulkanPendingState();

    FVulkanGlobalUniformPool& GetGlobalUniformPool();

	void SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& InRTInfo);

	void Reset();

	bool RenderPassBegin(FVulkanCmdBuffer* CmdBuffer);

	void PrepareDraw(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, VkPrimitiveTopology Topology);

	void RenderPassEnd(FVulkanCmdBuffer* CmdBuffer);

	inline bool IsRenderPassActive() const
	{
		return bBeginRenderPass;
	}
	
	void SetBoundShaderState(TRefCountPtr<FVulkanBoundShaderState> InBoundShaderState);

	// Pipeline states
	void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ);
	void SetScissor(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY);
	void SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height);
	void SetBlendState(FVulkanBlendState* NewState);
	void SetDepthStencilState(FVulkanDepthStencilState* NewState, uint32 StencilRef);
	void SetRasterizerState(FVulkanRasterizerState* NewState);

	// Returns shader state
	// TODO: Move binding/layout functionality to the shader (who owns what)?
	FVulkanBoundShaderState& GetBoundShaderState();

	// Retuns constructed render pass
	inline FVulkanRenderPass& GetRenderPass()
	{
		check(CurrentState.RenderPass);
		return *CurrentState.RenderPass;
	}

	inline FVulkanFramebuffer* GetFrameBuffer()
	{
		return CurrentState.FrameBuffer;
	}

	void NotifyDeletedRenderTarget(const FVulkanTextureBase* Texture);

	inline void UpdateRenderPass(FVulkanCmdBuffer* CmdBuffer)
	{
		//#todo-rco: Don't test here, move earlier to SetRenderTarget
		if (bChangeRenderTarget)
		{
			if (bBeginRenderPass)
			{
				RenderPassEnd(CmdBuffer);
			}

			if (!RenderPassBegin(CmdBuffer))
			{
				return;
			}
			bChangeRenderTarget = false;
		}
		if (!bBeginRenderPass)
		{
			RenderPassBegin(CmdBuffer);
			bChangeRenderTarget = false;
		}
	}

	void SetStreamSource(uint32 StreamIndex, FVulkanBuffer* VertexBuffer, uint32 Stride, uint32 Offset)
	{
		PendingStreams[StreamIndex].Stream = VertexBuffer;
		PendingStreams[StreamIndex].Stream2  = nullptr;
		PendingStreams[StreamIndex].Stream3  = VK_NULL_HANDLE;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		if (CurrentState.Shader)
		{
			CurrentState.Shader->MarkDirtyVertexStreams();
		}
	}

	void SetStreamSource(uint32 StreamIndex, FVulkanResourceMultiBuffer* VertexBuffer, uint32 Stride, uint32 Offset)
	{
		PendingStreams[StreamIndex].Stream = nullptr;
		PendingStreams[StreamIndex].Stream2 = VertexBuffer;
		PendingStreams[StreamIndex].Stream3  = VK_NULL_HANDLE;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		if (CurrentState.Shader)
		{
			CurrentState.Shader->MarkDirtyVertexStreams();
		}
	}

	void SetStreamSource(uint32 StreamIndex, VkBuffer InBuffer, uint32 Stride, uint32 Offset)
	{
		PendingStreams[StreamIndex].Stream = nullptr;
		PendingStreams[StreamIndex].Stream2 = nullptr;
		PendingStreams[StreamIndex].Stream3  = InBuffer;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		if (CurrentState.Shader)
		{
			CurrentState.Shader->MarkDirtyVertexStreams();
		}
	}

	struct FVertexStream
	{
		FVertexStream() : Stream(nullptr), Stream2(nullptr), Stream3(VK_NULL_HANDLE), BufferOffset(0) {}
		FVulkanBuffer* Stream;
		FVulkanResourceMultiBuffer* Stream2;
		VkBuffer Stream3;
		uint32 BufferOffset;
	};

	inline const FVertexStream* GetVertexStreams() const
	{
		return PendingStreams;
	}

	void InitFrame();

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

	friend class FVulkanCommandListContext;
	
	FRHISetRenderTargetsInfo RTInfo;

	// Resources caching
	FMapRenderPass RenderPassMap;
	FMapFrameBufferArray FrameBufferMap;

	FVertexStream PendingStreams[MaxVertexElementCount];

	// running key of the current pipeline state
	FVulkanPipelineGraphicsKey CurrentKey;

	// bResetMap true if only reset the map, false to free the map's memory
	void DestroyFrameBuffers(bool bResetMap);
};
