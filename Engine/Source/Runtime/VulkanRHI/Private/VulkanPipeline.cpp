// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipeline.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipeline.h"


void FVulkanPipelineState::Reset()
{
	Shader = nullptr;
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

void FVulkanPipelineState::InitializeDefaultStates()
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

#if !VULKAN_ENABLE_PIPELINE_CACHE
void FVulkanPipeline::Create(const FVulkanPipelineState& State)
{
	//SCOPE_CYCLE_COUNTER(STAT_VulkanCreatePipeline);

	// Pipeline
	VkGraphicsPipelineCreateInfo PipelineInfo;
	FMemory::Memzero(PipelineInfo);
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.layout = State.Shader->GetPipelineLayout();

	// Color Blend
	VkPipelineColorBlendStateCreateInfo CBInfo;
	FMemory::Memzero(CBInfo);
	CBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	CBInfo.attachmentCount = State.FrameBuffer->GetNumColorAttachments();
	CBInfo.pAttachments = State.BlendState->BlendStates;

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
		if (!State.Shader->HasShaderStage(CurrStage))
		{
			continue;
		}
		
		ShaderStages[PipelineInfo.stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ShaderStages[PipelineInfo.stageCount].stage = UEFrequencyToVKStageBit(CurrStage);
		ShaderStages[PipelineInfo.stageCount].module = State.Shader->GetShader(CurrStage).GetHandle();
		ShaderStages[PipelineInfo.stageCount].pName = "main";
		PipelineInfo.stageCount++;
	}
	
	// Vertex Input. The structure is mandatory even without vertex attributes.
	const VkPipelineVertexInputStateCreateInfo& VBInfo = State.Shader->GetVertexInputStateInfo().GetInfo();
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
#endif

#if !VULKAN_ENABLE_PIPELINE_CACHE
void FVulkanPipeline::Destroy()
{
	if (Pipeline)
	{
#if !VULKAN_ENABLE_PIPELINE_CACHE
		check(PipelineCache);
		vkDestroyPipelineCache(Device->GetInstanceHandle(), PipelineCache, nullptr);
		PipelineCache = VK_NULL_HANDLE;
#endif
		vkDestroyPipeline(Device->GetInstanceHandle(), Pipeline, nullptr);
		Pipeline = VK_NULL_HANDLE;
	}
}
#endif

void FVulkanPipeline::InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd, FVulkanPipelineState& State, bool bNeedsViewportUpdate, bool bNeedsScissorUpdate, bool bNeedsStencilRefUpdate)
{
	check(Device);
	check(Cmd);

	// Validate and update Viewport
	if (bNeedsViewportUpdate)
	{
		check(State.Viewport.width > 0);
		check(State.Viewport.height > 0);
		VulkanRHI::vkCmdSetViewport(Cmd->GetHandle(), 0, 1, &State.Viewport);

		State.bNeedsViewportUpdate = false;
	}

	// Validate and update scissor rect
	if (bNeedsScissorUpdate)
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

	if (bNeedsStencilRefUpdate)
	{
		VulkanRHI::vkCmdSetStencilReference(Cmd->GetHandle(), VK_STENCIL_FRONT_AND_BACK, State.StencilRef);
		State.bNeedsStencilRefUpdate = false;
	}
}

#if VULKAN_ENABLE_PIPELINE_CACHE

static TAutoConsoleVariable<int32> GEnablePipelineCacheLoadCvar(
	TEXT("r.Vulkan.PipelineCacheLoad"),
	1,
	TEXT("0 to disable loading the pipeline cache")
	TEXT("1 to enable using pipeline cache")
	);

FVulkanPipelineStateCache::FDiskEntry::~FDiskEntry()
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
			if (Version != FDiskEntry::VERSION)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache '%s' due to mismatched Version %d != %d"), *CacheFilename, Version, (int32)FDiskEntry::VERSION);
				continue;
			}
			int32 SizeOfDiskEntry = -1;
			Ar << SizeOfDiskEntry;
			if (SizeOfDiskEntry != (int32)(sizeof(FDiskEntry)))
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to load shader cache '%s' due to mismatched size of FDiskEntry %d != %d; forgot to bump up VERSION?"), *CacheFilename, SizeOfDiskEntry, (int32)sizeof(FDiskEntry));
				continue;
			}
			Ar << DiskEntries;

			for (int32 Index = 0; Index < DiskEntries.Num(); ++Index)
			{
				FVulkanPipeline* Pipeline = new FVulkanPipeline(Device);
				FDiskEntry* DiskEntry = &DiskEntries[Index];
				DiskEntry->bLoaded = true;
				CreateDiskEntryRuntimeObjects(DiskEntry);
				CreatePipelineFromDiskEntry(DiskEntry, Pipeline);

				FVulkanPipelineStateKey CreateInfo(DiskEntry->GraphicsKey, DiskEntry->VertexInputKey, DiskEntry->ShaderHashes);

				// Add to the cache
				KeyToPipelineMap.Add(CreateInfo, Pipeline);
				CreatedPipelines.Add(DiskEntry, Pipeline);
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

			UE_LOG(LogVulkanRHI, Display, TEXT("Loaded pipeline cache file '%s', %d Pipelines, %d bytes %s"), *CacheFilename, DiskEntries.Num(), MemFile.Num(), (bLoaded ? TEXT("VkPipelineCache compatible") : TEXT("")));
			break;
		}
	}

	return bLoaded;
}


