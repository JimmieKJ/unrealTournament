// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipeline.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipeline.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

static const double HitchTime = 2.0 / 60.0;

void FVulkanGfxPipelineState::Reset()
{
	BSS = nullptr;
	RenderPass = nullptr;
	BlendState = nullptr;
	DepthStencilState = nullptr;
	RasterizerState = nullptr;

	//FrameBuffer = nullptr;

	FMemory::Memzero(Scissor);
	FMemory::Memzero(Viewport);
	bNeedsScissorUpdate = bNeedsViewportUpdate = bNeedsStencilRefUpdate = true;
	StencilRef = PrevStencilRef = 0;
	InitializeDefaultStates();
}

void FVulkanGfxPipelineState::InitializeDefaultStates()
{
	// Input Assembly
	FMemory::Memzero(InputAssembly);
	InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	
	// Setup dynamic state
	FMemory::Memzero(DynamicState);
	DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	DynamicState.pDynamicStates = DynamicStatesEnabled;

	// Enable states
	FMemory::Memzero(DynamicStatesEnabled);
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
}

FVulkanPipeline::FVulkanPipeline(FVulkanDevice* InDevice)
	: Device(InDevice)
	, Pipeline(VK_NULL_HANDLE)
#if !VULKAN_ENABLE_PIPELINE_CACHE
	, PipelineCache(VK_NULL_HANDLE)
#endif
{
}

FVulkanPipeline::~FVulkanPipeline()
{
#if VULKAN_ENABLE_PIPELINE_CACHE
	Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Pipeline, Pipeline);
	Pipeline = VK_NULL_HANDLE;
#endif
}

FVulkanComputePipeline::FVulkanComputePipeline(FVulkanDevice* InDevice, FVulkanComputeShaderState* CSS)
	: FVulkanPipeline(InDevice)
{
	//#todo-rco: Move to shader pipeline
	VkComputePipelineCreateInfo PipelineInfo;
	FMemory::Memzero(PipelineInfo);
	PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	PipelineInfo.stage.module = CSS->ComputeShader->GetHandle();
	PipelineInfo.stage.pName = "main";
	PipelineInfo.layout = CSS->GetPipelineLayout();
	VERIFYVULKANRESULT(VulkanRHI::vkCreateComputePipelines(Device->GetInstanceHandle(), VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline));
}

#if !VULKAN_ENABLE_PIPELINE_CACHE
void FVulkanGfxPipeline::Create(const FVulkanGfxPipelineState& State)
{
	//SCOPE_CYCLE_COUNTER(STAT_VulkanCreatePipeline);

	// Pipeline
	VkGraphicsPipelineCreateInfo PipelineInfo;
	FMemory::Memzero(PipelineInfo);
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.layout = State.BSS->GetPipelineLayout();

	// Color Blend
	VkPipelineColorBlendStateCreateInfo CBInfo;
	FMemory::Memzero(CBInfo);
	CBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	CBInfo.attachmentCount = State.RenderPass->GetLayout().GetNumColorAttachments();
	CBInfo.pAttachments = State.BlendState->BlendStates;
	CBInfo.blendConstants[0] = 1.0f;
	CBInfo.blendConstants[1] = 1.0f;
	CBInfo.blendConstants[2] = 1.0f;
	CBInfo.blendConstants[3] = 1.0f;

	// Viewport
	VkPipelineViewportStateCreateInfo VPInfo;
	FMemory::Memzero(VPInfo);
	VPInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	VPInfo.viewportCount = 1;
	VPInfo.scissorCount = 1;

	// Multisample
	VkPipelineMultisampleStateCreateInfo MSInfo;
	FMemory::Memzero(MSInfo);
	MSInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	MSInfo.pSampleMask = NULL;
	MSInfo.rasterizationSamples = State.RenderPass->GetLayout().GetAttachmentDescriptions()[0].samples;

	// Two stages: vs and fs
	VkPipelineShaderStageCreateInfo ShaderStages[SF_NumFrequencies];
	FMemory::Memzero(ShaderStages);

    PipelineInfo.stageCount = 0;
	PipelineInfo.pStages = ShaderStages;
	for(uint32 ShaderStage = 0; ShaderStage < SF_NumFrequencies; ++ShaderStage)
	{
		const EShaderFrequency CurrStage = (EShaderFrequency)ShaderStage;
		if (!State.BSS->HasShaderStage(CurrStage))
		{
			continue;
		}
		
		ShaderStages[PipelineInfo.stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ShaderStages[PipelineInfo.stageCount].stage = UEFrequencyToVKStageBit(CurrStage);
		ShaderStages[PipelineInfo.stageCount].module = State.BSS->GetShader(CurrStage).GetHandle();
		ShaderStages[PipelineInfo.stageCount].pName = "main";
		PipelineInfo.stageCount++;
	}
	
	// Vertex Input. The structure is mandatory even without vertex attributes.
	const VkPipelineVertexInputStateCreateInfo& VBInfo = State.BSS->GetVertexInputStateInfo().GetInfo();
	PipelineInfo.pVertexInputState = &VBInfo;

	VkPipelineCacheCreateInfo PipelineCacheInfo;
	FMemory::Memzero(PipelineCacheInfo);
	PipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, nullptr, &PipelineCache));

	check(State.RenderPass);
	PipelineInfo.pColorBlendState = &CBInfo;
	PipelineInfo.pMultisampleState = &MSInfo;
	PipelineInfo.pViewportState = &VPInfo;
	PipelineInfo.renderPass = State.RenderPass->GetHandle();
	PipelineInfo.pInputAssemblyState = &State.InputAssembly;
	PipelineInfo.pRasterizationState = &State.RasterizerState->RasterizerState;
	PipelineInfo.pDepthStencilState = &State.DepthStencilState->DepthStencilState;
	PipelineInfo.pDynamicState = &State.DynamicState;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, nullptr, &Pipeline));
}

void FVulkanGfxPipeline::Destroy()
{
	if (Pipeline)
	{
		check(PipelineCache);
		VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, nullptr);
		PipelineCache = VK_NULL_HANDLE;
	}
}
#endif

