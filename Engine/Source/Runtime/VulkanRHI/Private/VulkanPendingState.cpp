// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.cpp: Private VulkanPendingState function definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"

// RTs 0-3
#define NUMBITS_BLEND_STATE				4 //(x4=16) = 16 ++
#define NUMBITS_RENDER_TARGET_FORMAT	4 //(x4=16) = 32 ++
#define NUMBITS_LOAD_OP					2 //(x4=8) = 40 ++
#define NUMBITS_STORE_OP				2 //(x4=8) = 48 ++
#define NUMBITS_CULL_MODE				2 //(x1=2) = 50 ++
#define NUMBITS_POLYFILL				1 //(x1=1) = 51 ++
#define NUMBITS_POLYTYPE				3 //(x3=3) = 54 ++
#define NUMBITS_DEPTH_BIAS_ENABLED		1 //(x1=1) = 55 ++
#define NUMBITS_DEPTH_TEST_ENABLED		1 //(x1=1) = 56 ++
#define NUMBITS_DEPTH_WRITE_ENABLED		1 //(x1=1) = 57 ++
#define NUMBITS_DEPTH_COMPARE_OP		3 //(x1=3) = 60 ++
#define NUMBITS_STENCIL_TEST_ENABLED	1 //(x1=1) = 61 ++
#define NUMBITS_FRONT_STENCIL_OP		3 //(x1=3) = 64 ++

// RTs 4-7
//#define NUMBITS_BLEND_STATE				4 //(x4=16) = 16 ++
//#define NUMBITS_RENDER_TARGET_FORMAT	3 //(x4=16) = 32 ++
//#define NUMBITS_LOAD_OP					2 //(x4=8) = 40 ++
//#define NUMBITS_STORE_OP				2 //(x4=8) = 48 ++
#define NUMBITS_BACK_STENCIL_OP			3 //(x1=3) = 51 ++
#define NUMBITS_MSAA_ENABLED			1 //(x1=1) = 52 ++


#define OFFSET_BLEND_STATE0				(0)
#define OFFSET_BLEND_STATE1				(OFFSET_BLEND_STATE0			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE2				(OFFSET_BLEND_STATE1			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE3				(OFFSET_BLEND_STATE2			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT0	(OFFSET_BLEND_STATE3			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT1	(OFFSET_RENDER_TARGET_FORMAT0	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT2	(OFFSET_RENDER_TARGET_FORMAT1	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT3	(OFFSET_RENDER_TARGET_FORMAT2	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD0		(OFFSET_RENDER_TARGET_FORMAT3	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD1		(OFFSET_RENDER_TARGET_LOAD0		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD2		(OFFSET_RENDER_TARGET_LOAD1		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD3		(OFFSET_RENDER_TARGET_LOAD2		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_STORE0		(OFFSET_RENDER_TARGET_LOAD3		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_STORE1		(OFFSET_RENDER_TARGET_STORE0	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE2		(OFFSET_RENDER_TARGET_STORE1	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE3		(OFFSET_RENDER_TARGET_STORE2	+ NUMBITS_STORE_OP)
#define OFFSET_CULL_MODE				(OFFSET_RENDER_TARGET_STORE3	+ NUMBITS_STORE_OP)
#define OFFSET_POLYFILL					(OFFSET_CULL_MODE				+ NUMBITS_CULL_MODE)
#define OFFSET_POLYTYPE					(OFFSET_POLYFILL				+ NUMBITS_POLYFILL)
#define OFFSET_DEPTH_BIAS_ENABLED		(OFFSET_POLYTYPE				+ NUMBITS_POLYTYPE)
#define OFFSET_DEPTH_TEST_ENABLED		(OFFSET_DEPTH_BIAS_ENABLED		+ NUMBITS_DEPTH_BIAS_ENABLED)
#define OFFSET_DEPTH_WRITE_ENABLED		(OFFSET_DEPTH_TEST_ENABLED		+ NUMBITS_DEPTH_TEST_ENABLED)
#define OFFSET_DEPTH_COMPARE_OP			(OFFSET_DEPTH_WRITE_ENABLED		+ NUMBITS_DEPTH_WRITE_ENABLED)
#define OFFSET_STENCIL_TEST_ENABLED		(OFFSET_DEPTH_COMPARE_OP		+ NUMBITS_DEPTH_COMPARE_OP)
#define OFFSET_FRONT_STENCIL_OP			(OFFSET_STENCIL_TEST_ENABLED	+ NUMBITS_STENCIL_TEST_ENABLED)
static_assert(OFFSET_FRONT_STENCIL_OP + NUMBITS_FRONT_STENCIL_OP <= 64, "Out of bits!");