void FVulkanPipelineStateCache::DestroyPipeline(FVulkanPipeline* Pipeline)
{
	if (Pipeline->Release() == 0)
	{
		const FVulkanPipelineStateKey* Key = KeyToPipelineMap.FindKey(Pipeline);
		check(Key);
		KeyToPipelineMap.Remove(*Key);
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
	int32 Version = FDiskEntry::VERSION;
	Ar << Version;
	int32 SizeOfDiskEntry = (int32)sizeof(FDiskEntry);
	Ar << SizeOfDiskEntry;
	Ar << DiskEntries;
	TArray<uint8> DeviceCache;
	size_t Size = 0;
	VERIFYVULKANRESULT(vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &Size, nullptr));
	if (Size > 0)
	{
		DeviceCache.AddUninitialized(Size);
		VERIFYVULKANRESULT(vkGetPipelineCacheData(Device->GetInstanceHandle(), PipelineCache, &Size, DeviceCache.GetData()));
	}
	Ar << DeviceCache;

	MemFile.Num();

	if (FFileHelper::SaveArrayToFile(MemFile, *CacheFilename))
	{
		UE_LOG(LogVulkanRHI, Display, TEXT("Saved pipeline cache file '%s', %d Pipelines, %d bytes"), *CacheFilename, DiskEntries.Num(), MemFile.Num());
	}
}

void FVulkanPipelineStateCache::CreateAndAdd(const FVulkanPipelineStateKey& CreateInfo, FVulkanPipeline* Pipeline, const FVulkanPipelineState& State, const FVulkanBoundShaderState& BSS)
{
	//if (EnablePipelineCacheCvar.GetValueOnRenderThread() == 1)
	//SCOPE_CYCLE_COUNTER(STAT_VulkanCreatePipeline);

	FDiskEntry* DiskEntry = new FDiskEntry();
	DiskEntry->GraphicsKey = CreateInfo.PipelineKey;
	DiskEntry->VertexInputKey = CreateInfo.VertexInputKey;
	PopulateDiskEntry(State, State.RenderPass, DiskEntry);

	// Create the pipeline
	CreatePipelineFromDiskEntry(DiskEntry, Pipeline);

	//UE_LOG(LogVulkanRHI, Display, TEXT("PK: Added Entry %llx Index %d"), CreateInfo.PipelineKey, DiskEntries.Num());

	// Add to the cache
	KeyToPipelineMap.Add(CreateInfo, Pipeline);
	DiskEntries.Add(DiskEntry);
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FBlendAttachment& Attachment)
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

void FVulkanPipelineStateCache::FDiskEntry::FBlendAttachment::ReadFrom(const VkPipelineColorBlendAttachmentState& InState)
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

void FVulkanPipelineStateCache::FDiskEntry::FBlendAttachment::WriteInto(VkPipelineColorBlendAttachmentState& Out) const
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


void FVulkanPipelineStateCache::FDiskEntry::FDescriptorSetLayoutBinding::ReadFrom(const VkDescriptorSetLayoutBinding& InState)
{
	Binding =			InState.binding;
	DescriptorCount =	InState.descriptorCount;
	DescriptorType =	InState.descriptorType;
	StageFlags =		InState.stageFlags;
}

