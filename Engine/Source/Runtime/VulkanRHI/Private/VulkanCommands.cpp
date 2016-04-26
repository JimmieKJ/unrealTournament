// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommands.cpp: Vulkan RHI commands implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanManager.h"

//#todo-rco: One of this per Context!
static TGlobalResource< TBoundShaderStateHistory<10000, false> > GBoundShaderStateHistory;

static inline bool UseRealUBs()
{
	static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
	return (CVar && CVar->GetValueOnAnyThread() != 0);
}

static inline VkPrimitiveTopology UEToVulkanType(EPrimitiveType PrimitiveType)
{
	switch (PrimitiveType)
	{
	case PT_PointList:			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case PT_LineList:			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PT_TriangleList:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PT_TriangleStrip:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	default:
		break;
	}

	checkf(false, TEXT("Unsupported primitive type"));
	return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

void FVulkanCommandListContext::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	FVulkanPendingState& State = Device->GetPendingState();
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (VertexBuffer != NULL)
	{
		State.SetStreamSource(StreamIndex, VertexBuffer->GetBuffer(), Stride, Offset + VertexBuffer->GetOffset());
	}
}

void FVulkanDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	FVulkanRasterizerState* NewState = ResourceCast(NewStateRHI);
	check(NewState);

	FVulkanPendingState& state = Device->GetPendingState();
	state.SetRasterizerState(NewState);
}

void FVulkanCommandListContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
#if 0
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
#endif

	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) 
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{ 
#if 0
	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
#endif

	VULKAN_SIGNAL_UNIMPLEMENTED();
	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(1);
	}
}

void FVulkanCommandListContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	TRefCountPtr<FVulkanBoundShaderState> BoundShaderState = ResourceCast(BoundShaderStateRHI);

	// Store in the history so it doesn't get released
	GBoundShaderStateHistory.Add(BoundShaderState);

	check(Device);
	FVulkanPendingState& State = Device->GetPendingState();
	State.SetBoundShaderState(BoundShaderState);
}


void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
#if 0
	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
#endif

	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
#if 0
	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
#endif

	VULKAN_SIGNAL_UNIMPLEMENTED();
}


void FVulkanCommandListContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(Device);
	FVulkanPendingState& PendingState = Device->GetPendingState();
	FVulkanBoundShaderState& BSS = PendingState.GetBoundShaderState();

	// Verify presence of a vertex shader and it has to be the same shader
	check(ResourceCast(VertexShaderRHI) == &BSS.GetShader(SF_Vertex));

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	BSS.SetTexture(SF_Vertex, TextureIndex, VulkanTexture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	check(Device);
	FVulkanPendingState& PendingState = Device->GetPendingState();
	FVulkanBoundShaderState& BSS = PendingState.GetBoundShaderState();

	// Verify presence of a pixel shader and it has to be the same shader
	check(ResourceCast(PixelShaderRHI) == &BSS.GetShader(SF_Pixel));

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	BSS.SetTexture(SF_Pixel, TextureIndex, VulkanTexture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}


void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	check(Device);
	Device->BindSRV(ResourceCast(SRVRHI), TextureIndex, SF_Vertex);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(Device);
	Device->BindSRV(ResourceCast(SRVRHI), TextureIndex, SF_Pixel);
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}


void FVulkanCommandListContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(Device);
	FVulkanPendingState& state = Device->GetPendingState();
	FVulkanBoundShaderState& ShaderState = state.GetBoundShaderState();

	check(&ShaderState.GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	ShaderState.SetSamplerState(SF_Vertex, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	check(Device);
	FVulkanPendingState& state = Device->GetPendingState();
	FVulkanBoundShaderState& ShaderState = state.GetBoundShaderState();

	check(&ShaderState.GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	ShaderState.SetSamplerState(SF_Pixel, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(Device);
	FVulkanPendingState& state = Device->GetPendingState();
	FVulkanBoundShaderState& ShaderState = state.GetBoundShaderState();

	// Verify shader
	check(&ShaderState.GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));
	ShaderState.SetShaderParameter(SF_Vertex, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	check(Device);
	FVulkanPendingState& state = Device->GetPendingState();
	FVulkanBoundShaderState& ShaderState = state.GetBoundShaderState();

	// Verify shader
	check(&ShaderState.GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));
	ShaderState.SetShaderParameter(SF_Pixel, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

struct FSrtResourceBinding
{
	typedef TRefCountPtr<FRHIResource> ResourceRef;

	FSrtResourceBinding(): BindingIndex(-1), Resource(nullptr) {}
	FSrtResourceBinding(int32 InBindingIndex, FRHIResource* InResource): BindingIndex(InBindingIndex), Resource(InResource) {}

	int32 BindingIndex;
	ResourceRef Resource;
};

static inline void GatherUniformBufferResources(
	const TArray<uint32>& InBindingArray,
	const uint32& InBindingMask,
	const FVulkanUniformBuffer* UniformBuffer,
	uint32 BufferIndex,
	TArray<FSrtResourceBinding>& OutResourcesBindings)
{
	check(UniformBuffer);

    if (!((1 << BufferIndex) & InBindingMask))
	{
		return;
	}

	const TArray<TRefCountPtr<FRHIResource>>& ResourceArray = UniformBuffer->GetResourceTable();

	// Expected to get an empty array
	check(OutResourcesBindings.Num() == 0);

	// Verify mask and array corelational validity
	check(InBindingMask == 0 ? (InBindingArray.Num() == 0) : (InBindingArray.Num() > 0));

	// InBindingArray contains index to the buffer offset and also buffer offsets
	uint32 BufferOffset = InBindingArray[BufferIndex];
	const uint32* ResourceInfos = &InBindingArray[BufferOffset];
	uint32 ResourceInfo = *ResourceInfos++;

	// Extract all resources related to the currend BufferIndex
	do
	{
		// Verify that we have correct buffer index
		check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

		// Extract binding index from ResourceInfo
		const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

		// Extract index of the resource stored in the resource table from ResourceInfo
		const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

		if(ResourceIndex < ResourceArray.Num())
		{
			check(ResourceArray[ResourceIndex]);
			OutResourcesBindings.Add(FSrtResourceBinding(BindingIndex, ResourceArray[ResourceIndex]));
		}

		// Iterate to next info
		ResourceInfo = *ResourceInfos++;
	}
	while(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
}

inline void FVulkanCommandListContext::SetShaderUniformBuffer(EShaderFrequency Stage, const FVulkanUniformBuffer* UniformBuffer, int32 BindingIndex)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif

	FVulkanPendingState& PendingState = Device->GetPendingState();
	FVulkanBoundShaderState& ShaderState = PendingState.GetBoundShaderState();

	// Walk through all resources to set all appropriate states
	const FVulkanShader& Shader = ShaderState.GetShader(Stage);

	// Uniform Buffers
	if (UseRealUBs())
	{
		ShaderState.SetUniformBuffer(PendingState, Stage,
			BindingIndex,
			UniformBuffer->GetRealUB());
	}
	else
	{
		ShaderState.SetUniformBufferConstantData(PendingState, Stage,
			BindingIndex,
			UniformBuffer->ConstantData);
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader.CodeHeader.SerializedBindings.ShaderResourceTable;

	// Gather texture bindings from the SRT table
	static TArray<FSrtResourceBinding> TextureBindings;
	TextureBindings.Reset();
	GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, TextureBindings);

	// Gather Sampler bindings from the SRT table
	static TArray<FSrtResourceBinding> SamplerBindings;
	SamplerBindings.Reset();
	GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, SamplerBindings);

	// The amount of samplers and textures should be proportional
	check(SamplerBindings.Num() == TextureBindings.Num());

	for(int32 Index = 0; Index < TextureBindings.Num(); Index++)
	{
		const FSrtResourceBinding& CurrTextureBinding = TextureBindings[Index];
		const FSrtResourceBinding& CurrSamplerBinding = SamplerBindings[Index];

		// Sampler and binding index is expected to be the same, since we are constructing
		// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER object, which has sampler and a texture/image
		check(CurrSamplerBinding.BindingIndex == CurrTextureBinding.BindingIndex);

		// Set Texture
		{
			FTextureRHIParamRef TexRef = (FTextureRHIParamRef)CurrTextureBinding.Resource.GetReference();
			const FVulkanTextureBase* BaseTexture = FVulkanTextureBase::Cast(TexRef);
			if(BaseTexture)
			{
				// Do the binding...
				ShaderState.SetTexture(Stage, CurrTextureBinding.BindingIndex, BaseTexture);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid texture in SRT table for shader '%s'"), *Shader.DebugName);
			}
		}

		// Set Sampler
		{
			FVulkanSamplerState* CurrSampler = static_cast<FVulkanSamplerState*>(CurrSamplerBinding.Resource.GetReference());
			check(CurrSampler);

			if(CurrSampler)
			{
				// Do the binding...
				ShaderState.SetSamplerState(Stage, CurrSamplerBinding.BindingIndex, CurrSampler);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid sampler in SRT table for shader '%s'"), *Shader.DebugName);
			}
		}
	}
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	// Validate if the shader which we are setting is the same as we expect
	check(&Device->GetPendingState().GetBoundShaderState().GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);

	SetShaderUniformBuffer(SF_Vertex, UniformBuffer, BufferIndex);
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	// Validate if the shader which we are setting is the same as we expect
	check(&Device->GetPendingState().GetBoundShaderState().GetShader(SF_Pixel) == ResourceCast(PixelShader));

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);

	SetShaderUniformBuffer(SF_Pixel, UniformBuffer, BufferIndex);
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	FVulkanDepthStencilState* NewState = ResourceCast(NewStateRHI);
	check(NewState);

	check(Device);
	Device->GetPendingState().SetDepthStencilState(NewState);
}

void FVulkanCommandListContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	FVulkanBlendState* NewState = ResourceCast(NewStateRHI);
	check(NewState);

	check(Device);
	Device->GetPendingState().SetBlendState(NewState);
}


// Occlusion/Timer queries.
void FVulkanCommandListContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
#if 0
	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);

	Query->Begin();
