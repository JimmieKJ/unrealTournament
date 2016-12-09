// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanShaders.cpp: Vulkan shader RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "GlobalShader.h"
#include "Serialization/MemoryReader.h"

#if (!PLATFORM_ANDROID)
#include <vulkan/icd-spv.h>
#endif

static TAutoConsoleVariable<int32> GStripGLSL(
	TEXT("r.Vulkan.StripGlsl"),
	1,
	TEXT("1 to remove glsl source (default)\n")\
	TEXT("0 to keep glsl source in each shader for debugging"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);


void FVulkanShader::Create(EShaderFrequency Frequency, const TArray<uint8>& InShaderCode)
{
	check(Device);

	FMemoryReader Ar(InShaderCode, true);

	Ar << CodeHeader;

	TArray<ANSICHAR> DebugNameArray;
	Ar << DebugNameArray;
	DebugName = ANSI_TO_TCHAR(DebugNameArray.GetData());

	TArray<uint8> Spirv;
	Ar << Spirv;

	Ar << GlslSource;
	if (GStripGLSL.GetValueOnAnyThread() == 1)
	{
		GlslSource.Empty(0);
	}

	int32 CodeOffset = Ar.Tell();

	VkShaderModuleCreateInfo ModuleCreateInfo;
	FMemory::Memzero(ModuleCreateInfo);
	ModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ModuleCreateInfo.pNext = nullptr;
	ModuleCreateInfo.flags = 0;

	check(Code == nullptr)
	ModuleCreateInfo.codeSize = Spirv.Num();
	Code = (uint32*)FMemory::Malloc(ModuleCreateInfo.codeSize);
	FMemory::Memcpy(Code, Spirv.GetData(), ModuleCreateInfo.codeSize);

	check(Code != nullptr);
	CodeSize = ModuleCreateInfo.codeSize;
	ModuleCreateInfo.pCode = Code;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, nullptr, &ShaderModule));

	if (Frequency == SF_Compute && CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num() == 0)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Compute shader %s %s has no resources!"), *CodeHeader.ShaderName, *DebugName);
	}
}

#if VULKAN_HAS_DEBUGGING_ENABLED
inline void ValidateBindingPoint(const FVulkanShader& InShader, const uint32 InBindingPoint, const uint8 InSubType)
{
#if 0
	const TArray<FVulkanShaderSerializedBindings::FBindMap>& BindingLayout = InShader.CodeHeader.SerializedBindings.Bindings;
	bool bFound = false;

	for (const auto& Binding : BindingLayout)
	{
		const bool bIsPackedUniform = InSubType == CrossCompiler::PACKED_TYPENAME_HIGHP
			|| InSubType == CrossCompiler::PACKED_TYPENAME_MEDIUMP
			|| InSubType == CrossCompiler::PACKED_TYPENAME_LOWP;

		if (Binding.EngineBindingIndex == InBindingPoint &&
			bIsPackedUniform ? (Binding.Type == EVulkanBindingType::PACKED_UNIFORM_BUFFER) : (Binding.SubType == InSubType)
			)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		FString SubTypeName = "UNDEFINED";

		switch (InSubType)
		{
		case CrossCompiler::PACKED_TYPENAME_HIGHP: SubTypeName = "HIGH PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_MEDIUMP: SubTypeName = "MEDIUM PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_LOWP: SubTypeName = "LOW PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_INT: SubTypeName = "INT UNIFORM PACKED BUFFER";				break;
		case CrossCompiler::PACKED_TYPENAME_UINT: SubTypeName = "UINT UNIFORM PACKED BUFFER";				break;
		case CrossCompiler::PACKED_TYPENAME_SAMPLER: SubTypeName = "SAMPLER";								break;
		case CrossCompiler::PACKED_TYPENAME_IMAGE: SubTypeName = "IMAGE";									break;
		default:
			break;
		}

		UE_LOG(LogVulkanRHI, Warning,
			TEXT("Setting '%s' resource for an unexpected binding slot UE:%d, for shader '%s'"),
			*SubTypeName, InBindingPoint, *InShader.DebugName);
	}
#endif
}
#endif // VULKAN_HAS_DEBUGGING_ENABLED

template<typename BaseResourceType, EShaderFrequency Frequency>
void TVulkanBaseShader<BaseResourceType, Frequency>::Create(const TArray<uint8>& InCode)
{
	FVulkanShader::Create(Frequency, InCode);
}


FVulkanShader::~FVulkanShader()
{
	check(Device);

	if (Code)
	{
		FMemory::Free(Code);
		Code = nullptr;
	}

	if (ShaderModule != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ShaderModule, ShaderModule);
		ShaderModule = VK_NULL_HANDLE;
	}
}

FVertexShaderRHIRef FVulkanDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	FVulkanVertexShader* Shader = new FVulkanVertexShader(Device);
	Shader->Create(Code);
	return Shader;
}

FPixelShaderRHIRef FVulkanDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	FVulkanPixelShader* Shader = new FVulkanPixelShader(Device);
	Shader->Create(Code);
	return Shader;
}

FHullShaderRHIRef FVulkanDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{ 
	FVulkanHullShader* Shader = new FVulkanHullShader(Device);
	Shader->Create(Code);
	return Shader;
}

FDomainShaderRHIRef FVulkanDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	FVulkanDomainShader* Shader = new FVulkanDomainShader(Device);
	Shader->Create(Code);
	return Shader;
}

FGeometryShaderRHIRef FVulkanDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{ 
	FVulkanGeometryShader* Shader = new FVulkanGeometryShader(Device);
	Shader->Create(Code);
	return Shader;
}

FGeometryShaderRHIRef FVulkanDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
}

