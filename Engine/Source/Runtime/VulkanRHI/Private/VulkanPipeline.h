// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPipeline.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

class FVulkanDevice;
class FVulkanFramebuffer;

// High level description of the state
struct FVulkanPipelineState
{
	FVulkanPipelineState()
		: RenderPass(nullptr)
		, FrameBuffer(nullptr)
	{
		Reset();
	}
	~FVulkanPipelineState()
	{
		Reset();
	}

	void Reset();

	TRefCountPtr<FVulkanBoundShaderState> Shader;
	FVulkanRenderPass* RenderPass;
	FVulkanFramebuffer* FrameBuffer;

	VkPipelineDynamicStateCreateInfo DynamicState;
	VkPipelineInputAssemblyStateCreateInfo InputAssembly;
	TRefCountPtr<FVulkanRasterizerState> RasterizerState;
	TRefCountPtr<FVulkanBlendState> BlendState;
	TRefCountPtr<FVulkanDepthStencilState> DepthStencilState;

	VkRect2D Scissor;
	VkViewport Viewport;
	uint32 StencilRef, PrevStencilRef;
	bool bNeedsScissorUpdate, bNeedsViewportUpdate, bNeedsStencilRefUpdate;

private:
	void InitializeDefaultStates();

private:
	VkDynamicState DynamicStatesEnabled[VK_DYNAMIC_STATE_RANGE_SIZE];
};

#if VULKAN_ENABLE_PIPELINE_CACHE
struct FVulkanPipelineStateKey
{
	FVulkanPipelineStateKey(const FVulkanPipelineGraphicsKey& InPipelineKey, uint32 InVertexInputKey, const FSHAHash* InShaderHashes) :
		PipelineKey(InPipelineKey),
		VertexInputKey(InVertexInputKey)
	{
		FMemory::Memcpy(ShaderHashes, InShaderHashes, sizeof(ShaderHashes));
	}
	
	FVulkanPipelineStateKey(const FVulkanPipelineGraphicsKey& InPipelineKey, uint32 InVertexInputKey, const FVulkanBoundShaderState* State) :
		PipelineKey(InPipelineKey),
		VertexInputKey(InVertexInputKey)
	{
		FMemory::Memcpy(ShaderHashes, State->GetShaderHashes(), sizeof(ShaderHashes));
	}

	FVulkanPipelineGraphicsKey PipelineKey;
	uint32 VertexInputKey;
	FSHAHash ShaderHashes[SF_Compute];

	inline bool operator == (const FVulkanPipelineStateKey& In) const
	{
		return PipelineKey == In.PipelineKey && VertexInputKey == In.VertexInputKey && FMemory::Memcmp(ShaderHashes, In.ShaderHashes, sizeof(ShaderHashes)) == 0;
	}
};

inline uint32 GetTypeHash(const FVulkanPipelineStateKey& Key)
{
	return (uint32)GetTypeHash(Key.PipelineKey) ^ *(uint32*)&Key.ShaderHashes[SF_Vertex];
}

#endif

class FVulkanPipeline : public VulkanRHI::FRefCount
{
public:
	FVulkanPipeline(FVulkanDevice* InDevice);
	~FVulkanPipeline();

#if !VULKAN_ENABLE_PIPELINE_CACHE
	void Create(const FVulkanPipelineState& State);
	void Destroy();
#endif

	inline VkPipeline GetHandle() const
	{
		return Pipeline;
	}

	inline void UpdateDynamicStates(FVulkanCmdBuffer* Cmd, FVulkanPipelineState& State)
	{
		if (State.bNeedsViewportUpdate || State.bNeedsScissorUpdate || State.bNeedsStencilRefUpdate)
		{
			InternalUpdateDynamicStates(Cmd, State, State.bNeedsViewportUpdate, State.bNeedsScissorUpdate, State.bNeedsStencilRefUpdate);
		}
	}

private:
	void InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd, FVulkanPipelineState& State, bool bNeedsViewportUpdate, bool bNeedsScissorUpdate, bool bNeedsStencilRefUpdate);

	FVulkanDevice* Device;
	VkPipeline Pipeline;
#if VULKAN_ENABLE_PIPELINE_CACHE
	friend class FVulkanPipelineStateCache;
#else
	VkPipelineCache PipelineCache;
#endif
};


#if VULKAN_ENABLE_PIPELINE_CACHE