#endif
}

void FVulkanCommandListContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
#if 0
	FVulkanRenderQuery* Query = ResourceCast(QueryRHI);

	Query->End();
#endif
}


void FVulkanCommandListContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	check(Device);

	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	FVulkanPendingState& State = Device->GetPendingState();
	FVulkanBoundShaderState& BSS = State.GetBoundShaderState();

	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

#if VULKAN_HAS_DEBUGGING_ENABLED
	if (!BSS.HasError())
#endif
	{
		State.PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));

		vkCmdDraw(Device->GetPendingState().GetCommandBuffer(), NumVertices, NumInstances, BaseVertexIndex, 0);
	}

	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	check(Device);

	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	//@NOTE: don't prepare draw without actually drawing
#if !PLATFORM_ANDROID
	Device->GetPendingState().PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
#endif
	
	VULKAN_SIGNAL_UNIMPLEMENTED();

	//if (IsImmediate())
	//{
	//	VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(0);
	//}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	check(Device);

	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	FVulkanPendingState& State = Device->GetPendingState();
	FVulkanBoundShaderState& BSS = State.GetBoundShaderState();

	#if VULKAN_HAS_DEBUGGING_ENABLED
		if(!BSS.HasError())
	#endif
	{
		State.PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));

		FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		vkCmdBindIndexBuffer(State.GetCommandBuffer(),
			IndexBuffer->GetBuffer()->GetBufferHandle(), IndexBuffer->GetOffset(), IndexBuffer->IndexType);

		uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
		vkCmdDrawIndexed(Device->GetPendingState().GetCommandBuffer(), NumIndices, NumInstances, StartIndex, 0, FirstInstance);
	}

	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	check(Device);

	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	//@NOTE: don't prepare draw without actually drawing
#if !PLATFORM_ANDROID
	Device->GetPendingState().PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
#if 0
	FVulkanStructuredBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);
#endif
#endif
	VULKAN_SIGNAL_UNIMPLEMENTED();

	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(0);
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	check(Device);

	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	//@NOTE: don't prepare draw without actually drawing
#if !PLATFORM_ANDROID
	Device->GetPendingState().PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
#if 0
	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
#endif
#endif
	VULKAN_SIGNAL_UNIMPLEMENTED();

	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(0);
	}
}

void FVulkanCommandListContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);

//	checkSlow(GPendingDrawPrimitiveUPVertexData == nullptr);

	PendingDrawPrimitiveUPVertexData = DynamicVB->Lock(VertexDataStride * NumVertices);
	OutVertexData = PendingDrawPrimitiveUPVertexData.Data;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}


void FVulkanCommandListContext::RHIEndDrawPrimitiveUP()
{
	int32 UPBufferIndex = GFrameNumberRenderThread % 3;
	DynamicVB->Unlock(PendingDrawPrimitiveUPVertexData);

	//if (!DrawingViewport)
	//{
	//	return;
	//}

	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	FVulkanPendingState& State = Device->GetPendingState();
	FVulkanBoundShaderState& Shader = State.GetBoundShaderState();

	State.SetStreamSource(0, PendingDrawPrimitiveUPVertexData.Buffer, PendingVertexDataStride, PendingDrawPrimitiveUPVertexData.Offset);
	
	#if VULKAN_HAS_DEBUGGING_ENABLED
		if(!Shader.HasError())
	#endif
	{
		State.PrepareDraw(UEToVulkanType((EPrimitiveType)PendingPrimitiveType));

		vkCmdDraw(State.GetCommandBuffer(), PendingNumVertices, 1, PendingMinVertexIndex, 0);
	}

	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}
}

