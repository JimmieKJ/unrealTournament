// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanRHIPrivate.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

// Dependencies
#include "VulkanConfiguration.h"
#include "Engine.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVulkanRHI, Log, All);

/** How many back buffers to cycle through */
#define NUM_RENDER_BUFFERS 2

#ifndef VK_PROTOTYPES
#define VK_PROTOTYPES	1
#endif

#if PLATFORM_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#if PLATFORM_ANDROID
	#define VK_USE_PLATFORM_ANDROID_KHR 1
#endif


#include <vulkan/vulkan.h>

#include "VulkanRHI.h"
#include "VulkanGlobalUniformBuffer.h"
#include "RHI.h"


// Default is 1 (which is aniso off), the number is adjusted after the limits are queried.
static int32 GMaxVulkanTextureFilterAnisotropic = 1;

struct FVulkanQueue;
class FVulkanCmdBuffer;
struct FVulkanShader;
struct FVulkanDescriptorSetsLayout;
class FVulkanBlendState;
class FVulkanDepthStencilState;
class FVulkanBoundShaderState;
class FVulkanPipeline;
struct FVulkanRenderPass;
struct FVulkanCommandBufferManager;
struct FVulkanPendingState;

inline VkShaderStageFlagBits UEFrequencyToVKStageBit(EShaderFrequency InStage)
{
	VkShaderStageFlagBits OutStage = VK_SHADER_STAGE_ALL;

	switch (InStage)
	{
	case SF_Vertex:		OutStage = VK_SHADER_STAGE_VERTEX_BIT;						break;
	case SF_Hull:		OutStage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;		break;
	case SF_Domain:		OutStage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;		break;
	case SF_Pixel:		OutStage = VK_SHADER_STAGE_FRAGMENT_BIT;					break;
	case SF_Geometry:	OutStage = VK_SHADER_STAGE_GEOMETRY_BIT;					break;
	case SF_Compute:	OutStage = VK_SHADER_STAGE_COMPUTE_BIT;						break;
	default:
		checkf(false, TEXT("Undefined shader stage %d"), (int32)InStage);
		break;
	}

	check(OutStage != VK_SHADER_STAGE_ALL);
	return OutStage;
}

class FVulkanRenderTargetLayout
{
public:
	FVulkanRenderTargetLayout(const FRHISetRenderTargetsInfo& RTInfo);

	inline uint32 GetHash() const { return Hash; }
	inline const VkExtent2D& GetExtent2D() const { return Extent.Extent2D; }
	inline const VkExtent3D& GetExtent3D() const { return Extent.Extent3D; }
	inline const VkAttachmentDescription* GetAttachmentDescriptions() const { return Desc; }
	inline uint32 GetNumColorAttachments() const { return NumColorAttachments; }
	inline bool GetHasDepthStencil() const { return bHasDepthStencil; }
	inline bool GetHasResolveAttachments() const { return bHasResolveAttachments; }
	inline uint32 GetNumAttachments() const { return NumAttachments; }

	inline const VkAttachmentReference* GetColorAttachmentReferences() const { return NumColorAttachments > 0 ? ColorReferences : nullptr; }
	inline const VkAttachmentReference* GetResolveAttachmentReferences() const { return bHasResolveAttachments ? ResolveReferences : nullptr; }
	inline const VkAttachmentReference* GetDepthStencilAttachmentReference() const { return bHasDepthStencil ? &DepthStencilReference : nullptr; }

protected:
	VkAttachmentReference ColorReferences[MaxSimultaneousRenderTargets];
	VkAttachmentReference ResolveReferences[MaxSimultaneousRenderTargets];
	VkAttachmentReference DepthStencilReference;

	VkAttachmentDescription Desc[MaxSimultaneousRenderTargets * 2 + 1];

	uint32 NumAttachments;
	uint32 NumColorAttachments;
	bool bHasDepthStencil;
	bool bHasResolveAttachments;

	uint32 Hash;

	union
	{
		VkExtent3D	Extent3D;
		VkExtent2D	Extent2D;
	} Extent;

#if VULKAN_ENABLE_PIPELINE_CACHE
	FVulkanRenderTargetLayout()
	{
		FMemory::Memzero(ColorReferences);
		FMemory::Memzero(ResolveReferences);
		FMemory::Memzero(DepthStencilReference);
		FMemory::Memzero(Desc);
		NumAttachments = 0;
		NumColorAttachments = 0;
		bHasDepthStencil = 0;
		bHasResolveAttachments = 0;
		Hash = 0;
		Extent.Extent3D.width = 0;
		Extent.Extent3D.height = 0;
		Extent.Extent3D.depth = 0;
	}
	friend class FVulkanPipelineStateCache;
#endif
};

