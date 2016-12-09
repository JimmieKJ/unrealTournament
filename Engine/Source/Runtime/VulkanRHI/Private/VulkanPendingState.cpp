// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.cpp: Private VulkanPendingState function definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"

#if UE_BUILD_DEBUG
struct FDebugPipelineKey
{
	union
	{
		struct
		{
			uint64 BlendState			: NUMBITS_BLEND_STATE;

			uint64 RenderTargetFormat0	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat1	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat2	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat3	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetLoad0	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad1	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad2	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad3	: NUMBITS_LOAD_OP;
			uint64 RenderTargetStore0	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore1	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore2	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore3	: NUMBITS_STORE_OP;
			uint64 CullMode				: NUMBITS_CULL_MODE;
			uint64 PolyFill				: NUMBITS_POLYFILL;
			uint64 PolyType				: NUMBITS_POLYTYPE;
			uint64 DepthBiasEnabled		: 1;
			uint64 DepthTestEnabled		: 1;
			uint64 DepthWriteEnabled	: 1;
			uint64 DepthCompareOp		: NUMBITS_DEPTH_COMPARE_OP;
			uint64 FrontStencilOp		: NUMBITS_STENCIL_STATE;
			uint64 DepthStencilFormat	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 DepthStencilLoad		: NUMBITS_LOAD_OP;
			uint64 DepthStencilStore	: NUMBITS_STORE_OP;
			uint64						: 0;

			uint64 RenderTargetFormat4	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat5	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat6	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetFormat7	: NUMBITS_RENDER_TARGET_FORMAT;
			uint64 RenderTargetLoad4	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad5	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad6	: NUMBITS_LOAD_OP;
			uint64 RenderTargetLoad7	: NUMBITS_LOAD_OP;
			uint64 RenderTargetStore4	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore5	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore6	: NUMBITS_STORE_OP;
			uint64 RenderTargetStore7	: NUMBITS_STORE_OP;
			uint64 BackStencilOp		: NUMBITS_STENCIL_STATE;
			uint64 StencilTestEnabled	: 1;
			uint64 MSAAEnabled			: 1;
			uint64 NumColorBlends		: NUMBITS_NUM_COLOR_BLENDS;
			uint64: 0;
		};
		uint64 Key[2];
	};