class FVulkanPipelineStateCache
{
public:
	FVulkanPipeline* Find(const FVulkanPipelineStateKey& CreateInfo)
	{
		FVulkanPipeline** Found = KeyToPipelineMap.Find(CreateInfo);
#if 0
		if (Found)
		{
			for (int i = 0; i < DiskEntries.Num(); ++i)
			{
				FDiskEntry* Entry = &DiskEntries[i];
				if (Entry->PipelineKey == CreateInfo.PipelineKey)
				{
					if (!FMemory::Memcmp(Entry->ShaderHashes, CreateInfo.ShaderHashes, sizeof(CreateInfo.ShaderHashes)))
					{
						UE_LOG(LogVulkanRHI, Display, TEXT("PK: Found matching Entry %llx Index %d"), CreateInfo.PipelineKey, i);
						break;
					}
				}
			}
		}
#endif
		return Found ? *Found : nullptr;
	}

	void DestroyPipeline(FVulkanPipeline* Pipeline);

	// Array of potential cache locations; first entries have highest priority. Only one cache file is loaded. If unsuccessful, tries next entry in the array.
	void InitAndLoad(const TArray<FString>& CacheFilenames);
	void Save(FString& CacheFilename);

	FVulkanPipelineStateCache(FVulkanDevice* InParent);
	~FVulkanPipelineStateCache();

	void CreateAndAdd(const FVulkanPipelineStateKey& CreateInfo, FVulkanPipeline* Pipeline, const FVulkanPipelineState& State, const FVulkanBoundShaderState& BSS);

	void RebuildCache();

	static TLinkedList<FVulkanBoundShaderState*>*& GetBSSList();

	// Actual information required to recreate a pipeline when saving/loading from disk
	struct FDiskEntry
	{
		FVulkanPipelineGraphicsKey GraphicsKey;
		uint32 VertexInputKey;
		bool bLoaded;

		uint32 RasterizationSamples;
		uint32 Topology;
		struct FBlendAttachment
		{
			bool bBlend;
			uint8 ColorBlendOp;
			uint8 SrcColorBlendFactor;
			uint8 DstColorBlendFactor;
			uint8 AlphaBlendOp;
			uint8 SrcAlphaBlendFactor;
			uint8 DstAlphaBlendFactor;
			uint8 ColorWriteMask;

			void ReadFrom(const VkPipelineColorBlendAttachmentState& InState);
			void WriteInto(VkPipelineColorBlendAttachmentState& OutState) const;

			bool operator==(const FBlendAttachment& In) const
			{
				return ColorBlendOp == In.ColorBlendOp &&
					SrcColorBlendFactor == In.SrcColorBlendFactor &&
					DstColorBlendFactor == In.DstColorBlendFactor &&
					AlphaBlendOp == In.AlphaBlendOp &&
					SrcAlphaBlendFactor == In.SrcAlphaBlendFactor &&
					DstAlphaBlendFactor == In.DstAlphaBlendFactor &&
					ColorWriteMask == In.ColorWriteMask;
			}
		};
		TArray<FBlendAttachment> ColorAttachmentStates;

		struct FDescriptorSetLayoutBinding
		{
			uint32 Binding;
			uint16 DescriptorCount;
			uint8 DescriptorType;
			uint8 StageFlags;

			void ReadFrom(const VkDescriptorSetLayoutBinding& InState);
			void WriteInto(VkDescriptorSetLayoutBinding& OutState) const;

			bool operator==(const FDescriptorSetLayoutBinding& In) const
			{
				return  Binding == In.Binding &&
					DescriptorCount == In.DescriptorCount &&
					DescriptorType == In.DescriptorType &&
					StageFlags == In.StageFlags;
			}
		};
		TArray<TArray<FDescriptorSetLayoutBinding>> DescriptorSetLayoutBindings;

		struct FVertexBinding
		{
			uint32 Stride;
			uint16 Binding;
			uint16 InputRate;

			void ReadFrom(const VkVertexInputBindingDescription& InState);
			void WriteInto(VkVertexInputBindingDescription& OutState) const;
			bool operator==(const FVertexBinding& In) const
			{
				return Stride == In.Stride &&
					Binding == In.Binding &&
					InputRate == In.InputRate;
			}
		};
		TArray<FVertexBinding> VertexBindings;
		struct FVertexAttribute
		{
			uint32 Location;
			uint32 Binding;
			uint32 Format;
			uint32 Offset;