#define OFFSET_BLEND_STATE4				(0x8000)
#define OFFSET_BLEND_STATE5				(OFFSET_BLEND_STATE4			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE6				(OFFSET_BLEND_STATE5			+ NUMBITS_BLEND_STATE)
#define OFFSET_BLEND_STATE7				(OFFSET_BLEND_STATE6			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT4	(OFFSET_BLEND_STATE7			+ NUMBITS_BLEND_STATE)
#define OFFSET_RENDER_TARGET_FORMAT5	(OFFSET_RENDER_TARGET_FORMAT4	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT6	(OFFSET_RENDER_TARGET_FORMAT5	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_FORMAT7	(OFFSET_RENDER_TARGET_FORMAT6	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD4		(OFFSET_RENDER_TARGET_FORMAT7	+ NUMBITS_RENDER_TARGET_FORMAT)
#define OFFSET_RENDER_TARGET_LOAD5		(OFFSET_RENDER_TARGET_LOAD4		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD6		(OFFSET_RENDER_TARGET_LOAD5		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_LOAD7		(OFFSET_RENDER_TARGET_LOAD6		+ NUMBITS_LOAD_OP)
#define OFFSET_RENDER_TARGET_STORE4		(OFFSET_RENDER_TARGET_LOAD7		+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE5		(OFFSET_RENDER_TARGET_STORE4	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE6		(OFFSET_RENDER_TARGET_STORE5	+ NUMBITS_STORE_OP)
#define OFFSET_RENDER_TARGET_STORE7		(OFFSET_RENDER_TARGET_STORE6	+ NUMBITS_STORE_OP)
#define OFFSET_BACK_STENCIL_OP			(OFFSET_RENDER_TARGET_STORE7	+ NUMBITS_STORE_OP)
#define OFFSET_MSAA_ENABLED				(OFFSET_BACK_STENCIL_OP			+ NUMBITS_BACK_STENCIL_OP)
static_assert(((OFFSET_MSAA_ENABLED + NUMBITS_MSAA_ENABLED) & ~0x8000) <= 64, "Out of bits!");

static const uint32 BlendBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_BLEND_STATE0,
	OFFSET_BLEND_STATE1,
	OFFSET_BLEND_STATE2,
	OFFSET_BLEND_STATE3,
	OFFSET_BLEND_STATE4,
	OFFSET_BLEND_STATE5,
	OFFSET_BLEND_STATE6,
	OFFSET_BLEND_STATE7
};
static const uint32 RTFormatBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_RENDER_TARGET_FORMAT0,
	OFFSET_RENDER_TARGET_FORMAT1,
	OFFSET_RENDER_TARGET_FORMAT2,
	OFFSET_RENDER_TARGET_FORMAT3,
	OFFSET_RENDER_TARGET_FORMAT4,
	OFFSET_RENDER_TARGET_FORMAT5,
	OFFSET_RENDER_TARGET_FORMAT6,
	OFFSET_RENDER_TARGET_FORMAT7
};
static const uint32 RTLoadBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_RENDER_TARGET_LOAD0,
	OFFSET_RENDER_TARGET_LOAD1,
	OFFSET_RENDER_TARGET_LOAD2,
	OFFSET_RENDER_TARGET_LOAD3,
	OFFSET_RENDER_TARGET_LOAD4,
	OFFSET_RENDER_TARGET_LOAD5,
	OFFSET_RENDER_TARGET_LOAD6,
	OFFSET_RENDER_TARGET_LOAD7
};
static const uint32 RTStoreBitOffsets[MaxSimultaneousRenderTargets] =
{
	OFFSET_RENDER_TARGET_STORE0,
	OFFSET_RENDER_TARGET_STORE1,
	OFFSET_RENDER_TARGET_STORE2,
	OFFSET_RENDER_TARGET_STORE3,
	OFFSET_RENDER_TARGET_STORE4,
	OFFSET_RENDER_TARGET_STORE5,
	OFFSET_RENDER_TARGET_STORE6,
	OFFSET_RENDER_TARGET_STORE7
};

static FORCEINLINE uint64* GetKey(FVulkanPipelineGraphicsKey& Key, uint64 Offset)
{
	return Key.Key + (((Offset & 0x8000) != 0) ? 1 : 0);
}

static FORCEINLINE void SetKeyBits(FVulkanPipelineGraphicsKey& Key, uint64 Offset, uint64 NumBits, uint64 Value)
{
	uint64& CurrentKey = *GetKey(Key, Offset);
	Offset = (Offset & ~0x8000);
	const uint64 BitMask = ((1ULL << NumBits) - 1) << Offset;
	CurrentKey = (CurrentKey & ~BitMask) | (((uint64)(Value) << Offset) & BitMask); \
}