	FDebugPipelineKey()
	{
		static_assert(sizeof(*this) == sizeof(FVulkanPipelineGraphicsKey), "size mismatch!");
		{
			// Sanity check that bits match, runs at startup time only
			FVulkanPipelineGraphicsKey CurrentKeys;

#define TEST_KEY(Offset, NumBits, Member)	\
{\
	FMemory::Memzero(*this);\
	FMemory::Memzero(CurrentKeys);\
	uint32 Value = (1 << NumBits) - 1;\
	/*SetKeyBits(CurrentKeys, Offset, NumBits, Value);*/\
	Member = Value;\
	/*check(CurrentKeys.Key[0] == Key[0] && CurrentKeys.Key[1] == Key[1] && Member == Member);*/\
}
			TEST_KEY(OFFSET_BLEND_STATE, NUMBITS_BLEND_STATE, BlendState);

			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT0, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat0);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT1, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat1);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT2, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat2);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT3, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat3);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT4, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat4);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT5, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat5);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT6, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat6);
			TEST_KEY(OFFSET_RENDER_TARGET_FORMAT7, NUMBITS_RENDER_TARGET_FORMAT, RenderTargetFormat7);
			TEST_KEY(OFFSET_DEPTH_STENCIL_FORMAT, NUMBITS_RENDER_TARGET_FORMAT, DepthStencilFormat);

			TEST_KEY(OFFSET_RENDER_TARGET_LOAD0, NUMBITS_LOAD_OP, RenderTargetLoad0);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD1, NUMBITS_LOAD_OP, RenderTargetLoad1);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD2, NUMBITS_LOAD_OP, RenderTargetLoad2);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD3, NUMBITS_LOAD_OP, RenderTargetLoad3);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD4, NUMBITS_LOAD_OP, RenderTargetLoad4);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD5, NUMBITS_LOAD_OP, RenderTargetLoad5);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD6, NUMBITS_LOAD_OP, RenderTargetLoad6);
			TEST_KEY(OFFSET_RENDER_TARGET_LOAD7, NUMBITS_LOAD_OP, RenderTargetLoad7);
			TEST_KEY(OFFSET_DEPTH_STENCIL_LOAD, NUMBITS_LOAD_OP, DepthStencilLoad);

			TEST_KEY(OFFSET_RENDER_TARGET_STORE0, NUMBITS_STORE_OP, RenderTargetStore0);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE1, NUMBITS_STORE_OP, RenderTargetStore1);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE2, NUMBITS_STORE_OP, RenderTargetStore2);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE3, NUMBITS_STORE_OP, RenderTargetStore3);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE4, NUMBITS_STORE_OP, RenderTargetStore4);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE5, NUMBITS_STORE_OP, RenderTargetStore5);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE6, NUMBITS_STORE_OP, RenderTargetStore6);
			TEST_KEY(OFFSET_RENDER_TARGET_STORE7, NUMBITS_STORE_OP, RenderTargetStore7);
			TEST_KEY(OFFSET_DEPTH_STENCIL_STORE, NUMBITS_STORE_OP, DepthStencilStore);

			TEST_KEY(OFFSET_CULL_MODE, NUMBITS_CULL_MODE, CullMode);
			TEST_KEY(OFFSET_POLYFILL, NUMBITS_POLYFILL, PolyFill);
			TEST_KEY(OFFSET_POLYTYPE, NUMBITS_POLYTYPE, PolyType);

			TEST_KEY(OFFSET_DEPTH_BIAS_ENABLED, 1, DepthBiasEnabled);
			TEST_KEY(OFFSET_DEPTH_TEST_ENABLED, 1, DepthTestEnabled);
			TEST_KEY(OFFSET_DEPTH_WRITE_ENABLED, 1, DepthWriteEnabled);
			TEST_KEY(OFFSET_DEPTH_COMPARE_OP, NUMBITS_DEPTH_COMPARE_OP, DepthCompareOp);

			TEST_KEY(OFFSET_FRONT_STENCIL_STATE, NUMBITS_STENCIL_STATE, FrontStencilOp);
			TEST_KEY(OFFSET_BACK_STENCIL_STATE, NUMBITS_STENCIL_STATE, BackStencilOp);
			TEST_KEY(OFFSET_STENCIL_TEST_ENABLED, 1, StencilTestEnabled);

			TEST_KEY(OFFSET_MSAA_ENABLED, 1, MSAAEnabled);
			TEST_KEY(OFFSET_NUM_COLOR_BLENDS, NUMBITS_NUM_COLOR_BLENDS, NumColorBlends);
		}
	}
};
static FDebugPipelineKey GDebugPipelineKey;
static_assert(sizeof(GDebugPipelineKey) == 2 * sizeof(uint64), "Debug struct not matching Hash/Sizes!");
#endif

FVulkanPendingState::FVulkanPendingState(FVulkanDevice* InDevice)
	: Device(InDevice)
	, GlobalUniformPool(nullptr)
{
	// Create the global uniform pool
	GlobalUniformPool = new FVulkanGlobalUniformPool();
}

FVulkanPendingState::~FVulkanPendingState()
{
	delete GlobalUniformPool;
	GlobalUniformPool = nullptr;
}

FVulkanPendingGfxState::FVulkanPendingGfxState(FVulkanDevice* InDevice)
	: FVulkanPendingState(InDevice)
	, bScissorEnable(false)
{
	Reset();

    // Create the global uniform pool
    GlobalUniformPool = new FVulkanGlobalUniformPool();
}

FVulkanPendingGfxState::~FVulkanPendingGfxState()
{
}


// Expected to be called after render pass has been ended
// and only from "FVulkanDynamicRHI::RHIEndDrawingViewport()"
void FVulkanPendingGfxState::Reset()
{
	CurrentState.Reset();

	FMemory::Memzero(PendingStreams);
}