			void ReadFrom(const VkVertexInputAttributeDescription& InState);
			void WriteInto(VkVertexInputAttributeDescription& OutState) const;

			bool operator==(const FVertexAttribute& In) const
			{
				return Location == In.Location &&
					Binding == In.Binding &&
					Format == In.Format &&
					Offset == In.Offset;
			}
		};
		TArray<FVertexAttribute> VertexAttributes;

		struct FRasterizer
		{
			uint8 PolygonMode;
			uint8 CullMode;
			float DepthBiasSlopeScale;
			float DepthBiasConstantFactor;

			void ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState);
			void WriteInto(VkPipelineRasterizationStateCreateInfo& OutState) const;

			bool operator==(const FRasterizer& In) const
			{
				return PolygonMode == In.PolygonMode &&
					CullMode == In.CullMode &&
					DepthBiasSlopeScale == In.DepthBiasSlopeScale &&
					DepthBiasConstantFactor == In.DepthBiasConstantFactor;
			}
		};
		FRasterizer Rasterizer;

		struct FDepthStencil
		{
			uint8 DepthCompareOp;
			bool bDepthTestEnable;
			bool bDepthWriteEnable;
			bool bStencilTestEnable;
			uint8 FrontFailOp;
			uint8 FrontPassOp;
			uint8 FrontDepthFailOp;
			uint8 FrontCompareOp;
			uint32 FrontWriteMask;
			uint32 FrontReference;
			uint8 BackFailOp;
			uint8 BackPassOp;
			uint8 BackDepthFailOp;
			uint8 BackCompareOp;
			uint32 BackWriteMask;
			uint32 BackReference;

			void ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState);
			void WriteInto(VkPipelineDepthStencilStateCreateInfo& OutState) const;