void FVulkanPipelineStateCache::FDiskEntry::FDescriptorSetLayoutBinding::WriteInto(VkDescriptorSetLayoutBinding& Out) const
{
	Out.binding			= Binding;
	Out.descriptorCount	= DescriptorCount;
	Out.descriptorType	= (VkDescriptorType)DescriptorType;
	Out.stageFlags		= StageFlags;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FDescriptorSetLayoutBinding& Binding)
{
	// Modify VERSION if serialization changes
	Ar << Binding.Binding;
	Ar << Binding.DescriptorCount;
	Ar << Binding.DescriptorType;
	Ar << Binding.StageFlags;
	return Ar;
}

void FVulkanPipelineStateCache::FDiskEntry::FVertexBinding::ReadFrom(const VkVertexInputBindingDescription& InState)
{
	Binding =	InState.binding;
	InputRate =	(uint16)InState.inputRate;
	Stride =	InState.stride;
}

void FVulkanPipelineStateCache::FDiskEntry::FVertexBinding::WriteInto(VkVertexInputBindingDescription& Out) const
{
	Out.binding =	Binding;
	Out.inputRate =	(VkVertexInputRate)InputRate;
	Out.stride =	Stride;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FVertexBinding& Binding)
{
	// Modify VERSION if serialization changes
	Ar << Binding.Stride;
	Ar << Binding.Binding;
	Ar << Binding.InputRate;
	return Ar;
}

void FVulkanPipelineStateCache::FDiskEntry::FVertexAttribute::ReadFrom(const VkVertexInputAttributeDescription& InState)
{
	Binding =	InState.binding;
	Format =	(uint32)InState.format;
	Location =	InState.location;
	Offset =	InState.offset;
}

void FVulkanPipelineStateCache::FDiskEntry::FVertexAttribute::WriteInto(VkVertexInputAttributeDescription& Out) const
{
	Out.binding =	Binding;
	Out.format =	(VkFormat)Format;
	Out.location =	Location;
	Out.offset =	Offset;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FVertexAttribute& Attribute)
{
	// Modify VERSION if serialization changes
	Ar << Attribute.Location;
	Ar << Attribute.Binding;
	Ar << Attribute.Format;
	Ar << Attribute.Offset;
	return Ar;
}

void FVulkanPipelineStateCache::FDiskEntry::FRasterizer::ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState)
{
	PolygonMode =				InState.polygonMode;
	CullMode =					InState.cullMode;
	DepthBiasSlopeScale =		InState.depthBiasSlopeFactor;
	DepthBiasConstantFactor =	InState.depthBiasConstantFactor;
}

void FVulkanPipelineStateCache::FDiskEntry::FRasterizer::WriteInto(VkPipelineRasterizationStateCreateInfo& Out) const
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

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FRasterizer& Rasterizer)
{
	// Modify VERSION if serialization changes
	Ar << Rasterizer.PolygonMode;
	Ar << Rasterizer.CullMode;
	Ar << Rasterizer.DepthBiasSlopeScale;
	Ar << Rasterizer.DepthBiasConstantFactor;
	return Ar;
}


void FVulkanPipelineStateCache::FDiskEntry::FDepthStencil::ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState)
{
	DepthCompareOp =		(uint8)InState.depthCompareOp;
	bDepthTestEnable =		InState.depthTestEnable != VK_FALSE;
	bDepthWriteEnable =		InState.depthWriteEnable != VK_FALSE;
	bStencilTestEnable =	InState.stencilTestEnable != VK_FALSE;
	FrontFailOp =			(uint8)InState.front.failOp;
	FrontPassOp =			(uint8)InState.front.passOp;
	FrontDepthFailOp =		(uint8)InState.front.depthFailOp;
	FrontCompareOp =		(uint8)InState.front.compareOp;
	FrontWriteMask =		InState.front.writeMask;
	FrontReference =		InState.front.reference;
	BackFailOp =			(uint8)InState.back.failOp;
	BackPassOp =			(uint8)InState.back.passOp;
	BackDepthFailOp =		(uint8)InState.back.depthFailOp;
	BackCompareOp =			(uint8)InState.back.compareOp;
	BackWriteMask =			InState.back.writeMask;
	BackReference =			InState.back.reference;
}