FVulkanDescriptorPool::FVulkanDescriptorPool(FVulkanDevice* InDevice)
	: Device(InDevice)
	, DescriptorPool(VK_NULL_HANDLE)
	, MaxDescriptorSets(0)
	, NumAllocatedDescriptorSets(0)
	, PeakAllocatedDescriptorSets(0)
{
	// Increased from 8192 to prevent Protostar crashing on Mali
	MaxDescriptorSets = 16384;

	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	FMemory::Memzero(MaxAllocatedTypes);
	FMemory::Memzero(NumAllocatedTypes);
	FMemory::Memzero(PeakAllocatedTypes);

	//#todo-rco: Get some initial values
	uint32 LimitMaxUniformBuffers = 2048;
	uint32 LimitMaxSamplers = 1024;
	uint32 LimitMaxCombinedImageSamplers = 4096;
	uint32 LimitMaxUniformTexelBuffers = 512;
	uint32 LimitMaxStorageTexelBuffers = 512;
	uint32 LimitMaxStorageImage = 512;

	TArray<VkDescriptorPoolSize> Types;
	VkDescriptorPoolSize* Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	Type->descriptorCount = LimitMaxUniformBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_SAMPLER;
	Type->descriptorCount = LimitMaxSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	Type->descriptorCount = LimitMaxCombinedImageSamplers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxUniformTexelBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	Type->descriptorCount = LimitMaxStorageTexelBuffers;

	Type = new(Types) VkDescriptorPoolSize;
	FMemory::Memzero(*Type);
	Type->type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	Type->descriptorCount = LimitMaxStorageImage;

	for (const VkDescriptorPoolSize& PoolSize : Types)
	{
		MaxAllocatedTypes[PoolSize.type] = PoolSize.descriptorCount;
	}

	VkDescriptorPoolCreateInfo PoolInfo;
	FMemory::Memzero(PoolInfo);
	PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, nullptr, &DescriptorPool));
}

FVulkanDescriptorPool::~FVulkanDescriptorPool()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, nullptr);
		DescriptorPool = VK_NULL_HANDLE;
	}
}

void FVulkanDescriptorPool::TrackAddUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	// Check and increment our current type usage
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] +=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		PeakAllocatedTypes[TypeIndex] = FMath::Max(PeakAllocatedTypes[TypeIndex], NumAllocatedTypes[TypeIndex]);
	}

	NumAllocatedDescriptorSets += Layout.GetLayouts().Num();
	PeakAllocatedDescriptorSets = FMath::Max(NumAllocatedDescriptorSets, PeakAllocatedDescriptorSets);
}

void FVulkanDescriptorPool::TrackRemoveUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] -=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		check(NumAllocatedTypes[TypeIndex] >= 0);
	}

	NumAllocatedDescriptorSets -= Layout.GetLayouts().Num();
}

inline void FVulkanComputeShaderState::BindDescriptorSets(FVulkanCmdBuffer* Cmd)
{
	check(CurrDescriptorSets);
	CurrDescriptorSets->Bind(Cmd, GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
}


inline void FVulkanBoundShaderState::BindDescriptorSets(FVulkanCmdBuffer* Cmd)
{
	check(CurrDescriptorSets);
	CurrDescriptorSets->Bind(Cmd, GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void FVulkanPendingComputeState::PrepareDispatch(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* Cmd)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallPrepareTime);

	check(CurrentState.CSS);
	bool bHasDescriptorSets = CurrentState.CSS->UpdateDescriptorSets(CmdListContext, Cmd, GlobalUniformPool);

	FVulkanComputePipeline* Pipeline = CurrentState.CSS->PrepareForDispatch(CurrentState);

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		VkPipeline NewPipeline = Pipeline->GetHandle();
		CurrentState.CSS->BindPipeline(Cmd->GetHandle(), NewPipeline);
		if (bHasDescriptorSets)
		{
			CurrentState.CSS->BindDescriptorSets(Cmd);
		}
	}
}

void FVulkanPendingGfxState::PrepareDraw(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* Cmd, VkPrimitiveTopology Topology)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallPrepareTime);

	checkf(Topology < (1 << NUMBITS_POLYTYPE), TEXT("PolygonMode was too high a value for the PSO key [%d]"), Topology);
	SetKeyBits(CurrentKey, OFFSET_POLYTYPE, NUMBITS_POLYTYPE, Topology);

	check(CurrentState.BSS);
    bool bHasDescriptorSets = CurrentState.BSS->UpdateDescriptorSets(CmdListContext, Cmd, GlobalUniformPool);

	// let the BoundShaderState return a pipeline object for the full current state of things
	CurrentState.InputAssembly.topology = Topology;
	FVulkanGfxPipeline* Pipeline = CurrentState.BSS->PrepareForDraw(CmdListContext->GetCurrentRenderPass() ? CmdListContext->GetCurrentRenderPass() : CmdListContext->GetPreviousRenderPass(), CurrentKey, CurrentState.BSS->GetVertexInputStateInfo().GetHash(), CurrentState);

	check(Pipeline);

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		VkPipeline NewPipeline = Pipeline->GetHandle();
		CurrentState.BSS->BindPipeline(Cmd->GetHandle(), NewPipeline);
		Pipeline->UpdateDynamicStates(Cmd, CurrentState);
		if (bHasDescriptorSets)
		{
			CurrentState.BSS->BindDescriptorSets(Cmd);
		}
		CurrentState.BSS->BindVertexStreams(Cmd, PendingStreams);
	}
}