#include "VulkanDevice.h"

struct FVulkanSemaphore
{
	FVulkanSemaphore(FVulkanDevice& InDevice):
		Device(InDevice),
		SemaphoreHandle(VK_NULL_HANDLE)
	{
		// Create semaphore
		VkSemaphoreCreateInfo PresentCompleteSemaphoreCreateInfo;
		FMemory::Memzero(PresentCompleteSemaphoreCreateInfo);
		PresentCompleteSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		PresentCompleteSemaphoreCreateInfo.pNext = nullptr;
		PresentCompleteSemaphoreCreateInfo.flags = 0;
		VERIFYVULKANRESULT(vkCreateSemaphore(Device.GetInstanceHandle(), &PresentCompleteSemaphoreCreateInfo, nullptr, &SemaphoreHandle));
	}

	~FVulkanSemaphore()
	{
		check(SemaphoreHandle != VK_NULL_HANDLE);
		vkDestroySemaphore(Device.GetInstanceHandle(), SemaphoreHandle, nullptr);
	}

	VkSemaphore GetHandle() const
	{
		check(SemaphoreHandle != VK_NULL_HANDLE);
		return SemaphoreHandle;
	}
	
private:
	FVulkanDevice& Device;
	VkSemaphore SemaphoreHandle;
};

#include "VulkanQueue.h"
#include "VulkanCommandBuffer.h"

struct FVulkanDepthStencilView
{
	FVulkanDepthStencilView() :
#if 0
		View(VK_NULL_HANDLE),
#endif
		Format(VK_FORMAT_UNDEFINED)
	{
	}

	void Create(FVulkanDevice& Device, FVulkanTexture2D& Texture)
	{
		check(0);
#if 0
		VkDepthStencilViewCreateInfo ViewInfo;
		ViewInfo.sType = VK_STRUCTURE_TYPE_DEPTH_STENCIL_VIEW_CREATE_INFO;
		ViewInfo.pNext = nullptr;
		ViewInfo.image = Texture.Surface.Image;
		ViewInfo.mipLevel = 0;
		ViewInfo.baseArraySlice = 0;
		ViewInfo.arraySize = 1;
		ViewInfo.flags = 0;
		VERIFYVULKANRESULT(vkCreateDepthStencilView(Device.Device, &ViewInfo, &View));

		Format = Texture.Surface.InternalFormat;
#endif
	}

	void Destroy(FVulkanDevice& Device)
	{
		check(0);
#if 0
		if (View != VK_NULL_HANDLE)
		{
			vkDestroyObject(Device.Device, VK_OBJECT_TYPE_DEPTH_STENCIL_VIEW, View);
			View = VK_NULL_HANDLE;
		}
#endif
	}

#if 0
	VkDepthStencilView View;
#endif
	VkFormat Format;
};

struct FVulkanFramebuffer
{
	FVulkanFramebuffer(FVulkanDevice& Device, const FRHISetRenderTargetsInfo& RTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPass& RenderPass);

	bool Matches(const FRHISetRenderTargetsInfo& RTInfo) const;

	inline uint32 GetNumColorAttachments() const { return NumColorAttachments; }

	void Destroy(FVulkanDevice& Device);

	VkFramebuffer GetHandle()
	{
		check(Framebuffer != VK_NULL_HANDLE);
		return Framebuffer;
	}

	TArray<VkImageView> Attachments;
	TArray<VkImageSubresourceRange> SubresourceRanges;

	void InsertWriteBarrier(VkCommandBuffer cmd);

private:
	VkFramebuffer Framebuffer;

	// We do not adjust RTInfo, since it used for hashing and is what the UE provides,
	// it's up to VulkanRHI to handle this correctly.
	FRHISetRenderTargetsInfo RTInfo;
	uint32 NumColorAttachments;

	// Predefined set of barriers, when executes ensuring all writes are finished
	TArray<VkImageMemoryBarrier> WriteBarriers;
};

struct FVulkanRenderPass
{
	const FVulkanRenderTargetLayout& GetLayout() const { return Layout; }
	VkRenderPass GetHandle() const { check(RenderPass != VK_NULL_HANDLE); return RenderPass; }

private:
	friend struct FVulkanPendingState;

#if VULKAN_ENABLE_PIPELINE_CACHE
	friend class FVulkanPipelineStateCache;
#endif