struct FDebugPipelineKey
{
	union
	{
		struct
		{
			uint64 BlendState0			: NUMBITS_BLEND_STATE;
			uint64 BlendState1			: NUMBITS_BLEND_STATE;
			uint64 BlendState2			: NUMBITS_BLEND_STATE;
			uint64 BlendState3			: NUMBITS_BLEND_STATE;
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
			uint64 DepthBiasEnabled		: NUMBITS_DEPTH_BIAS_ENABLED;
			uint64 DepthTestEnabled		: NUMBITS_DEPTH_TEST_ENABLED;
			uint64 DepthWriteEnabled	: NUMBITS_DEPTH_WRITE_ENABLED;
			uint64 DepthCompareOp		: NUMBITS_DEPTH_COMPARE_OP;
			uint64 StencilTestEnabled	: NUMBITS_STENCIL_TEST_ENABLED;
			uint64 FrontStencilOp		: NUMBITS_FRONT_STENCIL_OP;
			uint64						: 0;

			uint64 BlendState4			: NUMBITS_BLEND_STATE;
			uint64 BlendState5			: NUMBITS_BLEND_STATE;
			uint64 BlendState6			: NUMBITS_BLEND_STATE;
			uint64 BlendState7			: NUMBITS_BLEND_STATE;
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
			uint64 BackStencilOp		: NUMBITS_BACK_STENCIL_OP;
			uint64 MSAAEnabled			: NUMBITS_MSAA_ENABLED;
			uint64: 0;
		};
		uint64 Key[2];
	};

	FDebugPipelineKey()
	{
		static_assert(sizeof(*this) == sizeof(FVulkanPipelineGraphicsKey), "size mismatch");
		{
			// Sanity check that bits match
			FVulkanPipelineGraphicsKey CurrentKeys;
			FMemory::Memzero(*this);

			SetKeyBits(CurrentKeys, OFFSET_CULL_MODE, NUMBITS_CULL_MODE, 1);
			CullMode = 1;

			SetKeyBits(CurrentKeys, OFFSET_BLEND_STATE0, NUMBITS_BLEND_STATE, 7);
			BlendState0 = 7;

			SetKeyBits(CurrentKeys, OFFSET_BLEND_STATE5, NUMBITS_BLEND_STATE, 3);
			BlendState5 = 3;

			SetKeyBits(CurrentKeys, OFFSET_MSAA_ENABLED, NUMBITS_MSAA_ENABLED, 1);
			MSAAEnabled = 1;

			check(CurrentKeys.Key[0] == Key[0] && CurrentKeys.Key[1] == Key[1]);
		}

		{
			FVulkanPipelineGraphicsKey CurrentKeys;
			FMemory::Memzero(*this);

			SetKeyBits(CurrentKeys, OFFSET_POLYFILL, NUMBITS_POLYFILL, 1);
			PolyFill = 1;

			SetKeyBits(CurrentKeys, OFFSET_RENDER_TARGET_LOAD2, NUMBITS_LOAD_OP, 2);
			RenderTargetLoad2 = 2;

			SetKeyBits(CurrentKeys, OFFSET_BLEND_STATE5, NUMBITS_BLEND_STATE, 3);
			BlendState5 = 3;

			SetKeyBits(CurrentKeys, OFFSET_FRONT_STENCIL_OP, NUMBITS_FRONT_STENCIL_OP, 5);
			FrontStencilOp = 5;

			check(CurrentKeys.Key[0] == Key[0] && CurrentKeys.Key[1] == Key[1]);
		}
	}
};
static FDebugPipelineKey GDebugPipelineKey;
static_assert(sizeof(GDebugPipelineKey) == 2 * sizeof(uint64), "Debug struct not matching Hash/Sizes!");

#if VULKAN_HAS_DEBUGGING_ENABLED
inline uint8 LoadOpToChar(ERenderTargetLoadAction InLoadAction)
{
	uint8 OutLoadAction = 'E';

	switch (InLoadAction)
	{
	case ERenderTargetLoadAction::ELoad:		OutLoadAction = 'L';	break;
	case ERenderTargetLoadAction::EClear:		OutLoadAction = 'C';	break;
	case ERenderTargetLoadAction::ENoAction:	OutLoadAction = 'D';	break;
	default:															break;
	}

	return OutLoadAction;
}
#endif

