// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanShaders.cpp: Vulkan shader RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "GlobalShader.h"

#if (!PLATFORM_ANDROID)
#include <vulkan/icd-spv.h>
#endif

void FVulkanShader::Create(EShaderFrequency Frequency, const TArray<uint8>& InShaderCode)
{
	check(Device);

	FMemoryReader Ar(InShaderCode, true);

	Ar << CodeHeader;

	Ar << BindingTable;

	TArray<ANSICHAR> DebugNameArray;
	Ar << DebugNameArray;
	DebugName = ANSI_TO_TCHAR(DebugNameArray.GetData());

	TArray<uint8> Spirv;
	Ar << Spirv;

	Ar << GlslSource;

	int32 CodeOffset = Ar.Tell();

	VkShaderModuleCreateInfo ModuleCreateInfo;
	FMemory::Memzero(ModuleCreateInfo);
	ModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ModuleCreateInfo.pNext = nullptr;
	ModuleCreateInfo.flags = 0;

	const EShaderSourceType ShaderType = GetShaderType();
	switch(ShaderType)
	{
	case SHADER_TYPE_GLSL:
		{
//#if ANDROID: fall through
//#else
#if !PLATFORM_ANDROID
			// Create fake SPV structure to feed GLSL to the driver "under the covers"

			ModuleCreateInfo.codeSize = 3 * sizeof(uint32) + GlslSource.Num() + 1;
			Code = (uint32*)FMemory::Malloc(ModuleCreateInfo.codeSize);
			ModuleCreateInfo.pCode = Code;

			/* try version 0 first: VkShaderStage followed by GLSL */
			Code[0] = ICD_SPV_MAGIC;
			Code[1] = 0;
			FMemory::Memcpy(&Code[3], GlslSource.GetData(), GlslSource.Num() + 1);

			break;
#endif
		}
	case SHADER_TYPE_ESSL:
		{
			if (PLATFORM_ANDROID && Frequency == SF_Vertex)
			{
				//#todo-rco: Slow! Remove me when not needed on Android...
				FString NewSource = ANSI_TO_TCHAR(GlslSource.GetData());
				NewSource.Replace(TEXT("gl_VertexIndex"), TEXT("gl_VertexID"));
				NewSource.Replace(TEXT("gl_InstanceIndex"), TEXT("gl_InstanceID"));
				FMemory::Memcpy(GlslSource.GetData(), TCHAR_TO_ANSI(*NewSource), NewSource.Len() + 1);
			}
			ModuleCreateInfo.codeSize = FCStringAnsi::Strlen(GlslSource.GetData()) + 1;
			Code = (uint32*)FMemory::Malloc(ModuleCreateInfo.codeSize);
			ModuleCreateInfo.pCode = Code;
			FMemory::Memcpy(Code, GlslSource.GetData(), GlslSource.Num() + 1);
			break;
		}
	case SHADER_TYPE_SPIRV:
		{
			check(Code == nullptr)
			ModuleCreateInfo.codeSize = Spirv.Num();
			Code = (uint32*)FMemory::Malloc(ModuleCreateInfo.codeSize);
			FMemory::Memcpy(Code, Spirv.GetData(), ModuleCreateInfo.codeSize);
			break;
		}
	default:
		checkf(false, TEXT("Unhandled Vulkan ShaderType, see 'r.Vulkan.UseGLSL' console variable."));
		break;
	}

	check(Code != nullptr);
	CodeSize = ModuleCreateInfo.codeSize;
	ModuleCreateInfo.pCode = Code;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, nullptr, &ShaderModule));
}


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

FVulkanShader::EShaderSourceType FVulkanShader::GetShaderType()
{
	static EShaderSourceType Type = SHADER_TYPE_UNINITIALIZED;

	// Initialize type according to the settings
	if(Type == SHADER_TYPE_UNINITIALIZED)
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Vulkan.UseGLSL"));
		if(CVar)
		{
			Type = (EShaderSourceType)CVar->GetInt();
		}
		else
		{
			Type = SHADER_TYPE_SPIRV;
		}
	}

	return Type;
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
	VULKAN_SIGNAL_UNIMPLEMENTED();