	FVulkanRenderPass(FVulkanDevice& Device, const FVulkanRenderTargetLayout& RTLayout);
	~FVulkanRenderPass();

	void Begin(FVulkanCmdBuffer& CmdBuf, const VkFramebuffer& Framebuffer, const VkClearValue* AttachmentClearValues);
	void End(FVulkanCmdBuffer& CmdBuf)
	{
		vkCmdEndRenderPass(CmdBuf.GetHandle());
	}

private:
	FVulkanRenderTargetLayout Layout;
	VkRenderPass RenderPass;
	FVulkanDevice& Device;
};

struct FVulkanDescriptorSetsLayout
{
	FVulkanDescriptorSetsLayout(FVulkanDevice* InDevice);
	~FVulkanDescriptorSetsLayout();

	void AddDescriptor(int32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor, int32 BindingIndex);

	// Can be called only once, the idea is that the Layout remains fixed.
	void Compile();

	inline const TArray<VkDescriptorSetLayout> GetHandles() const
	{
		return LayoutHandles;
	}

	inline uint32 GetTypesUsed(VkDescriptorType Type) const
	{
		return LayoutTypes[Type];
	}

	struct FSetLayout
	{
		FSetLayout() : DescriptorSetIndex(-1) {}

		TArray<VkDescriptorSetLayoutBinding> LayoutBindings;
		int32 DescriptorSetIndex;
	};

	const TArray<FSetLayout>& GetLayouts() const
	{
		return Layouts;
	}

private:
	FVulkanDevice* Device;

	uint32 LayoutTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

	TArray<FSetLayout> Layouts;
	TArray<VkDescriptorSetLayout> LayoutHandles;
};

#include "VulkanPipeline.h"

class FVulkanDescriptorPool
{
public:
	FVulkanDescriptorPool(FVulkanDevice* InDevice);
	~FVulkanDescriptorPool();

	VkDescriptorPool GetHandle() const
	{
		return DescriptorPool;
	}

	void TrackAddUsage(const FVulkanDescriptorSetsLayout& Layout);
	void TrackRemoveUsage(const FVulkanDescriptorSetsLayout& Layout);

private:
	FVulkanDevice* Device;

	uint32 MaxDescriptorSets;
	uint32 NumAllocatedDescriptorSets;
	uint32 PeakAllocatedDescriptorSets;

	// Tracks number of allocated types, to ensure that we are not exceeding our allocated limit
	int32 MaxAllocatedTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	int32 NumAllocatedTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	int32 PeakAllocatedTypes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

	VkDescriptorPool DescriptorPool;
};

struct FVulkanDescriptorSets
{
	const TArray<VkDescriptorSet>& GetHandles() const
	{
		return Sets;
	}

	void Bind(FVulkanCmdBuffer* Cmd, FVulkanBoundShaderState* State);

private:
	friend class FVulkanDescriptorPool;
	friend struct FVulkanPendingState;

	FVulkanDescriptorSets(FVulkanDevice* InDevice, const FVulkanBoundShaderState* InState, FVulkanDescriptorPool* InPool);
	~FVulkanDescriptorSets();

	FVulkanDevice* Device;
	FVulkanDescriptorPool* Pool;
	const FVulkanDescriptorSetsLayout& Layout;
	TArray<VkDescriptorSet> Sets;
};


void VulkanSetImageLayout(VkCommandBuffer CmdBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout old_image_layout, VkImageLayout new_image_layout);

void VulkanImageCopy(VkCommandBuffer Cmd, FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI);

void VulkanResolveImage(VkCommandBuffer Cmd, FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI);