FVulkanPendingState::FVulkanPendingState(FVulkanDevice* InDevice)
	: Device(InDevice)
	, bBeginRenderPass(false)
	, bChangeRenderTarget(false)
	, GlobalUniformPool(nullptr)
	, CurrentCmdBufferIndex(0)
	, bScissorEnable(false)
{
	check(Device);

	for (uint32 CmdBufferIndex = 0; CmdBufferIndex < VULKAN_NUM_COMMAND_BUFFERS; ++CmdBufferIndex)
	{
		//#todo-rco: FIX ME!
		CmdBuffers[CmdBufferIndex] = Device->GetImmediateContext().GetCommandBufferManager()->Create();
	}

	Reset();

    // Create the global uniform pool
    GlobalUniformPool = new FVulkanGlobalUniformPool();
}

FVulkanPendingState::~FVulkanPendingState()
{
	check(Device);

	for (uint32 CmdBufferIndex = 0; CmdBufferIndex < VULKAN_NUM_COMMAND_BUFFERS; ++CmdBufferIndex)
	{
		//#todo-rco: FIX ME!
		Device->GetImmediateContext().GetCommandBufferManager()->Destroy(CmdBuffers[CmdBufferIndex]);
		CmdBuffers[CmdBufferIndex] = nullptr;
	}

	//Reset();

	/*
	if (RenderPass)
	{
		RenderPassMap.Remove(RenderPass->GetLayout().GetHash());
		delete RenderPass;
		RenderPass = nullptr;
	}
	*/

	delete GlobalUniformPool;

	for (auto& Pair : FrameBufferMap)
	{
		TArray<FVulkanFramebuffer*>& Entries = Pair.Value;
		for (auto* Entry : Entries)
		{
			Entry->Destroy(*Device);
			delete Entry;
		}
	}

	for (auto& Pair : RenderPassMap)
	{
		delete Pair.Value;
	}
	FrameBufferMap.Empty(0);
	RenderPassMap.Empty(0);
}

FVulkanDescriptorSets* FVulkanPendingState::AllocateDescriptorSet(const FVulkanBoundShaderState* BoundShaderState)
{
	FVulkanDescriptorSets* DescriptorSet = new FVulkanDescriptorSets(Device, BoundShaderState, Device->GetDescriptorPool());
	return DescriptorSet;
}

void FVulkanPendingState::DeallocateDescriptorSet(FVulkanDescriptorSets*& DescriptorSet, const FVulkanBoundShaderState* BoundShaderState)
{
	delete DescriptorSet;
	DescriptorSet = nullptr;
}

FVulkanGlobalUniformPool& FVulkanPendingState::GetGlobalUniformPool()
{
    check(GlobalUniformPool);
    return *GlobalUniformPool;
}

void FVulkanPendingState::RenderPassBegin()
{
	check(!bBeginRenderPass);

	FVulkanRenderTargetLayout DesiredLayout(RTInfo);
	FVulkanRenderPass* NewRenderPass = GetOrCreateRenderPass(DesiredLayout);

	// use the first attachment's number of samples to check for msaa
	SetKeyBits(CurrentKey, OFFSET_MSAA_ENABLED, NUMBITS_MSAA_ENABLED, (DesiredLayout.GetAttachmentDescriptions()[0].samples > 1) ? 1 : 0);

	CurrentState.RenderPass = NewRenderPass;
	check(CurrentState.RenderPass);

	CurrentState.FrameBuffer = GetOrCreateFramebuffer(RTInfo, DesiredLayout, *CurrentState.RenderPass);
	check(CurrentState.FrameBuffer);

	VkClearValue ClearValues[MaxSimultaneousRenderTargets + 1];
	FMemory::Memzero(ClearValues);
	uint32 Index = 0;

	// First attachments are always color
	const uint32 NumColorTargets = GetFrameBuffer()->GetNumColorAttachments();
	for (; Index < NumColorTargets; Index++)
	{
		// default to invalid format (no attachment)
		uint8 FormatKey = 0;

		const FRHIRenderTargetView& rtv = RTInfo.ColorRenderTarget[Index];
		if (rtv.Texture)
		{
			// get the format key out of the texture's surface
			FormatKey = GetVulkanTextureFromRHITexture(rtv.Texture)->Surface.FormatKey;

			if (rtv.Texture->HasClearValue())
			{
				VkClearValue& DstColor = ClearValues[Index];
				FClearValueBinding UEClearVal = RTInfo.ColorRenderTarget[Index].Texture->GetClearBinding();

				// Set color
				FLinearColor ClearColor = UEClearVal.GetClearColor();
				DstColor.color.float32[0] = ClearColor.R;
				DstColor.color.float32[1] = ClearColor.G;
				DstColor.color.float32[2] = ClearColor.B;
				DstColor.color.float32[3] = ClearColor.A;
			}
		}
		
		// track the per-RT info
		SetKeyBits(CurrentKey, RTFormatBitOffsets[Index], NUMBITS_RENDER_TARGET_FORMAT, FormatKey);
		SetKeyBits(CurrentKey, RTLoadBitOffsets[Index], NUMBITS_LOAD_OP, (uint64)rtv.LoadAction);
		SetKeyBits(CurrentKey, RTStoreBitOffsets[Index], NUMBITS_STORE_OP, (uint64)rtv.StoreAction);
	}

	// Clear MRT key info to avoid different pipeline keys
	for (int32 RemainingRTsIndex = NumColorTargets; RemainingRTsIndex < MaxSimultaneousRenderTargets; ++RemainingRTsIndex)
	{
		SetKeyBits(CurrentKey, RTFormatBitOffsets[RemainingRTsIndex], NUMBITS_RENDER_TARGET_FORMAT, 0);
		SetKeyBits(CurrentKey, RTLoadBitOffsets[RemainingRTsIndex], NUMBITS_LOAD_OP, 0);
		SetKeyBits(CurrentKey, RTStoreBitOffsets[RemainingRTsIndex], NUMBITS_STORE_OP, 0);
	}

	// Last attachment if available is depth-stencil
	if (RTInfo.DepthStencilRenderTarget.Texture)
	{
		VkClearValue& DstColor = ClearValues[Index];
		RTInfo.DepthStencilRenderTarget.Texture->GetClearBinding().GetDepthStencil(DstColor.depthStencil.depth, DstColor.depthStencil.stencil);
		// @todo vulkan - we don't track the format of the depth buffer in the key, only if depth is enabled (write/test) - should be enough, but worth checking that
		// if either bit is set that we have a depth buffer attached
	}
		
	CurrentState.RenderPass->Begin(GetCurrentCommandBuffer(), CurrentState.FrameBuffer->GetHandle(), ClearValues);
	bBeginRenderPass = true;
}