#if 0
	FVulkanHullShader* Shader = new FVulkanHullShader(&Device, Code);
	return Shader;
#else
	return nullptr;
#endif
}

FDomainShaderRHIRef FVulkanDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	VULKAN_SIGNAL_UNIMPLEMENTED();

#if 0
	FVulkanDomainShader* Shader = new FVulkanDomainShader(&Device, Code);
	return Shader;
#else
	return nullptr;
#endif
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
	VULKAN_SIGNAL_UNIMPLEMENTED();

#if 0
	FVulkanComputeShader* Shader = new FVulkanComputeShader(&Device, Code);
	return Shader;
#else
	return nullptr;
#endif
}

FVulkanBoundShaderState::FVulkanBoundShaderState(
		FVulkanDevice* InDevice,
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI)
	: CacheLink(InVertexDeclarationRHI,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
	, PipelineLayout(VK_NULL_HANDLE)
	, Device(InDevice)
	, Layout(nullptr)
	, CurrDescriptorSets(nullptr)
	, LastBoundPipeline(VK_NULL_HANDLE)
	, bDirtyVertexStreams(true)
	, BindingsNum(0)
	, BindingsMask(0)
	, AttributesNum(0)
#if VULKAN_ENABLE_PIPELINE_CACHE
	, GlobalListLink(this)
#endif
{
	INC_DWORD_STAT(STAT_VulkanNumBoundShaderState);

	FMemory::Memzero(VertexInputBindings);
	FMemory::Memzero(Attributes);
	FMemory::Memzero(VertexInputStateInfo);
	FMemory::Memzero(DescriptorSamplerImageInfoForStage);

	static_assert(CrossCompiler::PACKED_TYPEINDEX_MAX <= sizeof(DirtyPackedUniformBufferStagingMask[0]) * 8, "Won't fit!");
	FMemory::Memzero(DescriptorBufferInfoForPackedUBForStage);
	FMemory::Memzero(DirtyPackedUniformBufferStaging);
	FMemory::Memzero(DirtyPackedUniformBufferStagingMask);
	FMemory::Memzero(DescriptorBufferInfoForStage);
	FMemory::Memzero(SRVWriteInfoForStage);

	check(Device);

	Layout = new FVulkanDescriptorSetsLayout(Device);

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
	if (VertexShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Vertex] = VertexShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniforms(SF_Vertex, VertexShader->CodeHeader.SerializedBindings);
	}
	if (PixelShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Pixel] = PixelShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniforms(SF_Pixel, PixelShader->CodeHeader.SerializedBindings);
	}
	if (GeometryShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Geometry] = GeometryShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniforms(SF_Geometry, GeometryShader->CodeHeader.SerializedBindings);
	}
	if (HullShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Hull] = HullShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniforms(SF_Hull, HullShader->CodeHeader.SerializedBindings);
	}
	if (DomainShader)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		ShaderHashes[SF_Domain] = DomainShader->CodeHeader.SourceHash;
#endif
		InitGlobalUniforms(SF_Domain, DomainShader->CodeHeader.SerializedBindings);
	}

	GenerateLayoutBindings();

	uint32 UniformCombinedSamplerCount = 0;
	uint32 UniformSamplerBufferCount = 0;
	uint32 UniformBufferCount = 0;
	GetDescriptorInfoCounts(UniformCombinedSamplerCount, UniformSamplerBufferCount, UniformBufferCount);

	// Setup Write information
	CreateDescriptorWriteInfo(UniformCombinedSamplerCount, UniformSamplerBufferCount, UniformBufferCount);
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

	check(Layout);
	delete Layout;

	if (PipelineLayout != VK_NULL_HANDLE)
	{
		Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::PipelineLayout, PipelineLayout);
		PipelineLayout = VK_NULL_HANDLE;
	}

	DEC_DWORD_STAT(STAT_VulkanNumBoundShaderState);
}

