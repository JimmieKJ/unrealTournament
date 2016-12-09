// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommands.cpp: Vulkan RHI commands implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "EngineGlobals.h"

//#todo-rco: One of this per Context!
static TGlobalResource< TBoundShaderStateHistory<10000, false> > GBoundShaderStateHistory;

static TAutoConsoleVariable<int32> GCVarSubmitOnDispatch(
	TEXT("r.Vulkan.SubmitOnDispatch"),
	0,
	TEXT("0 to not do anything special on dispatch(default)\n")\
	TEXT("1 to submit the cmd buffer after each dispatch"),
	ECVF_RenderThreadSafe
);

static inline bool UseRealUBs()
{
	static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
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

void FVulkanCommandListContext::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint32 Offset)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if (VertexBuffer != NULL)
	{
		PendingGfxState->SetStreamSource(StreamIndex, VertexBuffer, Stride, Offset + VertexBuffer->GetOffset());
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

	PendingGfxState->SetRasterizerState(NewState);
}

void FVulkanCommandListContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
		ensure(0);
	}
	FVulkanComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	PendingComputeState->SetComputeShader(ComputeShader);
}

void FVulkanCommandListContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallTime);

	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	ensure(Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingComputeState->PrepareDispatch(this, Cmd);
	VulkanRHI::vkCmdDispatch(CmdBuffer, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	if (GCVarSubmitOnDispatch.GetValueOnRenderThread())
	{
		InternalSubmitActiveCmdBuffer();
	}

	// flush any needed buffers that the compute shader wrote to	
	if (bAutomaticFlushAfterComputeShader)
	{
		FlushAfterComputeShader();
	}

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(1);
#endif
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
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(1);
#endif
	}
}

void FVulkanCommandListContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	TRefCountPtr<FVulkanBoundShaderState> BoundShaderState = ResourceCast(BoundShaderStateRHI);

	// Store in the history so it doesn't get released
	GBoundShaderStateHistory.Add(BoundShaderState);

	PendingGfxState->SetBoundShaderState(BoundShaderState);
}


void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	PendingComputeState->CurrentState.CSS->SetUAV(UAVIndex, UAV);
	if (bAutomaticFlushAfterComputeShader && UAV)
	{
		PendingComputeState->CurrentState.UAVListForAutoFlush.Add(UAV);
	}
}

void FVulkanCommandListContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	FVulkanUnorderedAccessView* UAV = ResourceCast(UAVRHI);
	ensure(0);
}


void FVulkanCommandListContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FVulkanBoundShaderState& BSS = PendingGfxState->GetBoundShaderState();

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
	FVulkanBoundShaderState& BSS = PendingGfxState->GetBoundShaderState();

	// Verify presence of a vertex shader and it has to be the same shader
	check(ResourceCast(GeometryShader) == &BSS.GetShader(SF_Geometry));

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	BSS.SetTexture(SF_Geometry, TextureIndex, VulkanTexture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FVulkanBoundShaderState& BSS = PendingGfxState->GetBoundShaderState();

	// Verify presence of a pixel shader and it has to be the same shader
	check(ResourceCast(PixelShaderRHI) == &BSS.GetShader(SF_Pixel));

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	BSS.SetTexture(SF_Pixel, TextureIndex, VulkanTexture);
}

void FVulkanCommandListContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	FVulkanComputePipelineState& State = PendingComputeState->CurrentState;

	check(ResourceCast(ComputeShader) == State.CSS->ComputeShader);

	FVulkanTextureBase* VulkanTexture = GetVulkanTextureFromRHITexture(NewTextureRHI);
	State.CSS->SetTexture(TextureIndex, VulkanTexture);
}


void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();
	ShaderState.SetSRV(SF_Vertex, TextureIndex, ResourceCast(SRVRHI));
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
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();
	ShaderState.SetSRV(SF_Geometry, TextureIndex, ResourceCast(SRVRHI));
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();
	ShaderState.SetSRV(SF_Pixel, TextureIndex, ResourceCast(SRVRHI));
}

void FVulkanCommandListContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	FVulkanComputePipelineState& State = PendingComputeState->CurrentState;
	check(State.CSS->DoesShaderMatch(ResourceCast(ComputeShaderRHI)));
	State.CSS->SetSRV(TextureIndex, ResourceCast(SRVRHI));
}

void FVulkanCommandListContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

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
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

	check(&ShaderState.GetShader(SF_Geometry) == ResourceCast(GeometryShader));
	FVulkanSamplerState* Sampler = ResourceCast(NewStateRHI);
	ShaderState.SetSamplerState(SF_Geometry, SamplerIndex, Sampler);
}

void FVulkanCommandListContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

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
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

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
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

	// Verify shader
	check(&ShaderState.GetShader(SF_Geometry) == ResourceCast(GeometryShaderRHI));
	ShaderState.SetShaderParameter(SF_Geometry, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

	// Verify shader
	check(&ShaderState.GetShader(SF_Pixel) == ResourceCast(PixelShaderRHI));
	ShaderState.SetShaderParameter(SF_Pixel, BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FVulkanCommandListContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	FVulkanComputePipelineState& ShaderState = PendingComputeState->CurrentState;

	check(ShaderState.CSS->ComputeShader.GetReference() == ResourceCast(ComputeShaderRHI));
	ShaderState.CSS->SetShaderParameter(BufferIndex, BaseIndex, NumBytes, NewValue);
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

	FVulkanBoundShaderState& ShaderState = PendingGfxState->GetBoundShaderState();

	// Walk through all resources to set all appropriate states
	const FVulkanShader& Shader = ShaderState.GetShader(Stage);

	// Uniform Buffers
	if (UseRealUBs())
	{
		ShaderState.SetUniformBuffer(Stage, BindingIndex, UniformBuffer);
	}
	else
	{
		ShaderState.SetUniformBufferConstantData(Stage, BindingIndex, UniformBuffer->ConstantData);
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader.CodeHeader.SerializedBindings.ShaderResourceTable;
	if (ResourceBindingTable.ResourceTableLayoutHashes.Num() == 0)
	{
		return;
	}

	//#todo-rco: Quite slow...
	// Gather texture bindings from the SRT table
	TArray<FSrtResourceBinding> TextureBindings;
	GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, TextureBindings);

	// Gather Sampler bindings from the SRT table
	TArray<FSrtResourceBinding> SamplerBindings;
	GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BindingIndex, SamplerBindings);

	//#todo-rco: Separate samplers & textures
	// The amount of samplers and textures should be proportional
	check(SamplerBindings.Num() >= TextureBindings.Num());

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
	check(&PendingGfxState->GetBoundShaderState().GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));

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
	// Validate if the shader which we are setting is the same as we expect
	check(&PendingGfxState->GetBoundShaderState().GetShader(SF_Geometry) == ResourceCast(GeometryShader));

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);

	SetShaderUniformBuffer(SF_Geometry, UniformBuffer, BufferIndex);
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	// Validate if the shader which we are setting is the same as we expect
	check(&PendingGfxState->GetBoundShaderState().GetShader(SF_Pixel) == ResourceCast(PixelShader));

	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);

	SetShaderUniformBuffer(SF_Pixel, UniformBuffer, BufferIndex);
}

void FVulkanCommandListContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetUniformBufferTime);
#endif
	FVulkanComputePipelineState& State = PendingComputeState->CurrentState;

	// Walk through all resources to set all appropriate states
	FVulkanComputeShader* Shader = ResourceCast(ComputeShader);

	check(State.CSS->DoesShaderMatch(Shader));
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(BufferRHI);
	
	// Uniform Buffers
	if (UseRealUBs())
	{
		State.CSS->SetUniformBuffer(BufferIndex, UniformBuffer);
	}
	else
	{
		State.CSS->SetUniformBufferConstantData(BufferIndex, UniformBuffer->ConstantData);
	}

	const FShaderCompilerResourceTable& ResourceBindingTable = Shader->CodeHeader.SerializedBindings.ShaderResourceTable;
	if (ResourceBindingTable.ResourceTableLayoutHashes.Num() == 0)
	{
		return;
	}

	//#todo-rco: Quite slow...
	// Gather texture bindings from the SRT table
	TArray<FSrtResourceBinding> TextureBindings;
	GatherUniformBufferResources(ResourceBindingTable.TextureMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, TextureBindings);

	// Gather Sampler bindings from the SRT table
	TArray<FSrtResourceBinding> SamplerBindings;
	GatherUniformBufferResources(ResourceBindingTable.SamplerMap, ResourceBindingTable.ResourceTableBits, UniformBuffer, BufferIndex, SamplerBindings);

	//#todo-rco: Separate samplers & textures
	// The amount of samplers and textures should be proportional
	check(SamplerBindings.Num() >= TextureBindings.Num());

	for (int32 Index = 0; Index < TextureBindings.Num(); Index++)
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
			if (BaseTexture)
			{
				// Do the binding...
				State.CSS->SetTexture(CurrTextureBinding.BindingIndex, BaseTexture);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid texture in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}

		// Set Sampler
		{
			FVulkanSamplerState* CurrSampler = static_cast<FVulkanSamplerState*>(CurrSamplerBinding.Resource.GetReference());
			check(CurrSampler);

			if (CurrSampler)
			{
				// Do the binding...
				State.CSS->SetSamplerState(CurrSamplerBinding.BindingIndex, CurrSampler);
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Invalid sampler in SRT table for shader '%s'"), *Shader->DebugName);
			}
		}
	}
}

void FVulkanCommandListContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	FVulkanDepthStencilState* NewState = ResourceCast(NewStateRHI);
	check(NewState);

	PendingGfxState->SetDepthStencilState(NewState, StencilRef);
}

void FVulkanCommandListContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	FVulkanBlendState* NewState = ResourceCast(NewStateRHI);
	check(NewState);

	PendingGfxState->SetBlendState(NewState);
}

void FVulkanCommandListContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	FVulkanBoundShaderState& BSS = PendingGfxState->GetBoundShaderState();

	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareDraw(this, CmdBuffer, UEToVulkanType((EPrimitiveType)PrimitiveType));
	NumInstances = FMath::Max(1U, NumInstances);
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), NumVertices, NumInstances, BaseVertexIndex, 0);

	//if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
#endif
	}
}

void FVulkanCommandListContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	//@NOTE: don't prepare draw without actually drawing
#if 0//!PLATFORM_ANDROID
	PendingGfxState->PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
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
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	FVulkanBoundShaderState& BSS = PendingGfxState->GetBoundShaderState();

	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	FVulkanCmdBuffer* Cmd = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();
	PendingGfxState->PrepareDraw(this, Cmd, UEToVulkanType((EPrimitiveType)PrimitiveType));

	VulkanRHI::vkCmdBindIndexBuffer(CmdBuffer, IndexBuffer->GetHandle(), IndexBuffer->GetOffset(), IndexBuffer->GetIndexType());

	uint32 NumIndices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	NumInstances = FMath::Max(1U, NumInstances);
	VulkanRHI::vkCmdDrawIndexed(CmdBuffer, NumIndices, NumInstances, StartIndex, BaseVertexIndex, FirstInstance);

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
#endif
	}
}

void FVulkanCommandListContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

#if 0
	//@NOTE: don't prepare draw without actually drawing
#if !PLATFORM_ANDROID
	PendingState->PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
#if 0
	FVulkanStructuredBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);
#endif
#endif
#endif
	VULKAN_SIGNAL_UNIMPLEMENTED();

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(0);
#endif
	}
}

void FVulkanCommandListContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);
#if 0
	//@NOTE: don't prepare draw without actually drawing
#if !PLATFORM_ANDROID
	PendingState->PrepareDraw(UEToVulkanType((EPrimitiveType)PrimitiveType));
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
#if 0
	FVulkanVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
#endif
#endif
#endif
	VULKAN_SIGNAL_UNIMPLEMENTED();

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(0);
#endif
	}
}

void FVulkanCommandListContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);

//	checkSlow(GPendingDrawPrimitiveUPVertexData == nullptr);

	bool bAllocatedVB = TempFrameAllocationBuffer.Alloc(VertexDataStride * NumVertices, VertexDataStride, PendingDrawPrimUPVertexAllocInfo);
	check(bAllocatedVB);
	OutVertexData = PendingDrawPrimUPVertexAllocInfo.Data;

	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;
}