void FVulkanPendingState::RenderPassEnd()
{
	check(bBeginRenderPass);
	CurrentState.RenderPass->End(GetCurrentCommandBuffer());
	bBeginRenderPass = false;
}

// Expected to be called after render pass has been ended
// and only from "FVulkanDynamicRHI::RHIEndDrawingViewport()"
void FVulkanPendingState::Reset()
{
	check(!bBeginRenderPass);

	CurrentState.Reset();

	CurrentCmdBufferIndex = (CurrentCmdBufferIndex + 1) % VULKAN_NUM_COMMAND_BUFFERS;

	GetCurrentCommandBuffer().Reset();
	GetCurrentCommandBuffer().Begin();

	FMemory::Memzero(PendingStreams);
}

FVulkanDescriptorPool::FVulkanDescriptorPool(FVulkanDevice* InDevice)
	: Device(InDevice)
	, DescriptorPool(VK_NULL_HANDLE)
	, MaxDescriptorSets(0)
	, NumAllocatedDescriptorSets(0)
	, PeakAllocatedDescriptorSets(0)
{
	check(Device != nullptr);

	//#todo-rco: Get a proper/dynamic numbers
	MaxDescriptorSets = 32 * 1024;
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	FMemory::Memzero(MaxAllocatedTypes);
	FMemory::Memzero(NumAllocatedTypes);
	FMemory::Memzero(PeakAllocatedTypes);

	//#todo-rco: Get some initial values
	const uint32 LimitMaxUniformBuffers = 1 * 1024 * 1024;
	const uint32 LimitMaxSamplers = 1 * 1024 * 1024;
	const uint32 LimitMaxCombinedImageSamplers = 1 * 1024 * 1024;
	const uint32 LimitMaxUniformTexelBuffers = 1 * 1024 * 1024;

	TArray<VkDescriptorPoolSize> Types;
	auto* Type = new(Types) VkDescriptorPoolSize;
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

	VkDescriptorPoolCreateInfo PoolInfo;
	FMemory::Memzero(PoolInfo);
	PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

	//@TODO: Android don't support descriptor pools on device, so this doesn't matter for us yet. might for the PC IDC though.
	PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

	VERIFYVULKANRESULT(vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, nullptr, &DescriptorPool));

	for (int32 Index = 0; Index < Types.Num(); ++Index)
	{
		auto& Type = Types[Index];
		MaxAllocatedTypes[Type.type] =Type.descriptorCount;
	}
}

FVulkanDescriptorPool::~FVulkanDescriptorPool()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, nullptr);
		DescriptorPool = VK_NULL_HANDLE;
	}
}