void FVulkanGfxPipeline::InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd, FVulkanGfxPipelineState& State, bool bNeedsViewportUpdate, bool bNeedsScissorUpdate, bool bNeedsStencilRefUpdate, bool bCmdNeedsDynamicState)
{
	check(Device);
	check(Cmd);

	// Validate and update Viewport
	if (bNeedsViewportUpdate || bCmdNeedsDynamicState)
	{
		check(State.Viewport.width > 0);
		check(State.Viewport.height > 0);
		VulkanRHI::vkCmdSetViewport(Cmd->GetHandle(), 0, 1, &State.Viewport);

		State.bNeedsViewportUpdate = false;
	}

	// Validate and update scissor rect
	if (bNeedsScissorUpdate || bCmdNeedsDynamicState)
	{
		VkRect2D Scissor = State.Scissor;
		if (Scissor.extent.width == 0 || Scissor.extent.height == 0)
		{
			Scissor.extent.width = State.Viewport.width;
			Scissor.extent.height = State.Viewport.height;
		}

		VulkanRHI::vkCmdSetScissor(Cmd->GetHandle(), 0, 1, &Scissor);

		State.bNeedsScissorUpdate = false;
	}

	if (bNeedsStencilRefUpdate || bCmdNeedsDynamicState)
	{
		VulkanRHI::vkCmdSetStencilReference(Cmd->GetHandle(), VK_STENCIL_FRONT_AND_BACK, State.StencilRef);
		State.bNeedsStencilRefUpdate = false;
	}

	if (bCmdNeedsDynamicState)
	{
		Cmd->bNeedsDynamicStateSet = false;
	}
}

#if VULKAN_ENABLE_PIPELINE_CACHE

static TAutoConsoleVariable<int32> GEnablePipelineCacheLoadCvar(
	TEXT("r.Vulkan.PipelineCacheLoad"),
	1,
	TEXT("0 to disable loading the pipeline cache")
	TEXT("1 to enable using pipeline cache")
	);

FVulkanPipelineStateCache::FGfxPipelineEntry::~FGfxPipelineEntry()
{
	check(!bLoaded);
	check(!RenderPass);
}

FVulkanPipelineStateCache::FVulkanPipelineStateCache(FVulkanDevice* InDevice)
	: Device(InDevice)
	, PipelineCache(VK_NULL_HANDLE)
{
}

FVulkanPipelineStateCache::~FVulkanPipelineStateCache()
{
	DestroyCache();

	VulkanRHI::vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, nullptr);
	PipelineCache = VK_NULL_HANDLE;
}

bool FVulkanPipelineStateCache::Load(const TArray<FString>& CacheFilenames, TArray<uint8>& OutDeviceCache)
{
	bool bLoaded = false;
	for (const FString& CacheFilename : CacheFilenames)
	{
		TArray<uint8> MemFile;
		if (FFileHelper::LoadFileToArray(MemFile, *CacheFilename, FILEREAD_Silent))
		{
			FMemoryReader Ar(MemFile);
			int32 Version = -1;
			Ar << Version;
			if (Version != VERSION)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache '%s' due to mismatched Version %d != %d"), *CacheFilename, Version, (int32)VERSION);
				continue;
			}
			int32 SizeOfGfxEntry = -1;
			Ar << SizeOfGfxEntry;
			if (SizeOfGfxEntry != (int32)(sizeof(FGfxPipelineEntry)))
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache '%s' due to mismatched size of FGfxEntry %d != %d; forgot to bump up VERSION?"), *CacheFilename, SizeOfGfxEntry, (int32)sizeof(FGfxPipelineEntry));
				continue;
			}
			Ar << GfxPipelineEntries;

			for (int32 Index = 0; Index < GfxPipelineEntries.Num(); ++Index)
			{
				FVulkanGfxPipeline* Pipeline = new FVulkanGfxPipeline(Device);
				FGfxPipelineEntry* GfxEntry = &GfxPipelineEntries[Index];
				GfxEntry->bLoaded = true;
				CreatGfxEntryRuntimeObjects(GfxEntry);
				CreateGfxPipelineFromEntry(GfxEntry, Pipeline, nullptr);

				FVulkanGfxPipelineStateKey CreateInfo(GfxEntry->GraphicsKey, GfxEntry->VertexInputKey, GfxEntry->ShaderHashes);

				// Add to the cache
				KeyToGfxPipelineMap.Add(CreateInfo, Pipeline);
				CreatedGfxPipelines.Add(GfxEntry, Pipeline);
				Pipeline->AddRef();
			}

			Ar << OutDeviceCache;

			if (OutDeviceCache.Num() > 4)
			{
				uint32* Data = (uint32*)OutDeviceCache.GetData();
				uint32 HeaderSize = *Data++;
				if (HeaderSize == 16 + VK_UUID_SIZE)
				{
					uint32 HeaderVersion = *Data++;
					if (HeaderVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
					{
						uint32 VendorID = *Data++;
						if (VendorID == Device->GetDeviceProperties().vendorID)
						{
							uint32 DeviceID = *Data++;
							if (DeviceID == Device->GetDeviceProperties().deviceID)
							{
								uint8* Uuid = (uint8*)Data;
								if (FMemory::Memcmp(Device->GetDeviceProperties().pipelineCacheUUID, Uuid, sizeof(VK_UUID_SIZE)) == 0)
								{
									bLoaded = true;
								}
							}
						}
					}
				}
			}

			UE_LOG(LogVulkanRHI, Display, TEXT("Loaded pipeline cache file '%s', %d Pipelines, %d bytes %s"), *CacheFilename, GfxPipelineEntries.Num(), MemFile.Num(), (bLoaded ? TEXT("VkPipelineCache compatible") : TEXT("")));
			break;
		}
	}

	return bLoaded;
}


void FVulkanPipelineStateCache::DestroyPipeline(FVulkanGfxPipeline* Pipeline)
{
	if (Pipeline->Release() == 0)
	{
		const FVulkanGfxPipelineStateKey* Key = KeyToGfxPipelineMap.FindKey(Pipeline);
		check(Key);
		KeyToGfxPipelineMap.Remove(*Key);
	}
}