void FVulkanCommandListContext::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride,
	void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);

	PendingDrawPrimitiveUPVertexData = DynamicVB->Lock(VertexDataStride * NumVertices);
	OutVertexData = PendingDrawPrimitiveUPVertexData.Data;

	check(IndexDataStride == 2 || IndexDataStride == 4);
	PendingPrimitiveIndexType = IndexDataStride == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	PendingDrawPrimitiveUPIndexData = DynamicIB->Lock(IndexDataStride * NumIndices);
	OutIndexData = PendingDrawPrimitiveUPIndexData.Data;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;

	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}

void FVulkanCommandListContext::RHIEndDrawIndexedPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	DynamicIB->Unlock(PendingDrawPrimitiveUPIndexData);
	DynamicVB->Unlock(PendingDrawPrimitiveUPVertexData);

	FVulkanPendingState& State = Device->GetPendingState();
	FVulkanBoundShaderState& Shader = State.GetBoundShaderState();

	State.SetStreamSource(0, PendingDrawPrimitiveUPVertexData.Buffer, PendingVertexDataStride, PendingDrawPrimitiveUPVertexData.Offset);

	#if VULKAN_HAS_DEBUGGING_ENABLED
		if(!Shader.HasError())
	#endif
	{
		State.PrepareDraw(UEToVulkanType((EPrimitiveType)PendingPrimitiveType));
		uint32 NumIndices = GetVertexCountForPrimitiveCount(PendingNumPrimitives, PendingPrimitiveType);
		vkCmdBindIndexBuffer(State.GetCommandBuffer(), PendingDrawPrimitiveUPIndexData.Buffer->GetBufferHandle(), PendingDrawPrimitiveUPIndexData.Offset, PendingPrimitiveIndexType);

		vkCmdDrawIndexed(State.GetCommandBuffer(), NumIndices, 1, PendingMinVertexIndex, 0, 0);
	}

	//if (IsImmediate())
	{
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}
}

void FVulkanCommandListContext::RHIClear(bool bClearColor,const FLinearColor& Color, bool bClearDepth,float Depth, bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	check(Device);
	FVulkanPendingState& state = Device->GetPendingState();
	const uint32 NumColorAttachments = state.GetFrameBuffer()->GetNumColorAttachments();
	FVulkanCommandListContext::RHIClearMRT(bClearColor, NumColorAttachments, &Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FVulkanCommandListContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
{
	check(Device);

	if (!(bClearColor || bClearDepth || bClearStencil))
	{
		return;
	}

	check(bClearColor ? NumClearColors > 0 : true);

	FVulkanPendingState& State = Device->GetPendingState();
	State.UpdateRenderPass();
	
#if VULKAN_ALLOW_MIDPASS_CLEAR
	VkClearRect Rect;
	FMemory::Memzero(Rect);
	Rect.rect.offset.x = 0;
	Rect.rect.offset.y = 0;
	Rect.rect.extent = State.GetRenderPass().GetLayout().GetExtent2D();

	VkClearAttachment Attachement;
	FMemory::Memzero(Attachement);

	if(bClearColor)
	{
		for (int32 i = 0; i < NumClearColors; ++i)
		{
			Attachement.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			Attachement.colorAttachment = i;
			Attachement.clearValue.color.float32[0] = ClearColorArray[i].R;
			Attachement.clearValue.color.float32[1] = ClearColorArray[i].G;
			Attachement.clearValue.color.float32[2] = ClearColorArray[i].B;
			Attachement.clearValue.color.float32[3] = ClearColorArray[i].A;
			vkCmdClearAttachments(State.GetCommandBuffer(), 1, &Attachement, 1, &Rect);
		}
	}

	if(bClearDepth || bClearStencil)
	{
		Attachement.aspectMask = 0;
		Attachement.aspectMask |= bClearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
		Attachement.aspectMask |= bClearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
		Attachement.colorAttachment = NumClearColors;	// Depth attachment is always stored after the color attachment
		Attachement.clearValue.depthStencil.depth = Depth;
		Attachement.clearValue.depthStencil.stencil = Stencil;
		vkCmdClearAttachments(State.GetCommandBuffer(), 1, &Attachement, 1, &Rect);
	}
#endif
}

void FVulkanDynamicRHI::RHISuspendRendering()
{
}

void FVulkanDynamicRHI::RHIResumeRendering()
{
}

bool FVulkanDynamicRHI::RHIIsRenderingSuspended()
{
	return false;
}

void FVulkanDynamicRHI::RHIBlockUntilGPUIdle()
{
	Device->WaitUntilIdle();
}

uint32 FVulkanDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FVulkanCommandListContext::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIFlushComputeShaderCache()
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIBeginAsyncComputeJob_DrawThread(EAsyncComputePriority Priority)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIEndAsyncComputeJob_DrawThread(uint32 FenceIndex)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIGraphicsWaitOnAsyncComputeJob(uint32 FenceIndex)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}