void FVulkanDescriptorPool::TrackAddUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	// Check if we can allocate new DescriptorSet
	checkf(NumAllocatedDescriptorSets < MaxDescriptorSets, TEXT("DescriptorSet allocation overflow %u"), MaxDescriptorSets);

	// Check and increment our current type usage
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] +=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		PeakAllocatedTypes[TypeIndex] = FMath::Max(PeakAllocatedTypes[TypeIndex], NumAllocatedTypes[TypeIndex]);

		check(NumAllocatedTypes[TypeIndex] <= MaxAllocatedTypes[TypeIndex]);
	}

	NumAllocatedDescriptorSets += Layout.GetLayouts().Num();
	PeakAllocatedDescriptorSets = FMath::Max(NumAllocatedDescriptorSets, PeakAllocatedDescriptorSets);

	check(NumAllocatedDescriptorSets <= MaxDescriptorSets);
/*
	// Notify number of descriptor sets usage
	// A notification is triggered on each ~10%.
	if ((NumAllocatedDescriptorSets % (MaxDescriptorSets/10)) == 0)
	{
		const float CapacityPercent = (float)NumAllocatedDescriptorSets/MaxDescriptorSets*100;

		if (CapacityPercent < 70.0f)
		{
			UE_LOG(LogVulkanRHI, Display, TEXT("DescriptorPool usage is at %2.2f%% capacity (%u/%u)"),
				CapacityPercent,
				NumAllocatedDescriptorSets, MaxDescriptorSets);
		}
		else
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("DescriptorPool usage is at %2.2f%% capacity (%u/%u)"),
				CapacityPercent,
				NumAllocatedDescriptorSets, MaxDescriptorSets);
		}
	}
*/
}

void FVulkanDescriptorPool::TrackRemoveUsage(const FVulkanDescriptorSetsLayout& Layout)
{
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex < VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		NumAllocatedTypes[TypeIndex] -=	(int32)Layout.GetTypesUsed((VkDescriptorType)TypeIndex);
		check(NumAllocatedTypes[TypeIndex] >= 0);
	}

	NumAllocatedDescriptorSets -= Layout.GetLayouts().Num();
	check(NumAllocatedDescriptorSets >= 0);
}


inline void FVulkanDescriptorSets::Bind(FVulkanCmdBuffer* Cmd, FVulkanBoundShaderState* State)
{
	//#todo-rco: Compute
	vkCmdBindDescriptorSets(Cmd->GetHandle(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		State->GetPipelineLayout(),
		0, Sets.Num(), Sets.GetData(),
		0, nullptr);
}

inline void FVulkanBoundShaderState::BindDescriptorSets(FVulkanCmdBuffer* Cmd)
{
	check(CurrDescriptorSets);
	CurrDescriptorSets->Bind(Cmd, this);
}

void FVulkanPendingState::PrepareDraw(VkPrimitiveTopology Topology)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallPrepareTime);

	checkf(Topology < (1 << NUMBITS_POLYTYPE), TEXT("PolygonMode was too high a value for the PSO key [%d]"), Topology);
	SetKeyBits(CurrentKey, OFFSET_POLYTYPE, NUMBITS_POLYTYPE, Topology);

	//@TODO: let's try not to do this per draw call?
	UpdateRenderPass();

	check(CurrentState.Shader);
    CurrentState.Shader->UpdateDescriptorSets(GlobalUniformPool);

	// let the BoundShaderState return a pipeline object for the full current state of things
	CurrentState.InputAssembly.topology = Topology;
	FVulkanPipeline* Pipeline = CurrentState.Shader->PrepareForDraw(CurrentKey, CurrentState.Shader->GetVertexInputStateInfo().GetHash(), CurrentState);

	check(Pipeline);

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
		auto* Cmd = &GetCurrentCommandBuffer();
		Pipeline->UpdateDynamicStates(Cmd, CurrentState);

		VkPipeline NewPipeline = Pipeline->GetHandle();
		CurrentState.Shader->BindPipeline(Cmd->GetHandle(), NewPipeline);

		CurrentState.Shader->BindDescriptorSets(Cmd);
		CurrentState.Shader->BindVertexStreams(Cmd, PendingStreams);

		switch (Topology)
		{
		default:
			if (CurrentState.RasterizerState->RasterizerState.polygonMode != VK_POLYGON_MODE_LINE)
			{
				break;
			}
			// Fall through...
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			vkCmdSetLineWidth(Cmd->GetHandle(), 1.0f);
			break;
		}
	}
}

void FVulkanPendingState::SubmitPendingCommandBuffers(FVulkanPendingState::TCallback* Callback, void* CallbackUserData)
{
	if (IsRenderPassActive())
	{
		RenderPassEnd();
	}

	if (Callback)
	{
		(*Callback)(CallbackUserData);
	}

	check(Device);
	//#todo-rco: FIX ME!
	Device->GetImmediateContext().GetCommandBufferManager()->Submit(&GetCurrentCommandBuffer());
}