FComputeShaderRHIRef FVulkanDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{ 
	FVulkanComputeShader* Shader = new FVulkanComputeShader(Device);
	Shader->Create(Code);
	return Shader;
}

FVulkanShaderState::FVulkanShaderState(FVulkanDevice* InDevice)
	: Device(InDevice)
	, PipelineLayout(VK_NULL_HANDLE)
	, Layout(nullptr)
	, CurrDescriptorSets(nullptr)
	, LastBoundPipeline(VK_NULL_HANDLE)
{
	Layout = new FVulkanDescriptorSetsLayout(Device);
}

FVulkanShaderState::~FVulkanShaderState()
{
	check(Layout);
	delete Layout;

	if (PipelineLayout != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::PipelineLayout, PipelineLayout);
		PipelineLayout = VK_NULL_HANDLE;
	}
}

inline void FVulkanShaderState::GenerateLayoutBindingsForStage(VkShaderStageFlagBits StageFlags, EDescriptorSetStage DescSet, const FVulkanCodeHeader& CodeHeader, FVulkanDescriptorSetsLayout* OutLayout)
{
	//#todo-rco: Mobile assumption!
	int32 DescriptorSetIndex = (int32)DescSet;

	VkDescriptorSetLayoutBinding Binding;
	FMemory::Memzero(Binding);
	Binding.descriptorCount = 1;
	Binding.stageFlags = StageFlags;
	for (int32 Index = 0; Index < CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num(); ++Index)
	{
		Binding.binding = Index;
		Binding.descriptorType = CodeHeader.NEWDescriptorInfo.DescriptorTypes[Index];
		OutLayout->AddDescriptor(DescriptorSetIndex, Binding, Index);
	}
}


FVulkanComputeShaderState::FVulkanComputeShaderState(FVulkanDevice* InDevice, FComputeShaderRHIParamRef InComputeShader)
	: FVulkanShaderState(InDevice)
{
	ComputeShader = ResourceCast(InComputeShader);

	NEWPackedUniformBufferStagingDirty = 0;

	check(InComputeShader);
#if VULKAN_ENABLE_PIPELINE_CACHE
	ShaderHash = ComputeShader->CodeHeader.SourceHash;
#endif

	InitGlobalUniformsForStage(ComputeShader->CodeHeader, NEWPackedUniformBufferStaging, NEWPackedUniformBufferStagingDirty);
	GenerateLayoutBindingsForStage(VK_SHADER_STAGE_COMPUTE_BIT, EDescriptorSetStage::Compute, ComputeShader->CodeHeader, Layout);

	Layout->Compile();

	CreateDescriptorWriteInfos();
}

void FVulkanComputeShaderState::CreateDescriptorWriteInfos()
{
	check(DescriptorWrites.Num() == 0);

	DescriptorWrites.AddZeroed(ComputeShader->CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
	DescriptorImageInfo.AddZeroed(ComputeShader->CodeHeader.NEWDescriptorInfo.NumImageInfos);
	DescriptorBufferInfo.AddZeroed(ComputeShader->CodeHeader.NEWDescriptorInfo.NumBufferInfos);

	for (int32 Index = 0; Index < DescriptorImageInfo.Num(); ++Index)
	{
		// Texture.Load() still requires a default sampler...
		DescriptorImageInfo[Index].sampler = Device->GetDefaultSampler();
		DescriptorImageInfo[Index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DescriptorBufferInfo.GetData();

	NEWShaderDescriptorState.SetupDescriptorWrites(ComputeShader->CodeHeader.NEWDescriptorInfo, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo);
}

void FVulkanComputeShaderState::SetUniformBuffer(uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
{
	check(0);
#if 0
	uint32 VulkanBindingPoint = ComputeShader->GetBindingTable().UniformBufferBindingIndices[BindPoint];

	check(UniformBuffer && (UniformBuffer->GetBufferUsageFlags() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

	VkDescriptorBufferInfo* BufferInfo = &DescriptorBufferInfo[BindPoint];
	BufferInfo->buffer = UniformBuffer->GetHandle();
	BufferInfo->range = UniformBuffer->GetSize();

#if VULKAN_ENABLE_RHI_DEBUGGING
	//DebugInfo.UBs[BindPoint] = UniformBuffer;
#endif
#endif
}

void FVulkanComputeShaderState::SetSRVTextureView(uint32 BindPoint, const FVulkanTextureView& TextureView)
{
	ensure(0);
#if 0
	uint32 VulkanBindingPoint = ComputeShader->GetBindingTable().CombinedSamplerBindingIndices[BindPoint];
	DescriptorImageInfo[VulkanBindingPoint].imageView = TextureView.View;
	//DescriptorSamplerImageInfoForStage[VulkanBindingPoint].imageLayout = (TextureView.BaseTexture->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;

	DirtySamplerStates = true;
#if VULKAN_ENABLE_RHI_DEBUGGING
	//DebugInfo.SRVIs[BindPoint] = &TextureView;
#endif
#endif
}

void FVulkanComputeShaderState::SetSRV(uint32 TextureIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferView)
		{
			checkf(SRV->BufferView != VK_NULL_HANDLE, TEXT("Empty SRV"));
			SetSRVBufferViewState(TextureIndex, SRV->BufferView);
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			SetSRVTextureView(TextureIndex, SRV->TextureView);
		}
	}
	else
	{
		SetSRVBufferViewState(TextureIndex, nullptr);
	}
}

void FVulkanComputeShaderState::SetUAV(uint32 UAVIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->BufferView)
		{
			SetUAVBufferViewState(UAVIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			SetUAVTextureView(UAVIndex, UAV->TextureView);
		}
		else
		{
			ensure(0);
		}
	}
}

FVulkanComputePipeline* FVulkanComputeShaderState::PrepareForDispatch(const struct FVulkanComputePipelineState& State)
{
	TRefCountPtr<FVulkanComputePipeline>* Found = ComputePipelines.Find(State.CSS->ComputeShader);
	FVulkanComputePipeline* Pipeline = nullptr;
	if (Found)
	{
		Pipeline = *Found;
	}
	else
	{
		Pipeline = new FVulkanComputePipeline(Device, State.CSS);
		ComputePipelines.Add(State.CSS->ComputeShader, Pipeline);
	}

	return Pipeline;
}

void FVulkanComputeShaderState::ResetState()
{
#if 0
	DirtyPackedUniformBufferStaging = DirtyPackedUniformBufferStagingMask;
#endif

	CurrDescriptorSets = nullptr;

	LastBoundPipeline = VK_NULL_HANDLE;
}

bool FVulkanComputeShaderState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	check(GlobalUniformPool);

	FVulkanPendingComputeState& State = *CmdListContext->GetPendingComputeState();

	int32 WriteIndex = 0;

	CurrDescriptorSets = RequestDescriptorSets(CmdListContext, CmdBuffer);
	if (!CurrDescriptorSets)
	{
		return false;
	}

	const TArray<VkDescriptorSet>& DescriptorSetHandles = CurrDescriptorSets->GetHandles();
	int32 DescriptorSetIndex = 0;

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();

	const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
	++DescriptorSetIndex;

	bool bRequiresPackedUBUpdate = (NEWPackedUniformBufferStagingDirty != 0);
	if (bRequiresPackedUBUpdate)
	{
		UpdatePackedUniformBuffers(Device, ComputeShader->CodeHeader, NEWPackedUniformBufferStaging, NEWShaderDescriptorState, UniformBufferUploader, CPURingBufferBase, NEWPackedUniformBufferStagingDirty);
		NEWPackedUniformBufferStagingDirty = 0;
	}

	bool bRequiresNonPackedUBUpdate = (NEWShaderDescriptorState.DirtyMask != 0);
	if (!bRequiresNonPackedUBUpdate && !bRequiresPackedUBUpdate)
	{
		//#todo-rco: Skip this desc set writes and only call update for the modified ones!
		//continue;
		int x = 0;
	}

	NEWShaderDescriptorState.SetDescriptorSet(DescriptorSet);

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DescriptorWrites.Num());
	INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, DescriptorSetIndex);
#endif

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DescriptorWrites.Num(), DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}