void FVulkanPipelineStateCache::FDiskEntry::FDepthStencil::WriteInto(VkPipelineDepthStencilStateCreateInfo& Out) const
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
	Out.front.writeMask =		FrontWriteMask;
	Out.front.reference =		FrontReference;
	Out.back.failOp =			(VkStencilOp)BackFailOp;
	Out.back.passOp =			(VkStencilOp)BackPassOp;
	Out.back.depthFailOp =		(VkStencilOp)BackDepthFailOp;
	Out.back.compareOp =		(VkCompareOp)BackCompareOp;
	Out.back.writeMask =		BackWriteMask;
	Out.back.reference =		BackReference;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FDepthStencil& DepthStencil)
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
	Ar << DepthStencil.FrontWriteMask;
	Ar << DepthStencil.FrontReference;
	Ar << DepthStencil.BackFailOp;
	Ar << DepthStencil.BackPassOp;
	Ar << DepthStencil.BackDepthFailOp;
	Ar << DepthStencil.BackCompareOp;
	Ar << DepthStencil.BackWriteMask;
	Ar << DepthStencil.BackReference;
	return Ar;
}

void FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::FAttachmentRef::ReadFrom(const VkAttachmentReference& InState)
{
	Attachment =	InState.attachment;
	Layout =		(uint64)InState.layout;
}

void FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::FAttachmentRef::WriteInto(VkAttachmentReference& Out) const
{
	Out.attachment =	Attachment;
	Out.layout =		(VkImageLayout)Layout;
}

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::FAttachmentRef& AttachmentRef)
{
	// Modify VERSION if serialization changes
	Ar << AttachmentRef.Attachment;
	Ar << AttachmentRef.Layout;
	return Ar;
}

void FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::FAttachmentDesc::ReadFrom(const VkAttachmentDescription &InState)
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

void FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::FAttachmentDesc::WriteInto(VkAttachmentDescription& Out) const
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

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::FAttachmentDesc& AttachmentDesc)
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


void FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::ReadFrom(const FVulkanRenderTargetLayout& RTLayout)
{
	NumAttachments =			RTLayout.NumAttachments;
	NumColorAttachments =		RTLayout.NumColorAttachments;

	bHasDepthStencil =			RTLayout.bHasDepthStencil;
	bHasResolveAttachments =	RTLayout.bHasResolveAttachments;
	Hash =						RTLayout.Hash;

	Extent3D.X = RTLayout.Extent.Extent3D.width;
	Extent3D.Y = RTLayout.Extent.Extent3D.height;
	Extent3D.Z = RTLayout.Extent.Extent3D.depth;

	auto CopyAttachmentRefs = [&](TArray<FDiskEntry::FRenderTargets::FAttachmentRef>& Dest, const VkAttachmentReference* Source, uint32 Count)
	{
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FDiskEntry::FRenderTargets::FAttachmentRef* New = new(Dest) FDiskEntry::FRenderTargets::FAttachmentRef;
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

void FVulkanPipelineStateCache::FDiskEntry::FRenderTargets::WriteInto(FVulkanRenderTargetLayout& Out) const
{
	Out.NumAttachments =			NumAttachments;
	Out.NumColorAttachments =		NumColorAttachments;

	Out.bHasDepthStencil =			bHasDepthStencil;
	Out.bHasResolveAttachments =	bHasResolveAttachments;
	Out.Hash =						Hash;

	Out.Extent.Extent3D.width =		Extent3D.X;
	Out.Extent.Extent3D.height =	Extent3D.Y;
	Out.Extent.Extent3D.depth =		Extent3D.Z;

	auto CopyAttachmentRefs = [&](const TArray<FDiskEntry::FRenderTargets::FAttachmentRef>& Source, VkAttachmentReference* Dest, uint32 Count)
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

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry::FRenderTargets& RTs)
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

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry& Entry)
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

FArchive& operator << (FArchive& Ar, FVulkanPipelineStateCache::FDiskEntry* Entry)
{
	return Ar << (*Entry);
}

void FVulkanPipelineStateCache::CreatePipelineFromDiskEntry(const FDiskEntry* DiskEntry, FVulkanPipeline* Pipeline)
{
	// Pipeline
	VkGraphicsPipelineCreateInfo PipelineInfo;
	FMemory::Memzero(PipelineInfo);
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.layout = DiskEntry->PipelineLayout;

	// Color Blend
	VkPipelineColorBlendStateCreateInfo CBInfo;
	FMemory::Memzero(CBInfo);
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
	FMemory::Memzero(BlendStates);
	CBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	CBInfo.attachmentCount = DiskEntry->ColorAttachmentStates.Num();
	for (int32 Index = 0; Index < DiskEntry->ColorAttachmentStates.Num(); ++Index)
	{
		DiskEntry->ColorAttachmentStates[Index].WriteInto(BlendStates[Index]);
	}
	CBInfo.pAttachments = BlendStates;

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
	MSInfo.rasterizationSamples = (VkSampleCountFlagBits)DiskEntry->RasterizationSamples;

	// Two stages: vs and fs
	VkPipelineShaderStageCreateInfo ShaderStages[SF_Compute];
	FMemory::Memzero(ShaderStages);
	PipelineInfo.stageCount = 0;
	PipelineInfo.pStages = ShaderStages;
	for (uint32 ShaderStage = 0; ShaderStage < SF_Compute; ++ShaderStage)
	{
		if (DiskEntry->ShaderMicrocodes[ShaderStage].Num() == 0)
		{
			continue;
		}
		const EShaderFrequency CurrStage = (EShaderFrequency)ShaderStage;

		ShaderStages[PipelineInfo.stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ShaderStages[PipelineInfo.stageCount].stage = UEFrequencyToVKStageBit(CurrStage);
		ShaderStages[PipelineInfo.stageCount].module = DiskEntry->ShaderModules[CurrStage];
		ShaderStages[PipelineInfo.stageCount].pName = "main";
		PipelineInfo.stageCount++;
	}

	check(PipelineInfo.stageCount != 0);

	// Vertex Input. The structure is mandatory even without vertex attributes.
	VkPipelineVertexInputStateCreateInfo VBInfo;
	FMemory::Memzero(VBInfo);
	VBInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	TArray<VkVertexInputBindingDescription> VBBindings;
	for (const FDiskEntry::FVertexBinding& SourceBinding : DiskEntry->VertexBindings)
	{
		VkVertexInputBindingDescription* Binding = new(VBBindings) VkVertexInputBindingDescription;
		SourceBinding.WriteInto(*Binding);
	}
	VBInfo.vertexBindingDescriptionCount = VBBindings.Num();
	VBInfo.pVertexBindingDescriptions = VBBindings.GetData();
	TArray<VkVertexInputAttributeDescription> VBAttributes;
	for (const FDiskEntry::FVertexAttribute& SourceAttr : DiskEntry->VertexAttributes)
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

	PipelineInfo.renderPass = DiskEntry->RenderPass->GetHandle();

	VkPipelineInputAssemblyStateCreateInfo InputAssembly;
	FMemory::Memzero(InputAssembly);
	InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssembly.topology = (VkPrimitiveTopology)DiskEntry->Topology;

	PipelineInfo.pInputAssemblyState = &InputAssembly;

	VkPipelineRasterizationStateCreateInfo RasterizerState;
	FVulkanRasterizerState::ResetCreateInfo(RasterizerState);
	DiskEntry->Rasterizer.WriteInto(RasterizerState);

	VkPipelineDepthStencilStateCreateInfo DepthStencilState;
	FMemory::Memzero(DepthStencilState);
	DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	DiskEntry->DepthStencil.WriteInto(DepthStencilState);

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

	//#todo-rco: Group the pipelines and create multiple (derived) pipelines at once
	VERIFYVULKANRESULT(VulkanRHI::vkCreateGraphicsPipelines(Device->GetInstanceHandle(), PipelineCache, 1, &PipelineInfo, nullptr, &Pipeline->Pipeline));
}

void FVulkanPipelineStateCache::PopulateDiskEntry(const FVulkanPipelineState& State, const FVulkanRenderPass* RenderPass, FVulkanPipelineStateCache::FDiskEntry* OutDiskEntry)
{
	OutDiskEntry->RasterizationSamples = State.RenderPass->GetLayout().GetAttachmentDescriptions()[0].samples;
	OutDiskEntry->Topology = (uint32)State.InputAssembly.topology;

	OutDiskEntry->ColorAttachmentStates.AddUninitialized(State.FrameBuffer->GetNumColorAttachments());
	for (int32 Index = 0; Index < OutDiskEntry->ColorAttachmentStates.Num(); ++Index)
	{
		OutDiskEntry->ColorAttachmentStates[Index].ReadFrom(State.BlendState->BlendStates[Index]);
	}

	{
		const VkPipelineVertexInputStateCreateInfo& VBInfo = State.Shader->GetVertexInputStateInfo().GetInfo();
		OutDiskEntry->VertexBindings.AddUninitialized(VBInfo.vertexBindingDescriptionCount);
		for (uint32 Index = 0; Index < VBInfo.vertexBindingDescriptionCount; ++Index)
		{
			OutDiskEntry->VertexBindings[Index].ReadFrom(VBInfo.pVertexBindingDescriptions[Index]);
		}

		OutDiskEntry->VertexAttributes.AddUninitialized(VBInfo.vertexAttributeDescriptionCount);
		for (uint32 Index = 0; Index < VBInfo.vertexAttributeDescriptionCount; ++Index)
		{
			OutDiskEntry->VertexAttributes[Index].ReadFrom(VBInfo.pVertexAttributeDescriptions[Index]);
		}
	}

	const TArray<FVulkanDescriptorSetsLayout::FSetLayout>& Layouts = State.Shader->GetDescriptorSetsLayout().GetLayouts();
	OutDiskEntry->DescriptorSetLayoutBindings.AddDefaulted(Layouts.Num());
	for (int32 Index = 0; Index < Layouts.Num(); ++Index)
	{
		for (int32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.Num(); ++SubIndex)
		{
			FDiskEntry::FDescriptorSetLayoutBinding* Binding = new(OutDiskEntry->DescriptorSetLayoutBindings[Index]) FDiskEntry::FDescriptorSetLayoutBinding;
			Binding->ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
		}
	}
	OutDiskEntry->PipelineLayout = State.Shader->GetPipelineLayout();

	OutDiskEntry->Rasterizer.ReadFrom(State.RasterizerState->RasterizerState);

	OutDiskEntry->DepthStencil.ReadFrom(State.DepthStencilState->DepthStencilState);

	int32 NumShaders = 0;
	for (int32 Index = 0; Index < SF_Compute; ++Index)
	{
		FVulkanShader* Shader = State.Shader->GetShaderPtr((EShaderFrequency)Index);
		if (Shader)
		{
			check(Shader->CodeSize != 0);
			OutDiskEntry->ShaderMicrocodes[Index].AddUninitialized(Shader->CodeSize);
			FMemory::Memcpy(OutDiskEntry->ShaderMicrocodes[Index].GetData(), Shader->Code, Shader->CodeSize);
			OutDiskEntry->ShaderHashes[Index] = State.Shader->GetShaderHashes()[Index];
			OutDiskEntry->ShaderModules[Index] = Shader->GetHandle();
			++NumShaders;
		}
	}
	check(NumShaders > 0);

	const FVulkanRenderTargetLayout& RTLayout = RenderPass->GetLayout();
	OutDiskEntry->RenderTargets.ReadFrom(RTLayout);

	OutDiskEntry->RenderPass = RenderPass;
}

void FVulkanPipelineStateCache::CreateDiskEntryRuntimeObjects(FDiskEntry* DiskEntry)
{
	{
		// Descriptor Set Layouts
		DiskEntry->DescriptorSetLayouts.AddZeroed(DiskEntry->DescriptorSetLayoutBindings.Num());
		for (int32 SetIndex = 0; SetIndex < DiskEntry->DescriptorSetLayoutBindings.Num(); ++SetIndex)
		{
			TArray<VkDescriptorSetLayoutBinding> DescriptorBindings;
			DescriptorBindings.AddZeroed(DiskEntry->DescriptorSetLayoutBindings[SetIndex].Num());
			for (int32 Index = 0; Index < DiskEntry->DescriptorSetLayoutBindings[SetIndex].Num(); ++Index)
			{
				DiskEntry->DescriptorSetLayoutBindings[SetIndex][Index].WriteInto(DescriptorBindings[Index]);
			}

			VkDescriptorSetLayoutCreateInfo DescriptorLayoutInfo;
			FMemory::Memzero(DescriptorLayoutInfo);
			DescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			DescriptorLayoutInfo.pNext = nullptr;
			DescriptorLayoutInfo.bindingCount = DescriptorBindings.Num();
			DescriptorLayoutInfo.pBindings = DescriptorBindings.GetData();

			VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorSetLayout(Device->GetInstanceHandle(), &DescriptorLayoutInfo, nullptr, &DiskEntry->DescriptorSetLayouts[SetIndex]));
		}

		// Pipeline layout
		VkPipelineLayoutCreateInfo PipelineLayoutInfo;
		FMemory::Memzero(PipelineLayoutInfo);
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		//PipelineLayoutInfo.pNext = nullptr;
		PipelineLayoutInfo.setLayoutCount = DiskEntry->DescriptorSetLayouts.Num();
		PipelineLayoutInfo.pSetLayouts = DiskEntry->DescriptorSetLayouts.GetData();
		//PipelineLayoutInfo.pushConstantRangeCount = 0;
		//PipelineLayoutInfo.pPushConstantRanges = nullptr;

		VERIFYVULKANRESULT(VulkanRHI::vkCreatePipelineLayout(Device->GetInstanceHandle(), &PipelineLayoutInfo, nullptr, &DiskEntry->PipelineLayout));
	}

	{
		// Shaders
		for (int32 Index = 0; Index < ARRAY_COUNT(DiskEntry->ShaderMicrocodes); ++Index)
		{
			if (DiskEntry->ShaderMicrocodes[Index].Num() != 0)
			{
				VkShaderModuleCreateInfo ModuleCreateInfo;
				FMemory::Memzero(ModuleCreateInfo);
				ModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				ModuleCreateInfo.codeSize = DiskEntry->ShaderMicrocodes[Index].Num();
				ModuleCreateInfo.pCode = (uint32*)DiskEntry->ShaderMicrocodes[Index].GetData();
				VERIFYVULKANRESULT(VulkanRHI::vkCreateShaderModule(Device->GetInstanceHandle(), &ModuleCreateInfo, nullptr, &DiskEntry->ShaderModules[Index]));
			}
		}
	}

	{
		FVulkanRenderTargetLayout RTLayout;
		DiskEntry->RenderTargets.WriteInto(RTLayout);
		DiskEntry->RenderPass = new FVulkanRenderPass(*Device, RTLayout);
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
	auto DestroyDiskEntry = [DeviceHandle](FDiskEntry& DiskEntry)
	{
		for (int32 Index = 0; Index < ARRAY_COUNT(DiskEntry.ShaderModules); ++Index)
		{
			if (DiskEntry.ShaderModules[Index] != VK_NULL_HANDLE)
			{
				VulkanRHI::vkDestroyShaderModule(DeviceHandle, DiskEntry.ShaderModules[Index], nullptr);
			}
		}

		VulkanRHI::vkDestroyPipelineLayout(DeviceHandle, DiskEntry.PipelineLayout, nullptr);

		for (int32 Index = 0; Index < DiskEntry.DescriptorSetLayouts.Num(); ++Index)
		{
			VulkanRHI::vkDestroyDescriptorSetLayout(DeviceHandle, DiskEntry.DescriptorSetLayouts[Index], nullptr);
		}

		delete DiskEntry.RenderPass;
		DiskEntry.RenderPass = nullptr;
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

	for (FDiskEntry& Entry : DiskEntries)
	{
		if (Entry.bLoaded)
		{
			DestroyDiskEntry(Entry);
			Entry.bLoaded = false;

			FVulkanPipeline* Pipeline = CreatedPipelines.FindAndRemoveChecked(&Entry);
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

	DiskEntries.Empty();

	KeyToPipelineMap.Empty();
	CreatedPipelines.Empty();
}

void FVulkanPipelineStateCache::RebuildCache()
{
	UE_LOG(LogVulkanRHI, Warning, TEXT("Rebuilding pipeline cache; ditching %d entries"), DiskEntries.Num());

	if (IsInGameThread())
	{
		FlushRenderingCommands();
	}
	DestroyCache();
}

#endif	// VULKAN_ENABLE_PIPELINE_CACHE