void FVulkanPendingState::SubmitPendingCommandBuffersBlockingNoRenderPass()
{
	check(Device);
	Device->GetQueue()->SubmitBlocking(&GetCurrentCommandBuffer());
}

void FVulkanPendingState::SetRenderTargetsInfo(const FRHISetRenderTargetsInfo& InRTInfo)
{
	if (NeedsToSetRenderTarget(InRTInfo) == false)
	{
		return;
	}

	//@NOTE: this is only needed for the work-around below
	FRHISetRenderTargetsInfo PrevRTInfo = RTInfo;

	// back this up for the next SetRenderTarget
	PrevRenderTargetsInfo = InRTInfo;
	RTInfo = InRTInfo; // probably don't need both of these
	bChangeRenderTarget = true;

	//@NOTE: this is an awkward work-around for not supporting a null color attachment properly
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; Index++)
	{
		FRHIRenderTargetView& rtv = RTInfo.ColorRenderTarget[Index];
		if (rtv.Texture == nullptr)
		{
			rtv.Texture = PrevRTInfo.ColorRenderTarget[Index].Texture;
			check(rtv.Texture);
		}
	}
}

bool FVulkanPendingState::NeedsToSetRenderTarget(const FRHISetRenderTargetsInfo& InRTInfo)
{
	bool bAllChecksPassed = InRTInfo.NumColorRenderTargets == PrevRenderTargetsInfo.NumColorRenderTargets &&
		// handle the case where going from backbuffer + depth -> backbuffer + null, no need to reset RT and do a store/load
		(InRTInfo.DepthStencilRenderTarget.Texture == PrevRenderTargetsInfo.DepthStencilRenderTarget.Texture || InRTInfo.DepthStencilRenderTarget.Texture == nullptr);

	if (bAllChecksPassed)
	{
		for (int32 RenderTargetIndex = 0; RenderTargetIndex < InRTInfo.NumColorRenderTargets; RenderTargetIndex++)
		{
			const FRHIRenderTargetView& RenderTargetView = InRTInfo.ColorRenderTarget[RenderTargetIndex];
			const FRHIRenderTargetView& PrevRenderTargetView = PrevRenderTargetsInfo.ColorRenderTarget[RenderTargetIndex];

			// handle simple case of switching textures or mip/slice
			if (RenderTargetView.Texture != PrevRenderTargetView.Texture ||
				RenderTargetView.MipIndex != PrevRenderTargetView.MipIndex ||
				RenderTargetView.ArraySliceIndex != PrevRenderTargetView.ArraySliceIndex)
			{
				bAllChecksPassed = false;
				break;
			}

			// it's non-trivial when we need to switch based on load/store action:
			// LoadAction - it only matters what we are switching to in the new one
			//    If we switch to Load, no need to switch as we can re-use what we already have
			//    If we switch to Clear, we have to always switch to a new RT to force the clear
			//    If we switch to DontCare, there's definitely no need to switch
			if (RenderTargetView.LoadAction == ERenderTargetLoadAction::EClear)
			{
				bAllChecksPassed = false;
				break;
			}
			// StoreAction - this matters what the previous one was **In Spirit**
			//    If we come from Store, we need to switch to a new RT to force the store
			//    If we come from DontCare, then there's no need to switch
			//    @todo metal: However, we basically only use Store now, and don't
			//        care about intermediate results, only final, so we don't currently check the value
			//			if (PreviousRenderTargetView.StoreAction == ERenderTTargetStoreAction::EStore)
			//			{
			//				bAllChecksPassed = false;
			//				break;
			//			}
		}
	}

	// if we are setting them to nothing, then this is probably end of frame, and we can't make a framebuffer
	// with nothng, so just abort this (only need to check on single MRT case)
	if (InRTInfo.NumColorRenderTargets == 1 && InRTInfo.ColorRenderTarget[0].Texture == nullptr &&
		InRTInfo.DepthStencilRenderTarget.Texture == nullptr)
	{
		bAllChecksPassed = true;
	}

	return bAllChecksPassed == false;
}

void FVulkanPendingState::InitFrame()
{
	// make sure the first SetRenderTarget goes through
	PrevRenderTargetsInfo.NumColorRenderTargets = -1;
}