FVulkanBoundShaderState::FVulkanBoundShaderState(
		FVulkanDevice* InDevice,
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI)
	: FVulkanShaderState(InDevice)
	, CacheLink(InVertexDeclarationRHI,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
	, bDirtyVertexStreams(true)
	, BindingsNum(0)
	, BindingsMask(0)
	, AttributesNum(0)
#if VULKAN_ENABLE_PIPELINE_CACHE
	, GlobalListLink(this)
#endif
{
	static int32 sID = 0;
	ID = sID++;
	INC_DWORD_STAT(STAT_VulkanNumBoundShaderState);

	FMemory::Memzero(VertexInputBindings);
	FMemory::Memzero(Attributes);
	FMemory::Memzero(VertexInputStateInfo);
	FMemory::Memzero(NEWPackedUniformBufferStagingDirty);

	FVulkanVertexDeclaration* InVertexDeclaration = ResourceCast(InVertexDeclarationRHI);
	FVulkanVertexShader* InVertexShader = ResourceCast(InVertexShaderRHI);
	FVulkanPixelShader* InPixelShader = ResourceCast(InPixelShaderRHI);
	FVulkanHullShader* InHullShader = ResourceCast(InHullShaderRHI);
	FVulkanDomainShader* InDomainShader = ResourceCast(InDomainShaderRHI);
	FVulkanGeometryShader* InGeometryShader = ResourceCast(InGeometryShaderRHI);

	// cache everything
	VertexDeclaration = InVertexDeclaration;
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
	HullShader = InHullShader;
	DomainShader = InDomainShader;
	GeometryShader = InGeometryShader;

#if VULKAN_ENABLE_PIPELINE_CACHE
	GlobalListLink.LinkHead(FVulkanPipelineStateCache::GetBSSList());
#endif

	// Setup working areas for the global uniforms
	check(VertexShader);
#if VULKAN_ENABLE_PIPELINE_CACHE
	ShaderHashes[SF_Vertex] = VertexShader->CodeHeader.SourceHash;
#endif
	InitGlobalUniformsForStage(VertexShader->CodeHeader, NEWPackedUniformBufferStaging[SF_Vertex], NEWPackedUniformBufferStagingDirty[SF_Vertex]);
	GenerateLayoutBindingsForStage(VK_SHADER_STAGE_VERTEX_BIT, EDescriptorSetStage::Vertex, VertexShader->CodeHeader, Layout);

	if (PixelShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Pixel] = PixelShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniformsForStage(PixelShader->CodeHeader, NEWPackedUniformBufferStaging[SF_Pixel], NEWPackedUniformBufferStagingDirty[SF_Pixel]);
		GenerateLayoutBindingsForStage(VK_SHADER_STAGE_FRAGMENT_BIT, EDescriptorSetStage::Pixel, PixelShader->CodeHeader, Layout);
	}
	if (GeometryShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Geometry] = GeometryShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniformsForStage(GeometryShader->CodeHeader, NEWPackedUniformBufferStaging[SF_Geometry], NEWPackedUniformBufferStagingDirty[SF_Geometry]);
		GenerateLayoutBindings(SF_Geometry, GeometryShader->CodeHeader);
		GenerateLayoutBindingsForStage(VK_SHADER_STAGE_GEOMETRY_BIT, EDescriptorSetStage::Geometry, GeometryShader->CodeHeader, Layout);
	}
	if (HullShader)
	{
		// Can't have Hull w/o Domain
		check(DomainShader);
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Hull] = HullShader->CodeHeader.SourceHash;
		ShaderHashes[SF_Domain] = DomainShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniformsForStage(HullShader->CodeHeader, NEWPackedUniformBufferStaging[SF_Hull], NEWPackedUniformBufferStagingDirty[SF_Hull]);
		InitGlobalUniformsForStage(DomainShader->CodeHeader, NEWPackedUniformBufferStaging[SF_Domain], NEWPackedUniformBufferStagingDirty[SF_Domain]);
		GenerateLayoutBindingsForStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, EDescriptorSetStage::Hull, HullShader->CodeHeader, Layout);
		GenerateLayoutBindingsForStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, EDescriptorSetStage::Domain, DomainShader->CodeHeader, Layout);
	}
	else
	{
		// Can't have Domain w/o Hull
		check(DomainShader == nullptr);
	}

	Layout->Compile();
	GenerateVertexInputStateInfo();

	CreateDescriptorWriteInfos();
}