void FVulkanCommandListContext::RHIEndDrawPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallTime);

	FVulkanBoundShaderState& Shader = PendingGfxState->GetBoundShaderState();

	PendingGfxState->SetStreamSource(0, PendingDrawPrimUPVertexAllocInfo.GetHandle(), PendingVertexDataStride, PendingDrawPrimUPVertexAllocInfo.GetBindOffset());
	
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	PendingGfxState->PrepareDraw(this, CmdBuffer, UEToVulkanType((EPrimitiveType)PendingPrimitiveType));
	VulkanRHI::vkCmdDraw(CmdBuffer->GetHandle(), PendingNumVertices, 1, PendingMinVertexIndex, 0);

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
#endif
	}
}

void FVulkanCommandListContext::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride,
	void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUPPrepTime);

	bool bAllocatedVB = TempFrameAllocationBuffer.Alloc(VertexDataStride * NumVertices, IndexDataStride, PendingDrawPrimUPVertexAllocInfo);
	check(bAllocatedVB);
	OutVertexData = PendingDrawPrimUPVertexAllocInfo.Data;

	check(IndexDataStride == 2 || IndexDataStride == 4);
	PendingPrimitiveIndexType = IndexDataStride == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
	bool bAllocatedIB = TempFrameAllocationBuffer.Alloc(IndexDataStride * NumIndices, IndexDataStride, PendingDrawPrimUPIndexAllocInfo);
	check(bAllocatedIB);
	OutIndexData = PendingDrawPrimUPIndexAllocInfo.Data;

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

	FVulkanBoundShaderState& Shader = PendingGfxState->GetBoundShaderState();

	PendingGfxState->SetStreamSource(0, PendingDrawPrimUPVertexAllocInfo.GetHandle(), PendingVertexDataStride, PendingDrawPrimUPVertexAllocInfo.GetBindOffset());

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	VkCommandBuffer Cmd = CmdBuffer->GetHandle();
	PendingGfxState->PrepareDraw(this, CmdBuffer, UEToVulkanType((EPrimitiveType)PendingPrimitiveType));
	uint32 NumIndices = GetVertexCountForPrimitiveCount(PendingNumPrimitives, PendingPrimitiveType);
	VulkanRHI::vkCmdBindIndexBuffer(Cmd, PendingDrawPrimUPIndexAllocInfo.GetHandle(), PendingDrawPrimUPIndexAllocInfo.GetBindOffset(), PendingPrimitiveIndexType);
	VulkanRHI::vkCmdDrawIndexed(Cmd, NumIndices, 1, PendingMinVertexIndex, 0, 0);

	if (IsImmediate())
	{
#if 0
		VulkanRHI::GManager.GPUProfilingData.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
#endif
	}
}