FVulkanRenderPass* FVulkanPendingState::GetOrCreateRenderPass(const FVulkanRenderTargetLayout& RTLayout)
{
	uint32 Hash = RTLayout.GetHash();
	auto** RenderPassFound = RenderPassMap.Find(Hash);
	if (RenderPassFound)
	{
		return *RenderPassFound;
	}

	check(Device);
	auto* OutRenderPass = new FVulkanRenderPass(*Device, RTLayout);
	RenderPassMap.Add(Hash, OutRenderPass);
	return OutRenderPass;
}

FVulkanFramebuffer* FVulkanPendingState::GetOrCreateFramebuffer(const FRHISetRenderTargetsInfo& RHIRTInfo, const FVulkanRenderTargetLayout& InRTInfo, const FVulkanRenderPass& inRenderPass)
{
	uint32 Hash = InRTInfo.GetHash();
	auto& FramebufferList = FrameBufferMap.FindOrAdd(Hash);
	for (auto* Framebuffer : FramebufferList)
	{
		if (Framebuffer->Matches(RHIRTInfo))
		{
			return Framebuffer;
		}
	}

	check(Device);
	auto* OutFramebuffer = new FVulkanFramebuffer(*Device, RHIRTInfo, InRTInfo, inRenderPass);
	FramebufferList.Add(OutFramebuffer);
	return OutFramebuffer;
}

FVulkanRenderPass& FVulkanPendingState::GetRenderPass()
{
	check(CurrentState.RenderPass);
	return *CurrentState.RenderPass;
}

VkCommandBuffer& FVulkanPendingState::GetCommandBuffer()
{
	return GetCurrentCommandBuffer().GetHandle();
}

const VkBool32 FVulkanPendingState::GetIsCommandBufferEmpty() const
{
	return GetCurrentCommandBuffer().GetIsEmpty();
}

void FVulkanPendingState::SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
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

void FVulkanPendingState::SetScissor(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
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

void FVulkanPendingState::SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
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

void FVulkanPendingState::SetBoundShaderState(TRefCountPtr<FVulkanBoundShaderState> InBoundShaderState)
{
	check(InBoundShaderState);
	InBoundShaderState->ResetState();
	CurrentState.Shader = InBoundShaderState;
}

FVulkanBoundShaderState& FVulkanPendingState::GetBoundShaderState()
{
	check(CurrentState.Shader);
	return *CurrentState.Shader;
}


void FVulkanPendingState::SetBlendState(FVulkanBlendState* NewState)
{
	check(NewState);
	CurrentState.BlendState = NewState;

	// set blend modes into the key
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		SetKeyBits(CurrentKey, BlendBitOffsets[Index], NUMBITS_BLEND_STATE, NewState->BlendStateKeys[Index]);
	}
}


void FVulkanPendingState::SetDepthStencilState(FVulkanDepthStencilState* NewState)
{
	check(NewState);
	CurrentState.DepthStencilState = NewState;

	SetKeyBits(CurrentKey, OFFSET_DEPTH_TEST_ENABLED, NUMBITS_DEPTH_TEST_ENABLED, NewState->DepthStencilState.depthTestEnable);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_WRITE_ENABLED, NUMBITS_DEPTH_WRITE_ENABLED, NewState->DepthStencilState.depthWriteEnable);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_COMPARE_OP, NUMBITS_DEPTH_COMPARE_OP, NewState->DepthStencilState.depthCompareOp);
	SetKeyBits(CurrentKey, OFFSET_STENCIL_TEST_ENABLED, NUMBITS_STENCIL_TEST_ENABLED, NewState->DepthStencilState.stencilTestEnable);
	SetKeyBits(CurrentKey, OFFSET_FRONT_STENCIL_OP, NUMBITS_FRONT_STENCIL_OP, NewState->FrontStencilKey);
	SetKeyBits(CurrentKey, OFFSET_BACK_STENCIL_OP, NUMBITS_BACK_STENCIL_OP, NewState->BackStencilKey);
}

void FVulkanPendingState::SetRasterizerState(FVulkanRasterizerState* NewState)
{
	check(NewState);

	CurrentState.RasterizerState = NewState;

	// update running key
	SetKeyBits(CurrentKey, OFFSET_CULL_MODE, NUMBITS_CULL_MODE, NewState->RasterizerState.cullMode);
	SetKeyBits(CurrentKey, OFFSET_DEPTH_BIAS_ENABLED, NUMBITS_DEPTH_BIAS_ENABLED, NewState->RasterizerState.depthBiasEnable);
	SetKeyBits(CurrentKey, OFFSET_POLYFILL, NUMBITS_POLYFILL, NewState->RasterizerState.polygonMode == VK_POLYGON_MODE_FILL);
}

FVulkanFramebuffer* FVulkanPendingState::GetFrameBuffer()
{
	return CurrentState.FrameBuffer;
}