FVulkanBoundShaderState::~FVulkanBoundShaderState()
{
#if VULKAN_ENABLE_PIPELINE_CACHE
	GlobalListLink.Unlink();
#endif

	for (int32 Index = 0; Index < DescriptorSetsEntries.Num(); ++Index)
	{
		delete DescriptorSetsEntries[Index];
	}
	DescriptorSetsEntries.Empty(0);

	// toss the pipeline states
	for (auto& Pair : PipelineCache)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		// Reference is decremented inside the Destroy function
		Device->PipelineStateCache->DestroyPipeline(Pair.Value);
#else
		Pair.Value->Destroy();
#endif
	}

	DEC_DWORD_STAT(STAT_VulkanNumBoundShaderState);
}

FVulkanGfxPipeline* FVulkanBoundShaderState::PrepareForDraw(FVulkanRenderPass* RenderPass, const FVulkanPipelineGraphicsKey& PipelineKey, uint32 VertexInputKey, const struct FVulkanGfxPipelineState& State)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanGetOrCreatePipeline);

	// have we made a matching state object yet?
	FVulkanGfxPipeline* Pipeline = PipelineCache.FindRef(PipelineKey);

	// make one if not
	if (Pipeline == nullptr)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		// Try the device cache
		FVulkanGfxPipelineStateKey PipelineCreateInfo(PipelineKey, VertexInputKey, this);
		Pipeline = Device->PipelineStateCache->Find(PipelineCreateInfo);
		if (Pipeline)
		{
			// Add it to the local cache; manually control RefCount
			PipelineCache.Add(PipelineKey, Pipeline);
			Pipeline->AddRef();
		}
		else
#endif
		{
			// Create a new one
			Pipeline = new FVulkanGfxPipeline(Device);
#if VULKAN_ENABLE_PIPELINE_CACHE
			Device->PipelineStateCache->CreateAndAdd(RenderPass, PipelineCreateInfo, Pipeline, State, *this);
#else
			Pipeline->Create(State);
#endif

			// Add it to the local cache; manually control RefCount
			PipelineCache.Add(PipelineKey, Pipeline);
			Pipeline->AddRef();
/*
#if !UE_BUILD_SHIPPING
			if (Device->FrameCounter > 3)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Created a hitchy pipeline state for hash %llx%llx %x (this = %x) VS %s %x %d PS %s %x %d\n"), 
					PipelineKey.Key[0], PipelineKey.Key[1], VertexInputKey, this, *VertexShader->DebugName, (void*)VertexShader.GetReference(), VertexShader->GlslSource.Num(), 
					*PixelShader->DebugName, (void*)PixelShader.GetReference(), PixelShader->GlslSource.Num());
			}
#endif*/
		}
	}

	return Pipeline;
}


void FVulkanBoundShaderState::ResetState()
{
	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");
#if 0
	for(uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		DirtyTextures[Stage] = true;
		DirtySamplerStates[Stage] = true;
		DirtySRVs[Stage] = true;
		DirtyPackedUniformBufferStaging[Stage] = DirtyPackedUniformBufferStagingMask[Stage];
	}
#endif

	CurrDescriptorSets = nullptr;

	LastBoundPipeline = VK_NULL_HANDLE;
	bDirtyVertexStreams = true;
}

VkPipelineLayout FVulkanShaderState::GetPipelineLayout() const
{
	check(Layout);

	if (PipelineLayout != VK_NULL_HANDLE)
	{
		return PipelineLayout;
	}

	const TArray<VkDescriptorSetLayout>& LayoutHandles = Layout->GetHandles();

#if !VULKAN_KEEP_CREATE_INFO
	VkPipelineLayoutCreateInfo CreateInfo;
#endif
	FMemory::Memzero(CreateInfo);
	CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	CreateInfo.pNext = nullptr;
	CreateInfo.setLayoutCount = LayoutHandles.Num();
	CreateInfo.pSetLayouts = LayoutHandles.GetData();
	CreateInfo.pushConstantRangeCount = 0;
	CreateInfo.pPushConstantRanges = nullptr;

	VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineLayout(Device->GetInstanceHandle(), &CreateInfo, nullptr, &PipelineLayout));

	return PipelineLayout;
}