void FVulkanCommandListContext::RHIClear(bool bClearColor,const FLinearColor& Color, bool bClearDepth,float Depth, bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	if (!(bClearColor || bClearDepth || bClearStencil))
	{
		return;
	}
	//FRCLog::Printf(TEXT("RHIClear"));
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	const uint32 NumColorAttachments = bClearColor ? TransitionState.CurrentFramebuffer->GetNumColorAttachments() : 0;

	FVulkanCommandListContext::InternalClearMRT(CmdBuffer, bClearColor, NumColorAttachments, &Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FVulkanCommandListContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
{
	if (!(bClearColor || bClearDepth || bClearStencil))
	{
		return;
	}

	check(bClearColor ? NumClearColors > 0 : true);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	//FRCLog::Printf(TEXT("RHIClearMRT"));

	const uint32 NumColorAttachments = TransitionState.CurrentFramebuffer->GetNumColorAttachments();
	check(!bClearColor || (uint32)NumClearColors <= NumColorAttachments);
	InternalClearMRT(CmdBuffer, bClearColor, bClearColor ? NumClearColors : 0, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FVulkanCommandListContext::InternalClearMRT(FVulkanCmdBuffer* CmdBuffer, bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
{
	const VkExtent2D& Extents = TransitionState.CurrentRenderPass->GetLayout().GetExtent2D();
	if (ExcludeRect.Min.X == 0 && ExcludeRect.Width() == Extents.width && ExcludeRect.Min.Y == 0 && Extents.height)
	{
		//if (ForceFullScreen == EForceFullScreenClear::EDoNotForce)
		{
			return;
		}
		//else
		{
			//ensureMsgf(false, TEXT("Forced Full Screen Clear ignoring Exclude Rect Restriction"));
		}
	}

	ensure(ExcludeRect.Area() == 0);

	if (TransitionState.CurrentRenderPass)
	{
		VkClearRect Rect;
		FMemory::Memzero(Rect);
		Rect.rect.offset.x = 0;
		Rect.rect.offset.y = 0;
		Rect.rect.extent = Extents;

		VkClearAttachment Attachments[MaxSimultaneousRenderTargets + 1];
		FMemory::Memzero(Attachments);

		uint32 NumAttachments = NumClearColors;
		if (bClearColor)
		{
			for (int32 i = 0; i < NumClearColors; ++i)
			{
				Attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				Attachments[i].colorAttachment = i;
				Attachments[i].clearValue.color.float32[0] = ClearColorArray[i].R;
				Attachments[i].clearValue.color.float32[1] = ClearColorArray[i].G;
				Attachments[i].clearValue.color.float32[2] = ClearColorArray[i].B;
				Attachments[i].clearValue.color.float32[3] = ClearColorArray[i].A;
			}
		}

		if (bClearDepth || bClearStencil)
		{
			Attachments[NumClearColors].aspectMask = bClearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
			Attachments[NumClearColors].aspectMask |= bClearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
			Attachments[NumClearColors].colorAttachment = 0;
			Attachments[NumClearColors].clearValue.depthStencil.depth = Depth;
			Attachments[NumClearColors].clearValue.depthStencil.stencil = Stencil;
			++NumAttachments;
		}

		VulkanRHI::vkCmdClearAttachments(CmdBuffer->GetHandle(), NumAttachments, Attachments, 1, &Rect);
	}
	else
	{
		ensure(0);
		//VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), )
	}
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
	bAutomaticFlushAfterComputeShader = bEnable;
}

void FVulkanCommandListContext::RHIFlushComputeShaderCache()
{
	//#todo-rco: Flush
}

void FVulkanDynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RequestSubmitCurrentCommands()
{
	ensure(IsImmediate());
	bSubmitAtNextSafePoint = true;
}

void FVulkanCommandListContext::InternalSubmitActiveCmdBuffer()
{
	CommandBufferManager->SubmitActiveCmdBuffer(false);
	CommandBufferManager->PrepareForNewActiveCommandBuffer();
}

void FVulkanCommandListContext::PrepareForCPURead()
{
	ensure(IsImmediate());
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun())
	{
		ensure(CmdBuffer->IsOutsideRenderPass());
		CommandBufferManager->SubmitActiveCmdBuffer(true);
	}
}

void FVulkanCommandListContext::RHISubmitCommandsHint()
{
	RequestSubmitCurrentCommands();
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer && CmdBuffer->HasBegun() && CmdBuffer->IsOutsideRenderPass())
	{
		SafePointSubmit();
	}
	CommandBufferManager->RefreshFenceStatus();
}

void FVulkanCommandListContext::FlushAfterComputeShader()
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	int32 NumResourcesToFlush = PendingComputeState->CurrentState.UAVListForAutoFlush.Num();
	if (NumResourcesToFlush > 0)
	{
		TArray<VkImageMemoryBarrier> ImageBarriers;
		TArray<VkBufferMemoryBarrier> BufferBarriers;
		for (FVulkanUnorderedAccessView* UAV : PendingComputeState->CurrentState.UAVListForAutoFlush)
		{
			if (UAV->SourceVertexBuffer)
			{
				VkBufferMemoryBarrier Barrier;
				VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, UAV->SourceVertexBuffer->GetHandle(), UAV->SourceVertexBuffer->GetOffset(), UAV->SourceVertexBuffer->GetSize());
				BufferBarriers.Add(Barrier);
			}
			else if (UAV->SourceTexture)
			{
				FVulkanTextureBase* Texture = (FVulkanTextureBase*)UAV->SourceTexture->GetTextureBaseRHI();
				VkImageMemoryBarrier Barrier;
				VkImageLayout Layout = TransitionState.FindOrAddLayout(Texture->Surface.Image, VK_IMAGE_LAYOUT_GENERAL);
				VulkanRHI::SetupAndZeroImageBarrier(Barrier, Texture->Surface, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, Layout, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, Layout);
				ImageBarriers.Add(Barrier);
			}
			else
			{
				ensure(0);
			}
		}
		VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
		PendingComputeState->CurrentState.UAVListForAutoFlush.SetNum(0, false);
	}
}