void FVulkanPipelineStateCache::InitAndLoad(const TArray<FString>& CacheFilenames)
{
	TArray<uint8> DeviceCache;

	bool bLoaded = false;
	if (GEnablePipelineCacheLoadCvar.GetValueOnAnyThread() == 0)
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Not loading pipeline cache per r.Vulkan.PipelineCacheLoad=0"));
	}
	else
	{
		bLoaded = Load(CacheFilenames, DeviceCache);
	}

	VkPipelineCacheCreateInfo PipelineCacheInfo;
	FMemory::Memzero(PipelineCacheInfo);
	PipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	PipelineCacheInfo.initialDataSize = bLoaded ? DeviceCache.Num() : 0;
	PipelineCacheInfo.pInitialData = bLoaded ? DeviceCache.GetData() : 0;
	VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineCache(Device->GetInstanceHandle(), &PipelineCacheInfo, nullptr, &PipelineCache));
}

void FVulkanPipelineStateCache::Save(FString& CacheFilename)
{
	TArray<uint8> MemFile;
	FMemoryWriter Ar(MemFile);
	int32 Version = VERSION;
	Ar << Version;
	int32 SizeOfGfxEntry = (int32)sizeof(FGfxPipelineEntry);
	Ar << SizeOfGfxEntry;
	Ar << GfxPipelineEntries;
	TArray<uint8> DeviceCache;
	size_t Size = 0;
	VERIFYVULKANRESULT(VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &Size, nullptr));
	if (Size > 0)
	{
		DeviceCache.AddUninitialized(Size);
		VERIFYVULKANRESULT(VulkanRHI::vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &Size, DeviceCache.GetData()));
	}
	Ar << DeviceCache;

	MemFile.Num();

	if (FFileHelper::SaveArrayToFile(MemFile, *CacheFilename))
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Saved pipeline cache file '%s', %d Pipelines, %d bytes"), *CacheFilename, GfxPipelineEntries.Num(), MemFile.Num());
	}
}