void FVulkanBoundShaderState::GenerateLayoutBindings(EShaderFrequency Stage, const FVulkanCodeHeader& CodeHeader)
{
	VkShaderStageFlagBits StageFlags = UEFrequencyToVKStageBit(Stage);

	//#todo-rco: Mobile assumption!
	EDescriptorSetStage DescSet = GetDescriptorSetForStage(Stage);
	int32 DescriptorSetIndex = (int32)DescSet;

	VkDescriptorSetLayoutBinding Binding;
	FMemory::Memzero(Binding);
	Binding.descriptorCount = 1;
	Binding.stageFlags = StageFlags;
	for (int32 Index = 0; Index < CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num(); ++Index)
	{
		Binding.binding = Index;
		Binding.descriptorType = CodeHeader.NEWDescriptorInfo.DescriptorTypes[Index];
		Layout->AddDescriptor(DescriptorSetIndex, Binding, Index);
	}
}

void FVulkanBoundShaderState::CreateDescriptorWriteInfos()
{
	check(DescriptorWrites.Num() == 0);

	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");

	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		DescriptorWrites.AddZeroed(StageShader->CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num());
		DescriptorImageInfo.AddZeroed(StageShader->CodeHeader.NEWDescriptorInfo.NumImageInfos);
		DescriptorBufferInfo.AddZeroed(StageShader->CodeHeader.NEWDescriptorInfo.NumBufferInfos);
	}

	for (int32 Index = 0; Index < DescriptorImageInfo.Num(); ++Index)
	{
		// Texture.Load() still requires a default sampler...
		DescriptorImageInfo[Index].sampler = Device->GetDefaultSampler();
		DescriptorImageInfo[Index].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkWriteDescriptorSet* CurrentDescriptorWrite = DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DescriptorBufferInfo.GetData();

	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		NEWShaderDescriptorState[Stage].SetupDescriptorWrites(StageShader->CodeHeader.NEWDescriptorInfo, CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo);
		CurrentDescriptorWrite += StageShader->CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num();
		CurrentImageInfo += StageShader->CodeHeader.NEWDescriptorInfo.NumImageInfos;
		CurrentBufferInfo += StageShader->CodeHeader.NEWDescriptorInfo.NumBufferInfos;
	}
}

void FVulkanBoundShaderState::InternalBindVertexStreams(FVulkanCmdBuffer* Cmd, const void* InVertexStreams)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanBindVertexStreamsTime);
#endif
	check(VertexDeclaration);

	// Its possible to have no vertex buffers
	if (AttributesNum == 0)
	{
		// However, we need to verify that there are also no bindings
		check(BindingsNum == 0);
		return;
	}

	FVulkanPendingGfxState::FVertexStream* Streams = (FVulkanPendingGfxState::FVertexStream*)InVertexStreams;

	Tmp.VertexBuffers.Reset(0);
	Tmp.VertexOffsets.Reset(0);
	const VkVertexInputAttributeDescription* CurrAttribute = nullptr;
	for (uint32 BindingIndex = 0; BindingIndex < BindingsNum; BindingIndex++)
	{
		const VkVertexInputBindingDescription& CurrBinding = VertexInputBindings[BindingIndex];

		uint32 StreamIndex = BindingToStream.FindChecked(BindingIndex);
		FVulkanPendingGfxState::FVertexStream& CurrStream = Streams[StreamIndex];

		// Verify the vertex buffer is set
		if (!CurrStream.Stream && !CurrStream.Stream2 && CurrStream.Stream3 == VK_NULL_HANDLE)
		{
			// The attribute in stream index is probably compiled out
			#if VULKAN_HAS_DEBUGGING_ENABLED
				// Lets verify
				for (uint32 AttributeIndex = 0; AttributeIndex<AttributesNum; AttributeIndex++)
				{
					if (Attributes[AttributeIndex].binding == CurrBinding.binding)
					{
						UE_LOG(LogVulkanRHI, Warning, TEXT("Missing binding on location %d in '%s' vertex shader"),
							CurrBinding.binding,
							*GetShader(SF_Vertex).DebugName);
						ensure(0);
					}
				}
			#endif
			continue;
		}

		Tmp.VertexBuffers.Add(CurrStream.Stream
			? CurrStream.Stream->GetBufferHandle()
			: (CurrStream.Stream2
				? CurrStream.Stream2->GetHandle()
				: CurrStream.Stream3)
			);
		Tmp.VertexOffsets.Add(CurrStream.BufferOffset);
	}

	if(Tmp.VertexBuffers.Num() > 0)
	{
		// Bindings are expected to be in ascending order with no index gaps in between:
		// Correct:		0, 1, 2, 3
		// Incorrect:	1, 0, 2, 3
		// Incorrect:	0, 2, 3, 5
		// Reordering and creation of stream binding index is done in "GenerateVertexInputStateInfo()"
		VulkanRHI::vkCmdBindVertexBuffers(Cmd->GetHandle(), 0, Tmp.VertexBuffers.Num(), Tmp.VertexBuffers.GetData(), Tmp.VertexOffsets.GetData());
	}
}