void FVulkanPendingGfxState::InitFrame()
{
}

void FVulkanPendingGfxState::SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	VkViewport& vp = CurrentState.Viewport;
	FMemory::Memzero(vp);

	vp.x = MinX;
	vp.y = MinY;
	vp.width = MaxX - MinX;
	vp.height = MaxY - MinY;

	// Engine parses in some cases MaxZ as 0.0, which is rubbish.
	if(MinZ == MaxZ)
	{
		vp.minDepth = MinZ;
		vp.maxDepth = MinZ + 1.0f;
	}
	else
	{
		vp.minDepth = MinZ;
		vp.maxDepth = MaxZ;
	}

	CurrentState.bNeedsViewportUpdate = true;

	// Set scissor to match the viewport when disabled
	if (!bScissorEnable)
	{
		SetScissorRect(vp.x, vp.y, vp.width, vp.height);
	}
}

void FVulkanPendingGfxState::SetScissor(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	bScissorEnable = bEnable;

	if (bScissorEnable)
	{
		SetScissorRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
	}
	else
	{
		const VkViewport& vp = CurrentState.Viewport;
		SetScissorRect(vp.x, vp.y, vp.width, vp.height);
	}
}

void FVulkanPendingGfxState::SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
{
	VkRect2D& Scissor = CurrentState.Scissor;
	FMemory::Memzero(Scissor);

	Scissor.offset.x = MinX;
	Scissor.offset.y = MinY;
	Scissor.extent.width = Width;
	Scissor.extent.height = Height;

	// todo vulkan: compare against previous (and viewport above)
	CurrentState.bNeedsScissorUpdate = true;
}

void FVulkanPendingGfxState::SetBoundShaderState(TRefCountPtr<FVulkanBoundShaderState> InBoundShaderState)
{
	check(InBoundShaderState);
	InBoundShaderState->ResetState();
	CurrentState.BSS = InBoundShaderState;
}

FVulkanBoundShaderState& FVulkanPendingGfxState::GetBoundShaderState()
{
	check(CurrentState.BSS);
	return *CurrentState.BSS;
}


void FVulkanPendingGfxState::SetBlendState(FVulkanBlendState* NewState)
{
	check(NewState);
	CurrentState.BlendState = NewState;

	// set blend modes into the key
	SetKeyBits(CurrentKey, OFFSET_BLEND_STATE, NUMBITS_BLEND_STATE, NewState->BlendStateKey);
}

void FVulkanPendingGfxState::SetDepthStencilState(FVulkanDepthStencilState* NewState, uint32 StencilRef)
{
	check(NewState);
	CurrentState.DepthStencilState = NewState;
	CurrentState.StencilRef = StencilRef;
	CurrentState.bNeedsStencilRefUpdate = true;

	SetKeyBits(CurrentKey, OFFSET_DEPTH_TEST_ENABLED, 1, NewState->DepthStencilState.depthTestEnable);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_WRITE_ENABLED, 1, NewState->DepthStencilState.depthWriteEnable);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_COMPARE_OP, NUMBITS_DEPTH_COMPARE_OP, NewState->DepthStencilState.depthCompareOp);
	SetKeyBits(CurrentKey, OFFSET_STENCIL_TEST_ENABLED, 1, NewState->DepthStencilState.stencilTestEnable);
	SetKeyBits(CurrentKey, OFFSET_FRONT_STENCIL_STATE, NUMBITS_STENCIL_STATE, NewState->FrontStencilKey);
	SetKeyBits(CurrentKey, OFFSET_BACK_STENCIL_STATE, NUMBITS_STENCIL_STATE, NewState->BackStencilKey);
}

void FVulkanPendingGfxState::SetRasterizerState(FVulkanRasterizerState* NewState)
{
	check(NewState);

	CurrentState.RasterizerState = NewState;

	// update running key
	SetKeyBits(CurrentKey, OFFSET_CULL_MODE, NUMBITS_CULL_MODE, NewState->RasterizerState.cullMode);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_BIAS_ENABLED, 1, NewState->RasterizerState.depthBiasEnable);
	SetKeyBits(CurrentKey, OFFSET_POLYFILL, NUMBITS_POLYFILL, NewState->RasterizerState.polygonMode == VK_POLYGON_MODE_FILL);
}