// Stats
#include "Engine.h"
#include "Stats2.h"
DECLARE_STATS_GROUP(TEXT("Vulkan RHI"), STATGROUP_VulkanRHI, STATCAT_Advanced);
//DECLARE_STATS_GROUP(TEXT("Vulkan RHI Verbose"), STATGROUP_VulkanRHIVERBOSE, STATCAT_Advanced);
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawPrimitive calls"), STAT_VulkanDrawPrimitiveCalls, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Triangles drawn"), STAT_VulkanTriangles, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lines drawn"), STAT_VulkanLines, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Allocated"), STAT_VulkanTexturesAllocated, STATGROUP_VulkanRHI, );
//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Textures Released"), STAT_VulkanTexturesReleased, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call time"), STAT_VulkanDrawCallTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call prep time"), STAT_VulkanDrawCallPrepareTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Create uniform buffer time"), STAT_VulkanCreateUniformBufferTime, STATGROUP_VulkanRHI, );
//DECLARE_CYCLE_STAT_EXTERN(TEXT("Update uniform buffer"), STAT_VulkanUpdateUniformBufferTime, STATGROUP_VulkanRHI, );
//DECLARE_CYCLE_STAT_EXTERN(TEXT("GPU Flip Wait Time"), STAT_VulkanGPUFlipWaitTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Get Or Create Pipeline"), STAT_VulkanGetOrCreatePipeline, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Get DescriptorSet"), STAT_VulkanGetDescriptorSet, STATGROUP_VulkanRHI, );
//DECLARE_CYCLE_STAT_EXTERN(TEXT("Create Pipeline"), STAT_VulkanCreatePipeline, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Pipeline Bind"), STAT_VulkanPipelineBind, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Bound Shader States"), STAT_VulkanNumBoundShaderState, STATGROUP_VulkanRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Render Passes"), STAT_VulkanNumRenderPasses, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Dynamic VB Size"), STAT_VulkanDynamicVBSize, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Dynamic IB Size"), STAT_VulkanDynamicIBSize, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic VB Lock/Unlock time"), STAT_VulkanDynamicVBLockTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic IB Lock/Unlock time"), STAT_VulkanDynamicIBLockTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("DrawPrim UP Prep Time"), STAT_VulkanUPPrepTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Uniform Buffer Creation Time"), STAT_VulkanUniformBufferCreateTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Apply DS Uniform Buffers"), STAT_VulkanApplyDSUniformBuffers, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("SRV Update Time"), STAT_VulkanSRVUpdateTime, STATGROUP_VulkanRHI, );
#if VULKAN_ENABLE_AGGRESSIVE_STATS
DECLARE_CYCLE_STAT_EXTERN(TEXT("Apply DS Shader Resources"), STAT_VulkanApplyDSResources, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update DescriptorSets"), STAT_VulkanUpdateDescriptorSets, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num Desc Sets Updated"), STAT_VulkanNumDescSets, STATGROUP_VulkanRHI, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Num WriteDescriptors Cmd"), STAT_VulkanNumUpdateDescriptors, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set Shader Param"), STAT_VulkanSetShaderParamTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Set unif Buffer"), STAT_VulkanSetUniformBufferTime, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("VkUpdate DS"), STAT_VulkanVkUpdateDS, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Clear Dirty DS State"), STAT_VulkanClearDirtyDSState, STATGROUP_VulkanRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Bind Vertex Streams"), STAT_VulkanBindVertexStreamsTime, STATGROUP_VulkanRHI, );

#endif


class FVulkanDynamicIndexBuffer : public FVulkanDynamicPooledBuffer
{
public:
	FVulkanDynamicIndexBuffer(FVulkanDevice& Device);

	//#todo-rco: Check if return value optimization is happening
	FORCEINLINE FVulkanDynamicLockInfo Lock(VkDeviceSize InSize)
	{
		INC_DWORD_STAT_BY(STAT_VulkanDynamicIBSize, InSize);
		SCOPE_CYCLE_COUNTER(STAT_VulkanDynamicIBLockTime);
		return FVulkanDynamicPooledBuffer::Lock(InSize);
	}

	FORCEINLINE void Unlock(FVulkanDynamicLockInfo LockInfo)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanDynamicIBLockTime);
		FVulkanDynamicPooledBuffer::Unlock(LockInfo);
	}
};

class FVulkanDynamicVertexBuffer : public FVulkanDynamicPooledBuffer
{
public:
	FVulkanDynamicVertexBuffer(FVulkanDevice& Device);

	//#todo-rco: Check if return value optimization is happening
	FORCEINLINE FVulkanDynamicLockInfo Lock(VkDeviceSize InSize)
	{
		INC_DWORD_STAT_BY(STAT_VulkanDynamicVBSize, InSize);
		SCOPE_CYCLE_COUNTER(STAT_VulkanDynamicVBLockTime);
		return FVulkanDynamicPooledBuffer::Lock(InSize);
	}

	FORCEINLINE void Unlock(FVulkanDynamicLockInfo LockInfo)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanDynamicVBLockTime);
		FVulkanDynamicPooledBuffer::Unlock(LockInfo);
	}
};

#if VULKAN_HAS_DEBUGGING_ENABLED
extern TAutoConsoleVariable<int32> GValidationCvar;
#endif