static inline VkFormat UEToVkFormat(EVertexElementType Type)
{
	switch (Type)
	{
	case VET_Float1:
		return VK_FORMAT_R32_SFLOAT;
	case VET_Float2:
		return VK_FORMAT_R32G32_SFLOAT;
	case VET_Float3:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case VET_PackedNormal:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case VET_UByte4:
		return VK_FORMAT_R8G8B8A8_UINT;
	case VET_UByte4N:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case VET_Color:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case VET_Short2:
		return VK_FORMAT_R16G16_SINT;
	case VET_Short4:
		return VK_FORMAT_R16G16B16A16_SINT;
	case VET_Short2N:
		return VK_FORMAT_R16G16_SNORM;
	case VET_Half2:
		return VK_FORMAT_R16G16_SFLOAT;
	case VET_Half4:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case VET_Short4N:		// 4 X 16 bit word: normalized 
		return VK_FORMAT_R16G16B16A16_SNORM;
	case VET_UShort2:
		return VK_FORMAT_R16G16_UINT;
	case VET_UShort4:
		return VK_FORMAT_R16G16B16A16_UINT;
	case VET_UShort2N:		// 16 bit word normalized to (value/65535.0:value/65535.0:0:0:1)
		return VK_FORMAT_R16G16_UNORM;
	case VET_UShort4N:		// 4 X 16 bit word unsigned: normalized 
		return VK_FORMAT_R16G16B16A16_UNORM;
	case VET_Float4:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case VET_URGB10A2N:
		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	default:
		break;
	}

	check(!"Undefined vertex-element format conversion");
	return VK_FORMAT_UNDEFINED;
}

inline void FVulkanBoundShaderState::ProcessVertexElementForBinding(const FVertexElement& Element)
{
	check(Element.StreamIndex < MaxVertexElementCount);

	VkVertexInputBindingDescription& CurrBinding = VertexInputBindings[Element.StreamIndex];
	if((BindingsMask & (1 << Element.StreamIndex)) != 0)
	{
		// If exists, validate.
		// Info must be the same
		check(CurrBinding.binding == Element.StreamIndex);
		check(CurrBinding.inputRate == Element.bUseInstanceIndex ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX);
		check(CurrBinding.stride == Element.Stride);
	}
	else
	{
		// Zeroed outside
		check(CurrBinding.binding == 0 && CurrBinding.inputRate == 0 && CurrBinding.stride == 0);
		CurrBinding.binding = Element.StreamIndex;
		CurrBinding.inputRate = Element.bUseInstanceIndex ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
		CurrBinding.stride = Element.Stride;

		// Add mask flag and increment number of bindings
		BindingsMask |= 1 << Element.StreamIndex;
	}
}

void FVulkanBoundShaderState::GenerateVertexInputStateInfo()
{
	BindingsMask = 0;
	FMemory::Memzero(VertexInputBindings);

	AttributesNum = 0;
	FMemory::Memzero(Attributes);

	// Generate vertex attribute Layout
	check(VertexShader);
	const FVertexDeclarationElementList& VertexInput = VertexDeclaration->Elements;

	// Generate Bindings
	const uint32 VertexHeaderInOutAttributeMask = GetShader(SF_Vertex).CodeHeader.SerializedBindings.InOutMask;
	for (const FVertexElement& Element : VertexInput)
	{
		if((1<<Element.AttributeIndex) & VertexHeaderInOutAttributeMask)
		{
			ProcessVertexElementForBinding(Element);
		}
	}

	// Remove gaps between bindings
	BindingsNum = 0;
	BindingToStream.Reset();
	StreamToBinding.Reset();
	for(int32 i=0; i<ARRAY_COUNT(VertexInputBindings); i++)
	{
		if(!((1<<i) & BindingsMask))
		{
			continue;
		}

		BindingToStream.Add(BindingsNum, i);
		StreamToBinding.Add(i, BindingsNum);
		VkVertexInputBindingDescription& CurrBinding = VertexInputBindings[BindingsNum];
		CurrBinding = VertexInputBindings[i];
		CurrBinding.binding = BindingsNum;
		BindingsNum++;
	}

	// Clean originally placed bindings
	FMemory::Memset(VertexInputBindings + BindingsNum, 0, sizeof(VertexInputBindings[0]) * (ARRAY_COUNT(VertexInputBindings)-BindingsNum));

	// Attributes are expected to be uninitialized/empty
	check(AttributesNum == 0);
	for (const FVertexElement& CurrElement : VertexInput)
	{
		// Mask-out unused vertex input
		if (	(!((1<<CurrElement.AttributeIndex) & VertexHeaderInOutAttributeMask))
			||	!StreamToBinding.Contains(CurrElement.StreamIndex))
		{
			continue;
		}

		VkVertexInputAttributeDescription& CurrAttribute = Attributes[AttributesNum++]; // Zeroed at the begin of the function
		check(CurrAttribute.location == 0 && CurrAttribute.binding == 0 && CurrAttribute.format == 0 && CurrAttribute.offset == 0);

		CurrAttribute.binding = StreamToBinding.FindChecked(CurrElement.StreamIndex);
		CurrAttribute.location = CurrElement.AttributeIndex;
		CurrAttribute.format = UEToVkFormat(CurrElement.Type);
		CurrAttribute.offset = CurrElement.Offset;
	}

	VertexInputStateInfo.Create(BindingsNum, VertexInputBindings, AttributesNum, Attributes);
}

void FVulkanBoundShaderState::SetSRV(EShaderFrequency Stage, uint32 TextureIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferView)
		{
			checkf(SRV->BufferView != VK_NULL_HANDLE, TEXT("Empty SRV"));
			SetBufferViewState(Stage, TextureIndex, SRV->BufferView);
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			SetTextureView(Stage, TextureIndex, SRV->TextureView);
		}
	}
	else
	{
		NEWShaderDescriptorState[Stage].ClearBufferView(TextureIndex);
	}
}