FVulkanPipeline* FVulkanBoundShaderState::PrepareForDraw(const FVulkanPipelineGraphicsKey& PipelineKey, uint32 VertexInputKey, const struct FVulkanPipelineState& State)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanGetOrCreatePipeline);

	// have we made a matching state object yet?
	FVulkanPipeline* Pipeline = PipelineCache.FindRef(PipelineKey);

	// make one if not
	if (Pipeline == nullptr)
	{
#if VULKAN_ENABLE_PIPELINE_CACHE
		// Try the device cache
		FVulkanPipelineStateKey PipelineCreateInfo(PipelineKey, VertexInputKey, this);
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
			Pipeline = new FVulkanPipeline(Device);
#if VULKAN_ENABLE_PIPELINE_CACHE
			Device->PipelineStateCache->CreateAndAdd(PipelineCreateInfo, Pipeline, State, *this);
#else
			Pipeline->Create(State);
#endif

			// Add it to the local cache; manually control RefCount
			PipelineCache.Add(PipelineKey, Pipeline);
			Pipeline->AddRef();

#if !UE_BUILD_SHIPPING
			if (Device->FrameCounter > 3)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Created a hitchy pipeline state for hash %llx%llx %x (this = %x) VS %s %x %d PS %s %x %d\n"), 
					PipelineKey.Key[0], PipelineKey.Key[1], VertexInputKey, this, *VertexShader->DebugName, (void*)VertexShader.GetReference(), VertexShader->GlslSource.Num(), 
					*PixelShader->DebugName, (void*)PixelShader.GetReference(), PixelShader->GlslSource.Num());
			}
#endif
		}
	}

	return Pipeline;
}


void FVulkanBoundShaderState::ResetState()
{
	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");
	for(uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		DirtyTextures[Stage] = true;
		DirtySamplerStates[Stage] = true;
		DirtySRVs[Stage] = true;
		DirtyPackedUniformBufferStaging[Stage] = DirtyPackedUniformBufferStagingMask[Stage];
	}

	CurrDescriptorSets = nullptr;

	LastBoundPipeline = VK_NULL_HANDLE;
	bDirtyVertexStreams = true;
}

const VkPipelineLayout& FVulkanBoundShaderState::GetPipelineLayout() const
{
	check(Layout);

	if (PipelineLayout != VK_NULL_HANDLE)
	{
		return PipelineLayout;
	}

	const TArray<VkDescriptorSetLayout>& LayoutHandles = Layout->GetHandles();

	VkPipelineLayoutCreateInfo CreateInfo;
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

void FVulkanBoundShaderState::BindPipeline(VkCommandBuffer CmdBuffer, VkPipeline NewPipeline)
{
	if (LastBoundPipeline != NewPipeline)
	{
		//#todo-rco: Compute
		VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, NewPipeline);
		LastBoundPipeline = NewPipeline;
	}
}

void FVulkanBoundShaderState::InitGlobalUniforms(EShaderFrequency Stage, const FVulkanShaderSerializedBindings& SerializedBindings)
{
	auto FindPackedArraySize = [&](int32 PackedTypeIndex) -> uint16
	{
		int32 Size = 0;
		for (const CrossCompiler::FPackedArrayInfo& EntryInfo: SerializedBindings.PackedGlobalArrays)
		{
			if (EntryInfo.TypeIndex == PackedTypeIndex)
			{
				return EntryInfo.Size;
			}
		}
		check(0);
		return 0;
	};

	const TArray<FVulkanShaderSerializedBindings::FBindMap>& PackedBindings = SerializedBindings.Bindings[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER];
	for (int32 Index = 0; Index < PackedBindings.Num(); ++Index)
	{
		const FVulkanShaderSerializedBindings::FBindMap& Binding = PackedBindings[Index];
		uint8 TypeIndex = SerializedBindings.PackedUBTypeIndex[Index];
		check(TypeIndex != (uint8)-1);
		DirtyPackedUniformBufferStagingMask[Stage] |= (1 << TypeIndex);
		check(PackedUniformBufferStaging[Stage][TypeIndex].Num() == 0);
		int32 PackedArraySize = FindPackedArraySize(TypeIndex);
		PackedUniformBufferStaging[Stage][TypeIndex].AddZeroed(PackedArraySize);
	}

	DirtyPackedUniformBufferStaging[Stage] = DirtyPackedUniformBufferStagingMask[Stage];
}