void FVulkanPipelineStateCache::CreateAndAdd(FVulkanRenderPass* RenderPass, const FVulkanGfxPipelineStateKey& CreateInfo, FVulkanGfxPipeline* Pipeline, const FVulkanGfxPipelineState& State, const FVulkanBoundShaderState& BSS)
{
	//if (EnablePipelineCacheCvar.GetValueOnRenderThread() == 1)
	//SCOPE_CYCLE_COUNTER(STAT_VulkanCreatePipeline);

	FGfxPipelineEntry* GfxEntry = new FGfxPipelineEntry();
	GfxEntry->GraphicsKey = CreateInfo.PipelineKey;
	GfxEntry->VertexInputKey = CreateInfo.VertexInputKey;
	PopulateGfxEntry(State, RenderPass, GfxEntry);

	// Create the pipeline
	CreateGfxPipelineFromEntry(GfxEntry, Pipeline, &BSS);

	//UE_LOG(LogVulkanRHI, Display, TEXT("PK: Added Entry %llx Index %d"), CreateInfo.PipelineKey, GfxEntries.Num());

	// Add to the cache
	KeyToGfxPipelineMap.Add(CreateInfo, Pipeline);
	GfxPipelineEntries.Add(GfxEntry);
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FBlendAttachment& Attachment)
{
	// Modify VERSION if serialization changes
	Ar << Attachment.bBlend;
	Ar << Attachment.ColorBlendOp;
	Ar << Attachment.SrcColorBlendFactor;
	Ar << Attachment.DstColorBlendFactor;
	Ar << Attachment.AlphaBlendOp;
	Ar << Attachment.SrcAlphaBlendFactor;
	Ar << Attachment.DstAlphaBlendFactor;
	Ar << Attachment.ColorWriteMask;
	return Ar;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FBlendAttachment::ReadFrom(const VkPipelineColorBlendAttachmentState& InState)
{
	bBlend =				InState.blendEnable != VK_FALSE;
	ColorBlendOp =			(uint8)InState.colorBlendOp;
	SrcColorBlendFactor =	(uint8)InState.srcColorBlendFactor;
	DstColorBlendFactor =	(uint8)InState.dstColorBlendFactor;
	AlphaBlendOp =			(uint8)InState.alphaBlendOp;
	SrcAlphaBlendFactor =	(uint8)InState.srcAlphaBlendFactor;
	DstAlphaBlendFactor =	(uint8)InState.dstAlphaBlendFactor;
	ColorWriteMask =		(uint8)InState.colorWriteMask;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FBlendAttachment::WriteInto(VkPipelineColorBlendAttachmentState& Out) const
{
	Out.blendEnable =			bBlend ? VK_TRUE : VK_FALSE;
	Out.colorBlendOp =			(VkBlendOp)ColorBlendOp;
	Out.srcColorBlendFactor =	(VkBlendFactor)SrcColorBlendFactor;
	Out.dstColorBlendFactor =	(VkBlendFactor)DstColorBlendFactor;
	Out.alphaBlendOp =			(VkBlendOp)AlphaBlendOp;
	Out.srcAlphaBlendFactor =	(VkBlendFactor)SrcAlphaBlendFactor;
	Out.dstAlphaBlendFactor =	(VkBlendFactor)DstAlphaBlendFactor;
	Out.colorWriteMask =		(VkColorComponentFlags)ColorWriteMask;
}


void FVulkanPipelineStateCache::FDescriptorSetLayoutBinding::ReadFrom(const VkDescriptorSetLayoutBinding& InState)
{
	Binding =			InState.binding;
	DescriptorCount =	InState.descriptorCount;
	DescriptorType =	InState.descriptorType;
	StageFlags =		InState.stageFlags;
}

void FVulkanPipelineStateCache::FDescriptorSetLayoutBinding::WriteInto(VkDescriptorSetLayoutBinding& Out) const
{
	Out.binding			= Binding;
	Out.descriptorCount	= DescriptorCount;
	Out.descriptorType	= (VkDescriptorType)DescriptorType;
	Out.stageFlags		= StageFlags;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDescriptorSetLayoutBinding& Binding)
{
	// Modify VERSION if serialization changes
	Ar << Binding.Binding;
	Ar << Binding.DescriptorCount;
	Ar << Binding.DescriptorType;
	Ar << Binding.StageFlags;
	return Ar;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FVertexBinding::ReadFrom(const VkVertexInputBindingDescription& InState)
{
	Binding =	InState.binding;
	InputRate =	(uint16)InState.inputRate;
	Stride =	InState.stride;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FVertexBinding::WriteInto(VkVertexInputBindingDescription& Out) const
{
	Out.binding =	Binding;
	Out.inputRate =	(VkVertexInputRate)InputRate;
	Out.stride =	Stride;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FVertexBinding& Binding)
{
	// Modify VERSION if serialization changes
	Ar << Binding.Stride;
	Ar << Binding.Binding;
	Ar << Binding.InputRate;
	return Ar;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FVertexAttribute::ReadFrom(const VkVertexInputAttributeDescription& InState)
{
	Binding =	InState.binding;
	Format =	(uint32)InState.format;
	Location =	InState.location;
	Offset =	InState.offset;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FVertexAttribute::WriteInto(VkVertexInputAttributeDescription& Out) const
{
	Out.binding =	Binding;
	Out.format =	(VkFormat)Format;
	Out.location =	Location;
	Out.offset =	Offset;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FVertexAttribute& Attribute)
{
	// Modify VERSION if serialization changes
	Ar << Attribute.Location;
	Ar << Attribute.Binding;
	Ar << Attribute.Format;
	Ar << Attribute.Offset;
	return Ar;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRasterizer::ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState)
{
	PolygonMode =				InState.polygonMode;
	CullMode =					InState.cullMode;
	DepthBiasSlopeScale =		InState.depthBiasSlopeFactor;
	DepthBiasConstantFactor =	InState.depthBiasConstantFactor;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRasterizer::WriteInto(VkPipelineRasterizationStateCreateInfo& Out) const
{
	Out.polygonMode =				(VkPolygonMode)PolygonMode;
	Out.cullMode =					(VkCullModeFlags)CullMode;
	Out.frontFace =					VK_FRONT_FACE_CLOCKWISE;
	Out.depthClampEnable =			VK_FALSE;
	Out.depthBiasEnable =			DepthBiasConstantFactor != 0.0f ? VK_TRUE : VK_FALSE;
	Out.rasterizerDiscardEnable =	VK_FALSE;
	Out.depthBiasSlopeFactor =		DepthBiasSlopeScale;
	Out.depthBiasConstantFactor =	DepthBiasConstantFactor;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FRasterizer& Rasterizer)
{
	// Modify VERSION if serialization changes
	Ar << Rasterizer.PolygonMode;
	Ar << Rasterizer.CullMode;
	Ar << Rasterizer.DepthBiasSlopeScale;
	Ar << Rasterizer.DepthBiasConstantFactor;
	return Ar;
}


void FVulkanPipelineStateCache::FGfxPipelineEntry::FDepthStencil::ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState)
{
	DepthCompareOp =		(uint8)InState.depthCompareOp;
	bDepthTestEnable =		InState.depthTestEnable != VK_FALSE;
	bDepthWriteEnable =		InState.depthWriteEnable != VK_FALSE;
	bStencilTestEnable =	InState.stencilTestEnable != VK_FALSE;
	FrontFailOp =			(uint8)InState.front.failOp;
	FrontPassOp =			(uint8)InState.front.passOp;
	FrontDepthFailOp =		(uint8)InState.front.depthFailOp;
	FrontCompareOp =		(uint8)InState.front.compareOp;
	FrontCompareMask =		(uint8)InState.front.compareMask;
	FrontWriteMask =		InState.front.writeMask;
	FrontReference =		InState.front.reference;
	BackFailOp =			(uint8)InState.back.failOp;
	BackPassOp =			(uint8)InState.back.passOp;
	BackDepthFailOp =		(uint8)InState.back.depthFailOp;
	BackCompareOp =			(uint8)InState.back.compareOp;
	BackCompareMask =		(uint8)InState.back.compareMask;
	BackWriteMask =			InState.back.writeMask;
	BackReference =			InState.back.reference;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FDepthStencil::WriteInto(VkPipelineDepthStencilStateCreateInfo& Out) const
{
	Out.depthCompareOp =		(VkCompareOp)DepthCompareOp;
	Out.depthTestEnable =		bDepthTestEnable;
	Out.depthWriteEnable =		bDepthWriteEnable;
	Out.depthBoundsTestEnable =	VK_FALSE;	// What is this?
	Out.minDepthBounds =		0;
	Out.maxDepthBounds =		0;
	Out.stencilTestEnable =		bStencilTestEnable;
	Out.front.failOp =			(VkStencilOp)FrontFailOp;
	Out.front.passOp =			(VkStencilOp)FrontPassOp;
	Out.front.depthFailOp =		(VkStencilOp)FrontDepthFailOp;
	Out.front.compareOp =		(VkCompareOp)FrontCompareOp;
	Out.front.compareMask =		FrontCompareMask;
	Out.front.writeMask =		FrontWriteMask;
	Out.front.reference =		FrontReference;
	Out.back.failOp =			(VkStencilOp)BackFailOp;
	Out.back.passOp =			(VkStencilOp)BackPassOp;
	Out.back.depthFailOp =		(VkStencilOp)BackDepthFailOp;
	Out.back.compareOp =		(VkCompareOp)BackCompareOp;
	Out.back.writeMask =		BackWriteMask;
	Out.back.compareMask =		BackCompareMask;
	Out.back.reference =		BackReference;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FDepthStencil& DepthStencil)
{
	// Modify VERSION if serialization changes
	Ar << DepthStencil.DepthCompareOp;
	Ar << DepthStencil.bDepthTestEnable;
	Ar << DepthStencil.bDepthWriteEnable;
	Ar << DepthStencil.bStencilTestEnable;
	Ar << DepthStencil.FrontFailOp;
	Ar << DepthStencil.FrontPassOp;
	Ar << DepthStencil.FrontDepthFailOp;
	Ar << DepthStencil.FrontCompareOp;
	Ar << DepthStencil.FrontCompareMask;
	Ar << DepthStencil.FrontWriteMask;
	Ar << DepthStencil.FrontReference;
	Ar << DepthStencil.BackFailOp;
	Ar << DepthStencil.BackPassOp;
	Ar << DepthStencil.BackDepthFailOp;
	Ar << DepthStencil.BackCompareOp;
	Ar << DepthStencil.BackCompareMask;
	Ar << DepthStencil.BackWriteMask;
	Ar << DepthStencil.BackReference;
	return Ar;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::FAttachmentRef::ReadFrom(const VkAttachmentReference& InState)
{
	Attachment =	InState.attachment;
	Layout =		(uint64)InState.layout;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::FAttachmentRef::WriteInto(VkAttachmentReference& Out) const
{
	Out.attachment =	Attachment;
	Out.layout =		(VkImageLayout)Layout;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::FAttachmentRef& AttachmentRef)
{
	// Modify VERSION if serialization changes
	Ar << AttachmentRef.Attachment;
	Ar << AttachmentRef.Layout;
	return Ar;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::FAttachmentDesc::ReadFrom(const VkAttachmentDescription &InState)
{
	Format =			(uint32)InState.format;
	Flags =				(uint8)InState.flags;
	Samples =			(uint8)InState.samples;
	LoadOp =			(uint8)InState.loadOp;
	StoreOp =			(uint8)InState.storeOp;
	StencilLoadOp =		(uint8)InState.stencilLoadOp;
	StencilStoreOp =	(uint8)InState.stencilStoreOp;
	InitialLayout =		(uint64)InState.initialLayout;
	FinalLayout =		(uint64)InState.finalLayout;
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::FAttachmentDesc::WriteInto(VkAttachmentDescription& Out) const
{
	Out.format =			(VkFormat)Format;
	Out.flags =				Flags;
	Out.samples =			(VkSampleCountFlagBits)Samples;
	Out.loadOp =			(VkAttachmentLoadOp)LoadOp;
	Out.storeOp =			(VkAttachmentStoreOp)StoreOp;
	Out.stencilLoadOp =		(VkAttachmentLoadOp)StencilLoadOp;
	Out.stencilStoreOp =	(VkAttachmentStoreOp)StencilStoreOp;
	Out.initialLayout =		(VkImageLayout)InitialLayout;
	Out.finalLayout =		(VkImageLayout)FinalLayout;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::FAttachmentDesc& AttachmentDesc)
{
	// Modify VERSION if serialization changes
	Ar << AttachmentDesc.Format;
	Ar << AttachmentDesc.Flags;
	Ar << AttachmentDesc.Samples;
	Ar << AttachmentDesc.LoadOp;
	Ar << AttachmentDesc.StoreOp;
	Ar << AttachmentDesc.StencilLoadOp;
	Ar << AttachmentDesc.StencilStoreOp;
	Ar << AttachmentDesc.InitialLayout;
	Ar << AttachmentDesc.FinalLayout;

	return Ar;
}


void FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::ReadFrom(const FVulkanRenderTargetLayout& RTLayout)
{
	NumAttachments =			RTLayout.NumAttachments;
	NumColorAttachments =		RTLayout.NumColorAttachments;

	bHasDepthStencil =			RTLayout.bHasDepthStencil;
	bHasResolveAttachments =	RTLayout.bHasResolveAttachments;
	Hash =						RTLayout.Hash;

	Extent3D.X = RTLayout.Extent.Extent3D.width;
	Extent3D.Y = RTLayout.Extent.Extent3D.height;
	Extent3D.Z = RTLayout.Extent.Extent3D.depth;

	auto CopyAttachmentRefs = [&](TArray<FGfxPipelineEntry::FRenderTargets::FAttachmentRef>& Dest, const VkAttachmentReference* Source, uint32 Count)
	{
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FGfxPipelineEntry::FRenderTargets::FAttachmentRef* New = new(Dest) FGfxPipelineEntry::FRenderTargets::FAttachmentRef;
			New->ReadFrom(Source[Index]);
		}
	};
	CopyAttachmentRefs(ColorAttachments, RTLayout.ColorReferences, ARRAY_COUNT(RTLayout.ColorReferences));
	CopyAttachmentRefs(ResolveAttachments, RTLayout.ResolveReferences, ARRAY_COUNT(RTLayout.ResolveReferences));
	DepthStencil.ReadFrom(RTLayout.DepthStencilReference);

	Descriptions.AddZeroed(ARRAY_COUNT(RTLayout.Desc));
	for (int32 Index = 0; Index < ARRAY_COUNT(RTLayout.Desc); ++Index)
	{
		Descriptions[Index].ReadFrom(RTLayout.Desc[Index]);
	}
}

void FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets::WriteInto(FVulkanRenderTargetLayout& Out) const
{
	Out.NumAttachments =			NumAttachments;
	Out.NumColorAttachments =		NumColorAttachments;

	Out.bHasDepthStencil =			bHasDepthStencil;
	Out.bHasResolveAttachments =	bHasResolveAttachments;
	Out.Hash =						Hash;

	Out.Extent.Extent3D.width =		Extent3D.X;
	Out.Extent.Extent3D.height =	Extent3D.Y;
	Out.Extent.Extent3D.depth =		Extent3D.Z;

	auto CopyAttachmentRefs = [&](const TArray<FGfxPipelineEntry::FRenderTargets::FAttachmentRef>& Source, VkAttachmentReference* Dest, uint32 Count)
	{
		for (uint32 Index = 0; Index < Count; ++Index, ++Dest)
		{
			Source[Index].WriteInto(*Dest);
		}
	};
	CopyAttachmentRefs(ColorAttachments, Out.ColorReferences, ARRAY_COUNT(Out.ColorReferences));
	CopyAttachmentRefs(ResolveAttachments, Out.ResolveReferences, ARRAY_COUNT(Out.ResolveReferences));
	DepthStencil.WriteInto(Out.DepthStencilReference);

	for (int32 Index = 0; Index < ARRAY_COUNT(Out.Desc); ++Index)
	{
		Descriptions[Index].WriteInto(Out.Desc[Index]);
	}
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry::FRenderTargets& RTs)
{
	// Modify VERSION if serialization changes
	Ar << RTs.NumAttachments;
	Ar << RTs.NumColorAttachments;
	Ar << RTs.ColorAttachments;
	Ar << RTs.ResolveAttachments;
	Ar << RTs.DepthStencil;

	Ar << RTs.Descriptions;

	Ar << RTs.bHasDepthStencil;
	Ar << RTs.bHasResolveAttachments;
	Ar << RTs.Hash;
	Ar << RTs.Extent3D;

	return Ar;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineGraphicsKey& Key)
{
	Ar << Key.Key[0];
	Ar << Key.Key[1];
	return Ar;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry& Entry)
{
	// Modify VERSION if serialization changes
	Ar << Entry.GraphicsKey;
	Ar << Entry.VertexInputKey;
	Ar << Entry.RasterizationSamples;
	Ar << Entry.Topology;

	Ar << Entry.ColorAttachmentStates;

	Ar << Entry.DescriptorSetLayoutBindings;

	Ar << Entry.VertexBindings;
	Ar << Entry.VertexAttributes;
	Ar << Entry.Rasterizer;

	Ar << Entry.DepthStencil;

	for (int32 Index = 0; Index < ARRAY_COUNT(Entry.ShaderMicrocodes); ++Index)
	{
		Entry.ShaderMicrocodes[Index].BulkSerialize(Ar);
		Ar << Entry.ShaderHashes[Index];
	}

	Ar << Entry.RenderTargets;

	return Ar;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FGfxPipelineEntry* Entry)
{
	return Ar << (*Entry);
}

void FVulkanPipelineStateCache::CreateGfxPipelineFromEntry(const FGfxPipelineEntry* GfxEntry, FVulkanGfxPipeline* Pipeline, const FVulkanBoundShaderState* BSS)
{
	// Pipeline
	VkGraphicsPipelineCreateInfo PipelineInfo;
	FMemory::Memzero(PipelineInfo);
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.layout = GfxEntry->PipelineLayout;

	// Color Blend
	VkPipelineColorBlendStateCreateInfo CBInfo;
	FMemory::Memzero(CBInfo);
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
	FMemory::Memzero(BlendStates);
	CBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	CBInfo.attachmentCount = GfxEntry->ColorAttachmentStates.Num();
	for (int32 Index = 0; Index < GfxEntry->ColorAttachmentStates.Num(); ++Index)
	{
		GfxEntry->ColorAttachmentStates[Index].WriteInto(BlendStates[Index]);
	}
	CBInfo.pAttachments = BlendStates;
	CBInfo.blendConstants[0] = 1.0f;
	CBInfo.blendConstants[1] = 1.0f;
	CBInfo.blendConstants[2] = 1.0f;
	CBInfo.blendConstants[3] = 1.0f;

	// Viewport
	VkPipelineViewportStateCreateInfo VPInfo;
	FMemory::Memzero(VPInfo);
	VPInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	VPInfo.viewportCount = 1;
	VPInfo.scissorCount = 1;

	// Multisample
	VkPipelineMultisampleStateCreateInfo MSInfo;
	FMemory::Memzero(MSInfo);
	MSInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	MSInfo.pSampleMask = NULL;
	MSInfo.rasterizationSamples = (VkSampleCountFlagBits)GfxEntry->RasterizationSamples;

	// Two stages: vs and fs
	VkPipelineShaderStageCreateInfo ShaderStages[SF_Compute];
	FMemory::Memzero(ShaderStages);
	PipelineInfo.stageCount = 0;
	PipelineInfo.pStages = ShaderStages;
	for (uint32 ShaderStage = 0; ShaderStage < SF_Compute; ++ShaderStage)
	{
		if (GfxEntry->ShaderMicrocodes[ShaderStage].Num() == 0)
		{
			continue;
		}
		const EShaderFrequency CurrStage = (EShaderFrequency)ShaderStage;

		ShaderStages[PipelineInfo.stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ShaderStages[PipelineInfo.stageCount].stage = UEFrequencyToVKStageBit(CurrStage);
		ShaderStages[PipelineInfo.stageCount].module = GfxEntry->ShaderModules[CurrStage];
		ShaderStages[PipelineInfo.stageCount].pName = "main";
		PipelineInfo.stageCount++;
	}

	check(PipelineInfo.stageCount != 0);

	// Vertex Input. The structure is mandatory even without vertex attributes.
	VkPipelineVertexInputStateCreateInfo VBInfo;
	FMemory::Memzero(VBInfo);
	VBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	TArray<VkVertexInputBindingDescription> VBBindings;
	for (const FGfxPipelineEntry::FVertexBinding& SourceBinding : GfxEntry->VertexBindings)
	{
		VkVertexInputBindingDescription* Binding = new(VBBindings) VkVertexInputBindingDescription;
		SourceBinding.WriteInto(*Binding);
	}
	VBInfo.vertexBindingDescriptionCount = VBBindings.Num();
	VBInfo.pVertexBindingDescriptions = VBBindings.GetData();
	TArray<VkVertexInputAttributeDescription> VBAttributes;
	for (const FGfxPipelineEntry::FVertexAttribute& SourceAttr : GfxEntry->VertexAttributes)
	{
		VkVertexInputAttributeDescription* Attr = new(VBAttributes) VkVertexInputAttributeDescription;
		SourceAttr.WriteInto(*Attr);
	}
	VBInfo.vertexAttributeDescriptionCount = VBAttributes.Num();
	VBInfo.pVertexAttributeDescriptions = VBAttributes.GetData();
	PipelineInfo.pVertexInputState = &VBInfo;

	PipelineInfo.pColorBlendState = &CBInfo;
	PipelineInfo.pMultisampleState = &MSInfo;
	PipelineInfo.pViewportState = &VPInfo;

	PipelineInfo.renderPass = GfxEntry->RenderPass->GetHandle();

	VkPipelineInputAssemblyStateCreateInfo InputAssembly;
	FMemory::Memzero(InputAssembly);
	InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssembly.topology = (VkPrimitiveTopology)GfxEntry->Topology;

	PipelineInfo.pInputAssemblyState = &InputAssembly;

	VkPipelineRasterizationStateCreateInfo RasterizerState;
	FVulkanRasterizerState::ResetCreateInfo(RasterizerState);
	GfxEntry->Rasterizer.WriteInto(RasterizerState);

	VkPipelineDepthStencilStateCreateInfo DepthStencilState;
	FMemory::Memzero(DepthStencilState);
	DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	GfxEntry->DepthStencil.WriteInto(DepthStencilState);

	PipelineInfo.pRasterizationState = &RasterizerState;
	PipelineInfo.pDepthStencilState = &DepthStencilState;

	VkPipelineDynamicStateCreateInfo DynamicState;
	FMemory::Memzero(DynamicState);
	DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	VkDynamicState DynamicStatesEnabled[VK_DYNAMIC_STATE_RANGE_SIZE];
	DynamicState.pDynamicStates = DynamicStatesEnabled;
	FMemory::Memzero(DynamicStatesEnabled);
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
	DynamicStatesEnabled[DynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;

	PipelineInfo.pDynamicState = &DynamicState;

	double BeginTime = BSS ? FPlatformTime::Seconds() : 0.0;
	//#todo-rco: Group the pipelines and create multiple (derived) pipelines at once
	VERIFYVULKANRESULT(VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, nullptr, &Pipeline->Pipeline));
	double EndTime = BSS ? FPlatformTime::Seconds() : 0.0;
	double Delta = EndTime - BeginTime;
	if (Delta > HitchTime)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Hitchy pipeline key 0x%08x%08x 0x%08x%08x VtxInKey 0x%08x VS %s GS %s PS %s"), 
			(uint32)((GfxEntry->GraphicsKey.Key[0] >> 32) & 0xffffffff), (uint32)(GfxEntry->GraphicsKey.Key[0] & 0xffffffff),
			(uint32)((GfxEntry->GraphicsKey.Key[1] >> 32) & 0xffffffff), (uint32)(GfxEntry->GraphicsKey.Key[1] & 0xffffffff),
			GfxEntry->VertexInputKey,
			*BSS->VertexShader->DebugName,
			BSS->GeometryShader ? *BSS->GeometryShader->DebugName : TEXT("nullptr"),
			BSS->PixelShader ? *BSS->PixelShader->DebugName : TEXT("nullptr"));
	}
}

void FVulkanPipelineStateCache::PopulateGfxEntry(const FVulkanGfxPipelineState& State, const FVulkanRenderPass* RenderPass, FVulkanPipelineStateCache::FGfxPipelineEntry* OutGfxEntry)
{
	OutGfxEntry->RasterizationSamples = RenderPass->GetLayout().GetAttachmentDescriptions()[0].samples;
	OutGfxEntry->Topology = (uint32)State.InputAssembly.topology;

	OutGfxEntry->ColorAttachmentStates.AddUninitialized(RenderPass->GetLayout().GetNumColorAttachments());
	for (int32 Index = 0; Index < OutGfxEntry->ColorAttachmentStates.Num(); ++Index)
	{
		OutGfxEntry->ColorAttachmentStates[Index].ReadFrom(State.BlendState->BlendStates[Index]);
	}

	{
		const VkPipelineVertexInputStateCreateInfo& VBInfo = State.BSS->GetVertexInputStateInfo().GetInfo();
		OutGfxEntry->VertexBindings.AddUninitialized(VBInfo.vertexBindingDescriptionCount);
		for (uint32 Index = 0; Index < VBInfo.vertexBindingDescriptionCount; ++Index)
		{
			OutGfxEntry->VertexBindings[Index].ReadFrom(VBInfo.pVertexBindingDescriptions[Index]);
		}

		OutGfxEntry->VertexAttributes.AddUninitialized(VBInfo.vertexAttributeDescriptionCount);
		for (uint32 Index = 0; Index < VBInfo.vertexAttributeDescriptionCount; ++Index)
		{
			OutGfxEntry->VertexAttributes[Index].ReadFrom(VBInfo.pVertexAttributeDescriptions[Index]);
		}
	}

	const TArray<FVulkanDescriptorSetsLayout::FSetLayout>& Layouts = State.BSS->GetDescriptorSetsLayout().GetLayouts();
	OutGfxEntry->DescriptorSetLayoutBindings.AddDefaulted(Layouts.Num());
	for (int32 Index = 0; Index < Layouts.Num(); ++Index)
	{
		for (int32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.Num(); ++SubIndex)
		{
			FDescriptorSetLayoutBinding* Binding = new(OutGfxEntry->DescriptorSetLayoutBindings[Index]) FDescriptorSetLayoutBinding;
			Binding->ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
		}
	}
	OutGfxEntry->PipelineLayout = State.BSS->GetPipelineLayout();

	OutGfxEntry->Rasterizer.ReadFrom(State.RasterizerState->RasterizerState);

	OutGfxEntry->DepthStencil.ReadFrom(State.DepthStencilState->DepthStencilState);

	int32 NumShaders = 0;
	for (int32 Index = 0; Index < SF_Compute; ++Index)
	{
		FVulkanShader* Shader = State.BSS->GetShaderPtr((EShaderFrequency)Index);
		if (Shader)
		{
			check(Shader->CodeSize != 0);
			OutGfxEntry->ShaderMicrocodes[Index].AddUninitialized(Shader->CodeSize);
			FMemory::Memcpy(OutGfxEntry->ShaderMicrocodes[Index].GetData(), Shader->Code, Shader->CodeSize);
			OutGfxEntry->ShaderHashes[Index] = State.BSS->GetShaderHashes()[Index];
			OutGfxEntry->ShaderModules[Index] = Shader->GetHandle();
			++NumShaders;
		}
	}
	check(NumShaders > 0);

	const FVulkanRenderTargetLayout& RTLayout = RenderPass->GetLayout();
	OutGfxEntry->RenderTargets.ReadFrom(RTLayout);

	OutGfxEntry->RenderPass = RenderPass;
}

void FVulkanPipelineStateCache::CreatGfxEntryRuntimeObjects(FGfxPipelineEntry* GfxEntry)
{
	{
		// Descriptor Set Layouts
		GfxEntry->DescriptorSetLayouts.AddZeroed(GfxEntry->DescriptorSetLayoutBindings.Num());
		for (int32 SetIndex = 0; SetIndex < GfxEntry->DescriptorSetLayoutBindings.Num(); ++SetIndex)
		{
			TArray<VkDescriptorSetLayoutBinding> DescriptorBindings;
			DescriptorBindings.AddZeroed(GfxEntry->DescriptorSetLayoutBindings[SetIndex].Num());
			for (int32 Index = 0; Index < GfxEntry->DescriptorSetLayoutBindings[SetIndex].Num(); ++Index)
			{
				GfxEntry->DescriptorSetLayoutBindings[SetIndex][Index].WriteInto(DescriptorBindings[Index]);
			}

			VkDescriptorSetLayoutCreateInfo DescriptorLayoutInfo;
			FMemory::Memzero(DescriptorLayoutInfo);
			DescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			DescriptorLayoutInfo.pNext = nullptr;
			DescriptorLayoutInfo.bindingCount = DescriptorBindings.Num();
			DescriptorLayoutInfo.pBindings = DescriptorBindings.GetData();

			VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorSetLayout(Device->GetInstanceHandle(), &DescriptorLayoutInfo, nullptr, &GfxEntry->DescriptorSetLayouts[SetIndex]));
		}

		// Pipeline layout
		VkPipelineLayoutCreateInfo PipelineLayoutInfo;
		FMemory::Memzero(PipelineLayoutInfo);
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		//PipelineLayoutInfo.pNext = nullptr;
		PipelineLayoutInfo.setLayoutCount = GfxEntry->DescriptorSetLayouts.Num();
		PipelineLayoutInfo.pSetLayouts = GfxEntry->DescriptorSetLayouts.GetData();
		//PipelineLayoutInfo.pushConstantRangeCount = 0;
		//PipelineLayoutInfo.pPushConstantRanges = nullptr;

		VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineLayout(Device->GetInstanceHandle(), &PipelineLayoutInfo, nullptr, &GfxEntry->PipelineLayout));
	}

	{
		// Shaders
		for (int32 Index = 0; Index < ARRAY_COUNT(GfxEntry->ShaderMicrocodes); ++Index)
		{
			if (GfxEntry->ShaderMicrocodes[Index].Num() != 0)
			{
				VkShaderModuleCreateInfo ModuleCreateInfo;
				FMemory::Memzero(ModuleCreateInfo);
				ModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				ModuleCreateInfo.codeSize = GfxEntry->ShaderMicrocodes[Index].Num();
				ModuleCreateInfo.pCode = (uint32*)GfxEntry->ShaderMicrocodes[Index].GetData();
				VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, nullptr, &GfxEntry->ShaderModules[Index]));
			}
		}
	}

	{
		FVulkanRenderTargetLayout RTLayout;
		GfxEntry->RenderTargets.WriteInto(RTLayout);
		GfxEntry->RenderPass = new FVulkanRenderPass(*Device, RTLayout);
	}
}


static TLinkedList<FVulkanBoundShaderState*>* GBSSList = nullptr;

TLinkedList<FVulkanBoundShaderState*>*& FVulkanPipelineStateCache::GetBSSList()
{
	return GBSSList;
}

void FVulkanPipelineStateCache::DestroyCache()
{
	VkDevice DeviceHandle = Device->GetInstanceHandle();
	auto DestroyGfxEntry = [DeviceHandle](FGfxPipelineEntry& GfxEntry)
	{
		for (int32 Index = 0; Index < ARRAY_COUNT(GfxEntry.ShaderModules); ++Index)
		{
			if (GfxEntry.ShaderModules[Index] != VK_NULL_HANDLE)
			{
				VulkanRHI::vkDestroyShaderModule(DeviceHandle, GfxEntry.ShaderModules[Index], nullptr);
			}
		}

		VulkanRHI::vkDestroyPipelineLayout(DeviceHandle, GfxEntry.PipelineLayout, nullptr);

		for (int32 Index = 0; Index < GfxEntry.DescriptorSetLayouts.Num(); ++Index)
		{
			VulkanRHI::vkDestroyDescriptorSetLayout(DeviceHandle, GfxEntry.DescriptorSetLayouts[Index], nullptr);
		}

		delete GfxEntry.RenderPass;
		GfxEntry.RenderPass = nullptr;
	};

	for (TLinkedList<FVulkanBoundShaderState*>::TIterator It(GetBSSList()); It; It.Next())
	{
		FVulkanBoundShaderState* BSS = *It;

		// toss the pipeline states
		for (auto& Pair : BSS->PipelineCache)
		{
			// Reference is decremented inside the Destroy function
			DestroyPipeline(Pair.Value);
		}

		BSS->PipelineCache.Empty(0);
	}

	for (FGfxPipelineEntry& Entry : GfxPipelineEntries)
	{
		if (Entry.bLoaded)
		{
			DestroyGfxEntry(Entry);
			Entry.bLoaded = false;

			FVulkanGfxPipeline* Pipeline = CreatedGfxPipelines.FindAndRemoveChecked(&Entry);
			if (Pipeline->GetRefCount() >= 1)
			{
				check(Pipeline->GetRefCount() == 1);
				Pipeline->Release();
			}
			else
			{
				delete Pipeline;
			}
		}
		else
		{
			Entry.RenderPass = nullptr;
		}
	}

	GfxPipelineEntries.Empty();

	KeyToGfxPipelineMap.Empty();
	CreatedGfxPipelines.Empty();
}

void FVulkanPipelineStateCache::RebuildCache()
{
	UE_LOG(LogVulkanRHI, Warning, TEXT("Rebuilding pipeline cache; ditching %d entries"), GfxPipelineEntries.Num());

	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}
	DestroyCache();
}

#endif	// VULKAN_ENABLE_PIPELINE_CACHE