void FVulkanBoundShaderState::SetUniformBuffer(EShaderFrequency Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
{
	check(0);
#if 0
	FVulkanShader* Shader = GetShaderPtr(Stage);
	uint32 VulkanBindingPoint = Shader->GetBindingTable().UniformBufferBindingIndices[BindPoint];

	check(UniformBuffer && (UniformBuffer->GetBufferUsageFlags() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

	VkDescriptorBufferInfo* BufferInfo = &DescriptorBufferInfoForStage[Stage][BindPoint];
	BufferInfo->buffer = UniformBuffer->GetHandle();
	BufferInfo->range = UniformBuffer->GetSize();

	//#todo-rco: Mark Dirty UB	
#if VULKAN_ENABLE_RHI_DEBUGGING
	DebugInfo.UBs[Stage][BindPoint] = UniformBuffer;
#endif
#endif
}

FVulkanShaderState::FDescriptorSetsPair::~FDescriptorSetsPair()
{
	delete DescriptorSets;
}

inline FVulkanDescriptorSets* FVulkanShaderState::RequestDescriptorSets(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer)
{
	FDescriptorSetsEntry* FoundEntry = nullptr;
	for (FDescriptorSetsEntry* DescriptorSetsEntry : DescriptorSetsEntries)
	{
		if (DescriptorSetsEntry->CmdBuffer == CmdBuffer)
		{
			FoundEntry = DescriptorSetsEntry;
		}
	}

	if (!FoundEntry)
	{
		if (Layout->GetLayouts().Num() == 0)
		{
			return nullptr;
		}

		FoundEntry = new FDescriptorSetsEntry(CmdBuffer);
		DescriptorSetsEntries.Add(FoundEntry);
	}

	const uint64 CmdBufferFenceSignaledCounter = CmdBuffer->GetFenceSignaledCounter();
	for (int32 Index = 0; Index < FoundEntry->Pairs.Num(); ++Index)
	{
		FDescriptorSetsPair& Entry = FoundEntry->Pairs[Index];
		if (Entry.FenceCounter < CmdBufferFenceSignaledCounter)
		{
			Entry.FenceCounter = CmdBufferFenceSignaledCounter;
			return Entry.DescriptorSets;
		}
	}

	FDescriptorSetsPair* NewEntry = new (FoundEntry->Pairs) FDescriptorSetsPair;
	NewEntry->DescriptorSets = new FVulkanDescriptorSets(Device, GetDescriptorSetsLayout(), Context);
	NewEntry->FenceCounter = CmdBufferFenceSignaledCounter;
	return NewEntry->DescriptorSets;
}

FORCEINLINE_DEBUGGABLE void FVulkanShaderState::UpdateDescriptorSetsForStage(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer,
	FVulkanGlobalUniformPool* GlobalUniformPool, VkDescriptorSet DescriptorSet, FVulkanShader* StageShader,
	uint32 RemainingGlobalUniformMask, VkDescriptorBufferInfo* BufferInfo, TArray<uint8>* PackedUniformBuffer, 
	FVulkanRingBuffer* RingBuffer, uint8* RingBufferBase, int32& WriteIndex)
{
	check(0);
#if 0
	//if (bRequiresUpdate)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSResources);
#endif
		const uint32 NumDescriptorsForStageWithoutPackedUBs = StageShader->GetNumDescriptorsExcludingPackedUniformBuffers();
		for (uint32 Index = 0; Index < NumDescriptorsForStageWithoutPackedUBs; ++Index)
		{
			VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
			WriteDesc->dstSet = DescriptorSet;
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSUniformBuffers);
		while (RemainingGlobalUniformMask)
		{
			if (RemainingGlobalUniformMask & 1)
			{
				//Get a uniform buffer from the dynamic pool
				int32 UBSize = PackedUniformBuffer->Num();

				// get offset into the RingBufferBase pointer
				uint64 RingBufferOffset = RingBuffer->AllocateMemory(UBSize, Device->GetLimits().minUniformBufferOffsetAlignment);

				// get location in the ring buffer to use
				FMemory::Memcpy(RingBufferBase + RingBufferOffset, PackedUniformBuffer->GetData(), UBSize);

				// Here we can specify a more precise buffer update
				// However, this need to complemented with the buffer map/unmap functionality.
				//@NOTE: bufferView is for texel buffers
				BufferInfo->buffer = RingBuffer->GetHandle();
				BufferInfo->offset = RingBufferOffset + RingBuffer->GetBufferOffset();
				BufferInfo->range = UBSize;

				VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
				WriteDesc->dstSet = DescriptorSet;

				check(WriteDesc->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				++BufferInfo;
			}
			RemainingGlobalUniformMask = RemainingGlobalUniformMask >> 1;
			++PackedUniformBuffer;
		}
	}
#endif
}


void FVulkanShaderState::UpdatePackedUniformBuffers(FVulkanDevice* Device, const FVulkanCodeHeader& CodeHeader, FNEWPackedUniformBufferStaging& NEWPackedUniformBufferStaging, 
	FNEWVulkanShaderDescriptorState& NEWShaderDescriptorState, FVulkanUniformBufferUploader* UniformBufferUploader, uint8* CPURingBufferBase, uint64 RemainingPackedUniformsMask)
{
	VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;
	int32 PackedUBIndex = 0;
	SCOPE_CYCLE_COUNTER(STAT_VulkanApplyDSUniformBuffers);
	while (RemainingPackedUniformsMask)
	{
		if (RemainingPackedUniformsMask & 1)
		{
			const TArray<uint8>& StagedUniformBuffer = NEWPackedUniformBufferStaging.PackedUniformBuffers[PackedUBIndex];
			int32 BindingIndex = CodeHeader.NEWPackedUBToVulkanBindingIndices[PackedUBIndex].VulkanBindingIndex;

			const int32 UBSize = StagedUniformBuffer.Num();

			// get offset into the RingBufferBase pointer
			uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(UBSize, UBOffsetAlignment);

			// get location in the ring buffer to use
			FMemory::Memcpy(CPURingBufferBase + RingBufferOffset, StagedUniformBuffer.GetData(), UBSize);

			NEWShaderDescriptorState.SetUniformBuffer(BindingIndex, UniformBufferUploader->GetCPUBufferHandle(), RingBufferOffset + UniformBufferUploader->GetCPUBufferOffset(), UBSize);
		}
		RemainingPackedUniformsMask = RemainingPackedUniformsMask >> 1;
		++PackedUBIndex;
	}
}

void FVulkanShaderState::SetUniformBufferConstantDataForStage(const FVulkanCodeHeader& CodeHeader, uint32 BindPoint, 
	const TArray<uint8>& ConstantData, FNEWPackedUniformBufferStaging& NEWPackedUniformBufferStaging, uint64& NEWPackedUniformBufferStagingDirty)
{
	// Emulated UBs. Assumes UniformBuffersCopyInfo table is sorted by CopyInfo.SourceUBIndex
	if (BindPoint < (uint32)CodeHeader.NEWEmulatedUBCopyRanges.Num())
	{
		uint32 Range = CodeHeader.NEWEmulatedUBCopyRanges[BindPoint];
		uint16 Start = (Range >> 16) & 0xffff;
		uint16 Count = Range & 0xffff;
		for (int32 Index = Start; Index < Start + Count; ++Index)
		{
			const CrossCompiler::FUniformBufferCopyInfo& CopyInfo = CodeHeader.UniformBuffersCopyInfo[Index];
			check(CopyInfo.SourceUBIndex == BindPoint);
			TArray<uint8>& StagingBuffer = NEWPackedUniformBufferStaging.PackedUniformBuffers[(int32)CopyInfo.DestUBIndex];
			//check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
			FMemory::Memcpy(StagingBuffer.GetData() + CopyInfo.DestOffsetInFloats * 4, ConstantData.GetData() + CopyInfo.SourceOffsetInFloats * 4, CopyInfo.SizeInFloats * 4);
			NEWPackedUniformBufferStagingDirty = NEWPackedUniformBufferStagingDirty | ((uint64)1 << (uint64)CopyInfo.DestUBIndex);
		}
	}
}


bool FVulkanBoundShaderState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

    check(GlobalUniformPool);

	FVulkanPendingGfxState& State = *CmdListContext->GetPendingGfxState();

	int32 WriteIndex = 0;

	CurrDescriptorSets = RequestDescriptorSets(CmdListContext, CmdBuffer);
	if (!CurrDescriptorSets)
	{
		return false;
	}

	const TArray<VkDescriptorSet>& DescriptorSetHandles = CurrDescriptorSets->GetHandles();
	int32 DescriptorSetIndex = 0;

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();

	//#todo-rco: Compute!
	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");
	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		if (StageShader->CodeHeader.NEWDescriptorInfo.DescriptorTypes.Num() == 0)
		{
			// Empty set, still has its own index
			++DescriptorSetIndex;
			continue;
		}

		const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
		++DescriptorSetIndex;

		bool bRequiresPackedUBUpdate = (NEWPackedUniformBufferStagingDirty[Stage] != 0);
		if (bRequiresPackedUBUpdate)
		{
			UpdatePackedUniformBuffers(Device, StageShader->CodeHeader, NEWPackedUniformBufferStaging[Stage], NEWShaderDescriptorState[Stage], UniformBufferUploader, CPURingBufferBase, NEWPackedUniformBufferStagingDirty[Stage]);
			NEWPackedUniformBufferStagingDirty[Stage] = 0;
		}

		bool bRequiresNonPackedUBUpdate = (NEWShaderDescriptorState[Stage].DirtyMask != 0);
		if (!bRequiresNonPackedUBUpdate && !bRequiresPackedUBUpdate)
		{
			//#todo-rco: Skip this desc set writes and only call update for the modified ones!
			//continue;
			int x = 0;
		}

		NEWShaderDescriptorState[Stage].SetDescriptorSet(DescriptorSet);
	}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DescriptorWrites.Num());
	INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, DescriptorSetIndex);