void FVulkanBoundShaderState::GenerateLayoutBindings()
{
	GenerateLayoutBindings(SF_Vertex, VertexShader->CodeHeader.SerializedBindings);

	if (HullShader)
	{
		GenerateLayoutBindings(SF_Hull, HullShader->CodeHeader.SerializedBindings);
		if (DomainShader)
		{
			GenerateLayoutBindings(SF_Domain, DomainShader->CodeHeader.SerializedBindings);
		}
	}
	if (PixelShader)
	{
		GenerateLayoutBindings(SF_Pixel, PixelShader->CodeHeader.SerializedBindings);
	}
	if (GeometryShader)
	{
		GenerateLayoutBindings(SF_Geometry, GeometryShader->CodeHeader.SerializedBindings);
	}

	Layout->Compile();

	GenerateVertexInputStateInfo();
}

void FVulkanBoundShaderState::GenerateLayoutBindings(EShaderFrequency Stage, const FVulkanShaderSerializedBindings& SerializedBindings)
{
	VkShaderStageFlagBits StageFlags = UEFrequencyToVKStageBit(Stage);

	//#todo-rco: Mobile assumption!
	EDescriptorSetStage DescSet = GetDescriptorSetForStage(Stage);
	int32 DescriptorSetIndex = (int32)DescSet;

	auto LoopPerType = [&](FVulkanShaderSerializedBindings::EBindingType BindingType, VkDescriptorType DescriptorType)
		{
			const TArray<FVulkanShaderSerializedBindings::FBindMap>& BindingsPerType = SerializedBindings.Bindings[BindingType];
			for (int32 Index = 0; Index < BindingsPerType.Num(); ++Index)
			{
				VkDescriptorSetLayoutBinding Binding;
				FMemory::Memzero(Binding);
				Binding.binding = BindingsPerType[Index].VulkanBindingIndex;
				Binding.descriptorType = DescriptorType;
				Binding.descriptorCount = 1;
				Binding.stageFlags = StageFlags;

				Layout->AddDescriptor(DescriptorSetIndex, Binding, BindingsPerType[Index].VulkanBindingIndex);
			}
		};

	LoopPerType(FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	LoopPerType(FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
	LoopPerType(FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	LoopPerType(FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void FVulkanBoundShaderState::CreateDescriptorWriteInfo(uint32 InUniformCombinedSamplerCount, uint32 InUniformSamplerBufferCount, uint32 InUniformBufferCount)
{
	check(DescriptorWrites.Num() == 0);
	DescriptorWrites.AddZeroed(InUniformCombinedSamplerCount + InUniformSamplerBufferCount + InUniformBufferCount);
	DescriptorImageInfo.AddZeroed(InUniformCombinedSamplerCount);
	DescriptorBufferInfo.AddZeroed(InUniformBufferCount);

	for (uint32 Index = 0; Index < InUniformCombinedSamplerCount; ++Index)
	{
		VkDescriptorImageInfo& Sampler = DescriptorImageInfo[Index];
		// Texture.Load() still requires a default sampler...
		Sampler.sampler = Device->GetDefaultSampler();
		Sampler.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");
	int32 WriteIndex = 0;
	int32 ImageIndex = 0;
	int32 BufferIndex = 0;
	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		if (StageShader->GetNumDescriptors() == 0)
		{
			continue;
		}

		//#todo-rco: Use BindingTable instead...
		auto& Bindings = StageShader->CodeHeader.SerializedBindings.Bindings;

		if (InUniformCombinedSamplerCount)
		{
			DescriptorSamplerImageInfoForStage[Stage] = DescriptorImageInfo.GetData() + ImageIndex;

			for (int32 Index = 0; Index < Bindings[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER].Num(); ++Index)
			{
				FVulkanShaderSerializedBindings::FBindMap& Mapping = Bindings[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER][Index];
				VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
				WriteDesc->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				WriteDesc->dstBinding = Mapping.VulkanBindingIndex;
				WriteDesc->descriptorCount = 1;
				WriteDesc->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				VkDescriptorImageInfo* TextureInfo = &DescriptorImageInfo[ImageIndex++];
				WriteDesc->pImageInfo = TextureInfo;
				//#todo-rco: FIX! SamplerBuffers share numbering with Samplers
				SRVWriteInfoForStage[Stage].Add(WriteDesc);
			}
		}

		for (int32 Index = 0; Index < Bindings[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER].Num(); ++Index)
		{
			FVulkanShaderSerializedBindings::FBindMap& Mapping = Bindings[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER][Index];
			VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
			WriteDesc->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			WriteDesc->dstBinding = Mapping.VulkanBindingIndex;
			WriteDesc->descriptorCount = 1;
			WriteDesc->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
			SRVWriteInfoForStage[Stage].Add(WriteDesc);
		}

		if (InUniformBufferCount)
		{
			DescriptorBufferInfoForStage[Stage] = DescriptorBufferInfo.GetData() + BufferIndex;

			for (int32 Index = 0; Index < Bindings[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER].Num(); ++Index)
			{
				FVulkanShaderSerializedBindings::FBindMap& Mapping = Bindings[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER][Index];
				VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
				WriteDesc->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				WriteDesc->dstBinding = Mapping.VulkanBindingIndex;
				WriteDesc->descriptorCount = 1;
				WriteDesc->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				VkDescriptorBufferInfo* BufferInfo = &DescriptorBufferInfo[BufferIndex++];
				WriteDesc->pBufferInfo = BufferInfo;
			}

			DescriptorBufferInfoForPackedUBForStage[Stage] = DescriptorBufferInfo.GetData() + BufferIndex;
			for (int32 Index = 0; Index < Bindings[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER].Num(); ++Index)
			{
				FVulkanShaderSerializedBindings::FBindMap& Mapping = Bindings[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER][Index];
				VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
				WriteDesc->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				WriteDesc->dstBinding = Mapping.VulkanBindingIndex;
				WriteDesc->descriptorCount = 1;
				WriteDesc->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				VkDescriptorBufferInfo* BufferInfo = &DescriptorBufferInfo[BufferIndex++];
				WriteDesc->pBufferInfo = BufferInfo;
			}
		}
	}

	check(InUniformCombinedSamplerCount + InUniformSamplerBufferCount + InUniformBufferCount == DescriptorWrites.Num());
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

	FVulkanPendingState::FVertexStream* Streams = (FVulkanPendingState::FVertexStream*)InVertexStreams;

	Tmp.VertexBuffers.Reset(0);
	Tmp.VertexOffsets.Reset(0);
	const VkVertexInputAttributeDescription* CurrAttribute = nullptr;
	for (uint32 BindingIndex = 0; BindingIndex < BindingsNum; BindingIndex++)
	{
		const VkVertexInputBindingDescription& CurrBinding = VertexInputBindings[BindingIndex];

		uint32 StreamIndex = BindingToStream.FindChecked(BindingIndex);
		FVulkanPendingState::FVertexStream& CurrStream = Streams[StreamIndex];

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

void FVulkanBoundShaderState::SetShaderParameter(EShaderFrequency Stage, uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSetShaderParamTime);
#endif

	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	TArray<uint8>& StagingBuffer = PackedUniformBufferStaging[Stage][BufferIndex];
	check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
	FMemory::Memcpy(StagingBuffer.GetData() + ByteOffset, NewValue, NumBytes);
	DirtyPackedUniformBufferStaging[Stage] |= (1 << BufferIndex);
}

#if VULKAN_HAS_DEBUGGING_ENABLED
inline void ValidateBindingPoint(const FVulkanShader& InShader, const uint32 InBindingPoint, const uint8 InSubType)
{
#if 0
	const TArray<FVulkanShaderSerializedBindings::FBindMap>& BindingLayout = InShader.CodeHeader.SerializedBindings.Bindings;
	bool bFound = false;

	for(const auto& Binding : BindingLayout)
	{
		const bool bIsPackedUniform =	InSubType == CrossCompiler::PACKED_TYPENAME_HIGHP
									||	InSubType == CrossCompiler::PACKED_TYPENAME_MEDIUMP
									||	InSubType == CrossCompiler::PACKED_TYPENAME_LOWP;

		if(Binding.EngineBindingIndex == InBindingPoint &&
			bIsPackedUniform ? (Binding.Type == FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER) : (Binding.SubType == InSubType)
			)
		{
			bFound = true;
			break;
		}
	}

	if(!bFound)
	{
		FString SubTypeName = "UNDEFINED";

		switch(InSubType)
		{
		case CrossCompiler::PACKED_TYPENAME_HIGHP: SubTypeName		= "HIGH PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_MEDIUMP: SubTypeName	= "MEDIUM PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_LOWP: SubTypeName		= "LOW PRECISION UNIFORM PACKED BUFFER";	break;
		case CrossCompiler::PACKED_TYPENAME_INT: SubTypeName		= "INT UNIFORM PACKED BUFFER";				break;
		case CrossCompiler::PACKED_TYPENAME_UINT: SubTypeName		= "UINT UNIFORM PACKED BUFFER";				break;
		case CrossCompiler::PACKED_TYPENAME_SAMPLER: SubTypeName	= "SAMPLER";								break;
		case CrossCompiler::PACKED_TYPENAME_IMAGE: SubTypeName		= "IMAGE";									break;
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

void FVulkanBoundShaderState::SetTexture(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureBase* VulkanTextureBase)
{
	// Validate BindPoint
	#if VULKAN_HAS_DEBUGGING_ENABLED
		// Vulkan implementation shares the same binding-point for a texture and a sampler
		ValidateBindingPoint(GetShader(Stage), BindPoint, CrossCompiler::PACKED_TYPENAME_SAMPLER);
	#endif

	check(VulkanTextureBase);

	FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
	uint32 VulkanBindingPoint = StageShader->GetBindingTable().CombinedSamplerBindingIndices[BindPoint];
	DescriptorSamplerImageInfoForStage[Stage][VulkanBindingPoint].imageView = VulkanTextureBase ? VulkanTextureBase->DefaultView.View : VK_NULL_HANDLE;

	DirtyTextures[Stage] = true;
#if VULKAN_ENABLE_RHI_DEBUGGING
	DebugInfo.Textures[Stage][BindPoint] = VulkanTextureBase;
#endif
}

void FVulkanBoundShaderState::SetSamplerState(EShaderFrequency Stage, uint32 BindPoint, FVulkanSamplerState* Sampler)
{
	// Validate BindPoint
	#if VULKAN_HAS_DEBUGGING_ENABLED
		// Vulkan implementation shares the same binding-point for a texture and a sampler
		ValidateBindingPoint(GetShader(Stage), BindPoint, CrossCompiler::PACKED_TYPENAME_SAMPLER);
	#endif

	FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
	uint32 VulkanBindingPoint = StageShader->GetBindingTable().CombinedSamplerBindingIndices[BindPoint];
	DescriptorSamplerImageInfoForStage[Stage][VulkanBindingPoint].sampler = Sampler ? Sampler->Sampler : VK_NULL_HANDLE;

	DirtySamplerStates[Stage] = true;
#if VULKAN_ENABLE_RHI_DEBUGGING
	DebugInfo.SamplerStates[Stage][BindPoint] = Sampler;
#endif
}

void FVulkanBoundShaderState::SetTextureView(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureView& TextureView)
{
	FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
	uint32 VulkanBindingPoint = StageShader->GetBindingTable().CombinedSamplerBindingIndices[BindPoint];
	DescriptorSamplerImageInfoForStage[Stage][VulkanBindingPoint].imageView = TextureView.View;

	DirtySamplerStates[Stage] = true;
#if VULKAN_ENABLE_RHI_DEBUGGING
	DebugInfo.SRVIs[Stage][BindPoint] = &TextureView;
#endif
}

void FVulkanBoundShaderState::SetBufferViewState(EShaderFrequency Stage, uint32 BindPoint, FVulkanBufferView* View)
{
	// Validate BindPoint
	#if VULKAN_HAS_DEBUGGING_ENABLED
		// Vulkan implementation shares the same binding-point for a texture and a sampler
		ValidateBindingPoint(GetShader(Stage), BindPoint, CrossCompiler::PACKED_TYPENAME_SAMPLER);
	#endif


	FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
	uint32 VulkanBindingPoint = StageShader->GetBindingTable().SamplerBufferBindingIndices[BindPoint];
	check(!View || (View->Flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));
	SRVWriteInfoForStage[Stage][VulkanBindingPoint]->pTexelBufferView = View ? &View->View : VK_NULL_HANDLE;

	DirtySRVs[Stage] = true;
#if VULKAN_ENABLE_RHI_DEBUGGING
	DebugInfo.SRVBs[Stage][BindPoint] = View;
#endif
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
		SetBufferViewState(Stage, TextureIndex, nullptr);
	}
}

void FVulkanBoundShaderState::SetUniformBufferConstantData(FVulkanPendingState& PendingState, EShaderFrequency Stage, uint32 BindPoint, const TArray<uint8>& ConstantData)
{
	FVulkanShader* Shader = GetShaderPtr(Stage);

	// Emulated UBs
	for (const CrossCompiler::FUniformBufferCopyInfo& CopyInfo : Shader->CodeHeader.UniformBuffersCopyInfo)
	{
		if (CopyInfo.SourceUBIndex == BindPoint)
		{
			TArray<uint8>& StagingBuffer = PackedUniformBufferStaging[Stage][CopyInfo.DestUBTypeIndex];
			//check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
			FMemory::Memcpy(StagingBuffer.GetData() + CopyInfo.DestOffsetInFloats * 4, ConstantData.GetData() + CopyInfo.SourceOffsetInFloats * 4, CopyInfo.SizeInFloats * 4);
			DirtyPackedUniformBufferStaging[Stage] |= (1 << CopyInfo.DestUBTypeIndex);
		}
	}
}

void FVulkanBoundShaderState::SetUniformBuffer(FVulkanPendingState& PendingState, EShaderFrequency Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
{
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
}

void FVulkanBoundShaderState::GetDescriptorInfoCounts(uint32& OutCombinedSamplerCount, uint32& OutSamplerBufferCount, uint32& OutUniformBufferCount)
{
	OutCombinedSamplerCount = 0;
	OutSamplerBufferCount = 0;
	OutUniformBufferCount = 0;

	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
		if (StageShader)
		{
			if (StageShader->GetNumDescriptors() > 0)
			{
				auto& Layouts = StageShader->CodeHeader.SerializedBindings.Bindings;

				OutCombinedSamplerCount += Layouts[FVulkanShaderSerializedBindings::TYPE_COMBINED_IMAGE_SAMPLER].Num();
				OutSamplerBufferCount += Layouts[FVulkanShaderSerializedBindings::TYPE_SAMPLER_BUFFER].Num();
				OutUniformBufferCount += Layouts[FVulkanShaderSerializedBindings::TYPE_PACKED_UNIFORM_BUFFER].Num() + Layouts[FVulkanShaderSerializedBindings::TYPE_UNIFORM_BUFFER].Num();

				DirtyTextures[Stage] = true;
				DirtySamplerStates[Stage] = true;
				DirtySRVs[Stage] = true;
				DirtyPackedUniformBufferStaging[Stage] = DirtyPackedUniformBufferStagingMask[Stage];
			}
		}
	}
}

FVulkanBoundShaderState::FDescriptorSetsPair::~FDescriptorSetsPair()
{
	delete DescriptorSets;
}

inline FVulkanDescriptorSets* FVulkanBoundShaderState::RequestDescriptorSets(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer)
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
	NewEntry->DescriptorSets = new FVulkanDescriptorSets(Device, this, Context);
	NewEntry->FenceCounter = CmdBufferFenceSignaledCounter;
	return NewEntry->DescriptorSets;
}


void FVulkanBoundShaderState::UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

    check(GlobalUniformPool);

	FVulkanPendingState& State = CmdListContext->GetPendingState();

	int32 WriteIndex = 0;

	CurrDescriptorSets = RequestDescriptorSets(CmdListContext, CmdBuffer);

	const TArray<VkDescriptorSet>& DescriptorSetHandles = CurrDescriptorSets->GetHandles();
	int32 DescriptorSetIndex = 0;

#if VULKAN_USE_RING_BUFFER_FOR_GLOBAL_UBS
	// this is an optimization for the ring buffer to only truly lock once for all uniforms
	FVulkanRingBuffer* RingBuffer = Device->GetUBRingBuffer();
	uint8* RingBufferBase = (uint8*)RingBuffer->GetMappedPointer();
#endif

	//#todo-rco: Compute!
	static_assert(SF_Geometry + 1 == SF_Compute, "Loop assumes compute is after gfx stages!");
	for (uint32 Stage = 0; Stage < SF_Compute; Stage++)
	{
		FVulkanShader* StageShader = GetShaderPtr((EShaderFrequency)Stage);
		if (!StageShader)
		{
			continue;
		}

		const uint32 NumDescriptorsForStage = StageShader->GetNumDescriptors();
		if (NumDescriptorsForStage == 0)
		{
			continue;
		}

		const VkDescriptorSet DescriptorSet = DescriptorSetHandles[DescriptorSetIndex];
		++DescriptorSetIndex;

		bool bRequiresNonUBUpdate = DirtySamplerStates[Stage] || DirtySRVs[Stage] || DirtyTextures[Stage];
		bool bRequiresUBUpdate = (DirtyPackedUniformBufferStaging[Stage] != 0);
		if (!bRequiresNonUBUpdate && !bRequiresUBUpdate)
		{
			//#todo-rco: Skip this desc set writes and only call update for the modified ones!
			//continue;
			int x = 0;
		}

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
			uint32 RemainingGlobalUniformMask = DirtyPackedUniformBufferStagingMask[Stage];
			VkDescriptorBufferInfo* BufferInfo = DescriptorBufferInfoForPackedUBForStage[Stage];
			TArray<uint8>* PackedUniformBuffer = &PackedUniformBufferStaging[Stage][0];
			while (RemainingGlobalUniformMask)
			{
				if (RemainingGlobalUniformMask & 1)
				{
					//Get a uniform buffer from the dynamic pool
					int32 UBSize = PackedUniformBuffer->Num();

#if VULKAN_USE_RING_BUFFER_FOR_GLOBAL_UBS
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

#else
					FVulkanPooledUniformBuffer* GlobalUniformBuffer = GlobalUniformPool->GetGlobalUniformBufferFromPool(*Device, UBSize).GetReference();
					FVulkanBuffer& CurrentBuffer = GlobalUniformBuffer->Buffer;

					void* BufferPtr = CurrentBuffer.Lock(UBSize);
					FMemory::Memcpy(BufferPtr, PackedUniformBuffer->GetData(), UBSize);
					CurrentBuffer.Unlock();

					// Here we can specify a more precise buffer update
					// However, this need to complemented with the buffer map/unmap functionality.
					//@NOTE: bufferView is for texel buffers
					BufferInfo->buffer = CurrentBuffer.GetBufferHandle();
					BufferInfo->range = UBSize;

#endif

					VkWriteDescriptorSet* WriteDesc = &DescriptorWrites[WriteIndex++];
					WriteDesc->dstSet = DescriptorSet;

					check(WriteDesc->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
					++BufferInfo;
				}
				RemainingGlobalUniformMask = RemainingGlobalUniformMask >> 1;
				++PackedUniformBuffer;
			}
		}
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

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanClearDirtyDSState);
#endif
		FMemory::Memzero(DirtyTextures);
		FMemory::Memzero(DirtySamplerStates);
		FMemory::Memzero(DirtySRVs);
		FMemory::Memzero(DirtyPackedUniformBufferStaging);
	}
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

FVulkanDescriptorPool* FVulkanCommandListContext::AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, VkDescriptorSet* OutSets)
{
	FVulkanDescriptorPool* Pool = DescriptorPools.Last();
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = InDescriptorSetAllocateInfo;
	DescriptorSetAllocateInfo.descriptorPool = Pool->GetHandle();

	VkResult Result = VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets);

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