			bool operator==(const FDepthStencil& In) const
			{
				return DepthCompareOp == In.DepthCompareOp &&
					bDepthTestEnable == In.bDepthTestEnable &&
					bDepthWriteEnable == In.bDepthWriteEnable &&
					bStencilTestEnable == In.bStencilTestEnable &&
					FrontFailOp == In.FrontFailOp &&
					FrontPassOp == In.FrontPassOp &&
					FrontDepthFailOp == In.FrontDepthFailOp &&
					FrontCompareOp == In.FrontCompareOp &&
					FrontWriteMask == In.FrontWriteMask &&
					FrontReference == In.FrontReference &&
					BackFailOp == In.BackFailOp &&
					BackPassOp == In.BackPassOp &&
					BackDepthFailOp == In.BackDepthFailOp &&
					BackCompareOp == In.BackCompareOp &&
					BackWriteMask == In.BackWriteMask &&
					BackReference == In.BackReference;
			}
		};
		FDepthStencil DepthStencil;

		//#todo-rco: Compute!
		TArray<uint8> ShaderMicrocodes[SF_Compute];
		FSHAHash ShaderHashes[SF_Compute];

		struct FRenderTargets
		{
			struct FAttachmentRef
			{
				uint32 Attachment;
				uint64 Layout;

				void ReadFrom(const VkAttachmentReference& InState);
				void WriteInto(VkAttachmentReference& OutState) const;
				bool operator == (const FAttachmentRef& In) const
				{
					return Attachment == In.Attachment && Layout == In.Layout;
				}
			};
			TArray<FAttachmentRef> ColorAttachments;
			TArray<FAttachmentRef> ResolveAttachments;
			FAttachmentRef DepthStencil;

			struct FAttachmentDesc
			{
				uint32 Format;
				uint8 Flags;
				uint8 Samples;
				uint8 LoadOp;
				uint8 StoreOp;
				uint8 StencilLoadOp;
				uint8 StencilStoreOp;
				uint64 InitialLayout;
				uint64 FinalLayout;

				bool operator==(const FAttachmentDesc& In) const
				{
					return Format == In.Format &&
						Flags == In.Flags &&
						Samples == In.Samples &&
						LoadOp == In.LoadOp &&
						StoreOp == In.StoreOp &&
						StencilLoadOp == In.StencilLoadOp &&
						StencilStoreOp == In.StencilStoreOp &&
						InitialLayout == In.InitialLayout &&
						FinalLayout == In.FinalLayout;
				}

				void ReadFrom(const VkAttachmentDescription &InState);
				void WriteInto(VkAttachmentDescription& OutState) const;
			};
			TArray<FAttachmentDesc> Descriptions;

			uint8 NumAttachments;
			uint8 NumColorAttachments;
			bool bHasDepthStencil;
			bool bHasResolveAttachments;
			uint32 Hash;
			FVector Extent3D;

			void ReadFrom(const FVulkanRenderTargetLayout &InState);
			void WriteInto(FVulkanRenderTargetLayout& OutState) const;

			bool operator==(const FRenderTargets& In) const
			{
				return ColorAttachments == In.ColorAttachments &&
					ResolveAttachments == In.ResolveAttachments &&
					DepthStencil == In.DepthStencil &&
					Descriptions == In.Descriptions &&
					NumAttachments == In.NumAttachments &&
					NumColorAttachments == In.NumColorAttachments &&
					bHasDepthStencil == In.bHasDepthStencil &&
					bHasResolveAttachments == In.bHasResolveAttachments &&
					Hash == In.Hash &&
					Extent3D == In.Extent3D;
			}
		};
		FRenderTargets RenderTargets;

		enum
		{
			// Bump every time serialization changes
			VERSION = 3,
		};

		FDiskEntry()
			: VertexInputKey(0)
			, bLoaded(false)
			, RasterizationSamples(0)
			, Topology(0)
			, PipelineLayout(VK_NULL_HANDLE)
			, RenderPass(nullptr)
		{
			FMemory::Memzero(Rasterizer);
			FMemory::Memzero(DepthStencil);
			FMemory::Memzero(ShaderModules);
		}

		~FDiskEntry();

		// Vulkan Runtime Data/Objects
		TArray<VkDescriptorSetLayout> DescriptorSetLayouts;
		VkPipelineLayout PipelineLayout;
		VkShaderModule ShaderModules[SF_Compute];
		const FVulkanRenderPass* RenderPass;

		bool operator==(const FDiskEntry& In) const
		{
			if (!(GraphicsKey == In.GraphicsKey))
			{
				return false;
			}

			if (VertexInputKey != In.VertexInputKey)
			{
				return false;
			}

			if( bLoaded != In.bLoaded)
			{
				return false;
			}

			if (RasterizationSamples != In.RasterizationSamples)
			{
				return false;
			}

			if (Topology != In.Topology)
			{
				return false;
			}

			if (ColorAttachmentStates != In.ColorAttachmentStates)
			{
				return false;
			}

			if (DescriptorSetLayoutBindings != In.DescriptorSetLayoutBindings)
			{
				return false;
			}

			if (!(Rasterizer == In.Rasterizer))
			{
				return false;
			}

			if (!(DepthStencil == In.DepthStencil))
			{
				return false;
			}

			for (int32 Index = 0; Index < ARRAY_COUNT(In.ShaderHashes); ++Index)
			{
				if (ShaderHashes[Index] != In.ShaderHashes[Index])
				{
					return false;
				}
			}

			for (int32 Index = 0; Index < ARRAY_COUNT(In.ShaderMicrocodes); ++Index)
			{
				if (ShaderMicrocodes[Index] != In.ShaderMicrocodes[Index])
				{
					return false;
				}
			}

			if (!(RenderTargets == In.RenderTargets))
			{
				return false;
			}

			if (VertexBindings != In.VertexBindings)
			{
				return false;
			}

			if (VertexAttributes != In.VertexAttributes)
			{
				return false;
			}

			return true;
		}
	};

private:
	FVulkanDevice* Device;

	// Map the runtime key to a pipeline pointer
	TMap<FVulkanPipelineStateKey, FVulkanPipeline*> KeyToPipelineMap;

	// Map used to delete the created pipelines
	TMap<FDiskEntry*, FVulkanPipeline*> CreatedPipelines;

	TIndirectArray<FDiskEntry> DiskEntries;

	VkPipelineCache PipelineCache;

	void CreatePipelineFromDiskEntry(const FDiskEntry* DiskEntry, FVulkanPipeline* Pipeline);
	void PopulateDiskEntry(const FVulkanPipelineState& State, const FVulkanRenderPass* RenderPass, FDiskEntry* OutDiskEntry);
	void CreateDiskEntryRuntimeObjects(FDiskEntry* DiskEntry);
	bool Load(const TArray<FString>& CacheFilenames, TArray<uint8>& OutDeviceCache);
	void DestroyCache();
};

#endif	// VULKAN_ENABLE_PIPELINE_CACHE