#endif

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DescriptorWrites.Num(), DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}

FBoundShaderStateRHIRef FVulkanDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	//#todo-rco: Check for null PS support
	if (!PixelShaderRHI)
	{
		// use special null pixel shader when PixelShader was set to NULL
		PixelShaderRHI = TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel))->GetPixelShader();
	}

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);

	if (CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}

	return new FVulkanBoundShaderState(Device, VertexDeclarationRHI,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI);
}

FVulkanDescriptorPool* FVulkanCommandListContext::AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, const FVulkanDescriptorSetsLayout& Layout, VkDescriptorSet* OutSets)
{
	FVulkanDescriptorPool* Pool = DescriptorPools.Last();
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = InDescriptorSetAllocateInfo;
	VkResult Result = VK_ERROR_OUT_OF_DEVICE_MEMORY;

	int32 ValidationLayerEnabled = 0;
#if VULKAN_HAS_DEBUGGING_ENABLED
	extern TAutoConsoleVariable<int32> GValidationCvar;
	ValidationLayerEnabled = GValidationCvar->GetInt();
#endif
	// Only try to find if it will fit in the pool if we're in validation mode
	if (ValidationLayerEnabled == 0 || Pool->CanAllocate(Layout))
	{
		DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();
		Result = VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets);
	}

	if (Result < VK_SUCCESS)
	{
		if (Pool->IsEmpty())
		{
			VERIFYVULKANRESULT(Result);
		}
		else
		{
			// Spec says any negative value could be due to fragmentation, so create a new Pool. If it fails here then we really are out of memory!
			Pool = new FVulkanDescriptorPool(Device);
			DescriptorPools.Add(Pool);
			DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();
			VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets));
		}
	}

	return Pool;
}
