// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanTexture.cpp: Vulkan texture RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"


static FCriticalSection GTextureMapLock;
static TMap<FRHIResource*, VulkanRHI::FStagingBuffer*> GPendingLockedBuffers;

static const VkImageTiling GVulkanViewTypeTilingMode[VK_IMAGE_VIEW_TYPE_RANGE_SIZE] =
{
	VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D
    VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_2D
    VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_3D
    VK_IMAGE_TILING_OPTIMAL,	// VK_IMAGE_VIEW_TYPE_CUBE
    VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_1D_ARRAY
    VK_IMAGE_TILING_OPTIMAL,		// VK_IMAGE_VIEW_TYPE_2D_ARRAY
    VK_IMAGE_TILING_LINEAR,		// VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
};


VkImage FVulkanSurface::CreateImage(
	FVulkanDevice& InDevice,
	VkImageViewType ResourceType,
	EPixelFormat InFormat,
	uint32 SizeX, uint32 SizeY, uint32 SizeZ,
	bool bArray, uint32 ArraySize,
	uint32 NumMips,
	uint32 NumSamples,
	uint32 UEFlags,
	VkFormat* OutFormat,
	VkImageCreateInfo* OutInfo,
	VkMemoryRequirements* OutMemoryRequirements,
	bool bForceLinearTexture)
{
	const VkPhysicalDeviceProperties& DeviceProperties = InDevice.GetDeviceProperties();

	checkf(GPixelFormats[InFormat].Supported, TEXT("Format %d"), InFormat);

	VkImageCreateInfo TmpCreateInfo;
	VkImageCreateInfo& ImageCreateInfo = OutInfo ? *OutInfo : TmpCreateInfo;
	FMemory::Memzero(ImageCreateInfo);
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

	switch(ResourceType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_1D;
		check(SizeX <= DeviceProperties.limits.maxImageDimension1D);
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		check(SizeX == SizeY);
		check(SizeX <= DeviceProperties.limits.maxImageDimensionCube);
		check(SizeY <= DeviceProperties.limits.maxImageDimensionCube);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		check(SizeX <= DeviceProperties.limits.maxImageDimension2D);
		check(SizeY <= DeviceProperties.limits.maxImageDimension2D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		check(SizeY <= DeviceProperties.limits.maxImageDimension3D);
		ImageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		break;
	default:
		checkf(false, TEXT("Unhandled image type %d"), ResourceType);
		break;
	}

	ImageCreateInfo.format = UEToVkFormat(InFormat, (UEFlags & TexCreate_SRGB) == TexCreate_SRGB);

	checkf(ImageCreateInfo.format != VK_FORMAT_UNDEFINED, TEXT("Pixel Format %d not defined!"), InFormat);
	if (OutFormat)
	{
		*OutFormat = ImageCreateInfo.format;
	}

	ImageCreateInfo.extent.width = SizeX;
	ImageCreateInfo.extent.height = SizeY;
	ImageCreateInfo.extent.depth = ResourceType == VK_IMAGE_VIEW_TYPE_3D ? SizeZ : 1;
	ImageCreateInfo.mipLevels = NumMips;
	uint32 LayerCount = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;
	ImageCreateInfo.arrayLayers = (bArray ? SizeZ : 1) * LayerCount;
	check(ImageCreateInfo.arrayLayers <= DeviceProperties.limits.maxImageArrayLayers);

	ImageCreateInfo.flags = (ResourceType == VK_IMAGE_VIEW_TYPE_CUBE) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

	ImageCreateInfo.tiling = bForceLinearTexture ? VK_IMAGE_TILING_LINEAR : GVulkanViewTypeTilingMode[ResourceType];

	ImageCreateInfo.usage = 0;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	//@TODO: should everything be created with the source bit?
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (UEFlags & TexCreate_Presentable)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;		
	}
	else if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
	{
		ImageCreateInfo.usage |= (UEFlags & TexCreate_RenderTargetable) ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & (TexCreate_DepthStencilResolveTarget))
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & TexCreate_ResolveTargetable)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else if (UEFlags & TexCreate_UAV)
	{
		ImageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}

	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.queueFamilyIndexCount = 0;
	ImageCreateInfo.pQueueFamilyIndices = nullptr;

	if(ResourceType == VK_IMAGE_VIEW_TYPE_3D)
	{
		check(true);
	}

	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Not allowed to create Linear textures with %d samples, reverting to 1 sample"), NumSamples);
		NumSamples = 1;
	}

	switch (NumSamples)
	{
	case 1:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		break;
	case 2:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
		break;
	case 4:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
		break;
	case 8:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
		break;
	case 16:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
		break;
	case 32:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_32_BIT;
		break;
	case 64:
		ImageCreateInfo.samples = VK_SAMPLE_COUNT_64_BIT;
		break;
	default:
		checkf(0, TEXT("Unsupported number of samples %d"), NumSamples);
		break;
	}

//#todo-rco: Verify flags work on newer Android drivers
#if !PLATFORM_ANDROID
	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].linearTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
	else if (ImageCreateInfo.tiling == VK_IMAGE_TILING_OPTIMAL)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].optimalTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ensure((ImageCreateInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0);
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
#endif

	VkImage Image;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(InDevice.GetInstanceHandle(), &ImageCreateInfo, nullptr, &Image));

	// Fetch image size
	VkMemoryRequirements tmpMemReq;
	VkMemoryRequirements& MemReq = OutMemoryRequirements ? *OutMemoryRequirements : tmpMemReq;
	VulkanRHI::vkGetImageMemoryRequirements(InDevice.GetInstanceHandle(), Image, &MemReq);

	return Image;
}


static TArray<VkFormat> GVulkanPixelFormatKeys;

FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 InNumMips,
								uint32 InNumSamples, uint32 InUEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(VK_NULL_HANDLE)
	, InternalFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, Format(InFormat)
	, UEFlags(InUEFlags)
	, FormatKey(0)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(true)
	, Allocation(nullptr)
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, FullAspectMask(0)
	, PartialAspectMask(0)
{
	VkImageCreateInfo ImageCreateInfo;	// Zeroed inside CreateImage
	VkMemoryRequirements MemoryRequirements; // Zeroed inside CreateImage
	Image = FVulkanSurface::CreateImage(InDevice, ResourceType,
		InFormat, SizeX, SizeY, SizeZ,
		bArray, ArraySize, NumMips, NumSamples, UEFlags,
		&InternalFormat,
		&ImageCreateInfo, &MemoryRequirements);

	FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(Format, true, true);
	PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(Format, false, true);

	// If VK_IMAGE_TILING_OPTIMAL is specified,
	// memoryTypeBits in vkGetImageMemoryRequirements will become 1
	// which does not support VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
	if (ImageCreateInfo.tiling != VK_IMAGE_TILING_OPTIMAL)
	{
		MemProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}

	const bool bRenderTarget = (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable)) != 0;
	const bool bCPUReadback = (UEFlags & TexCreate_CPUReadback) != 0;
	const bool bDynamic = (UEFlags & TexCreate_Dynamic) != 0;

	if (!bDynamic && !bCPUReadback)
	{
		ResourceAllocation = InDevice.GetResourceHeapManager().AllocateImageMemory(MemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, __FILE__, __LINE__);
		ResourceAllocation->BindImage(Device, Image);
	}
	else
	{
		Allocation = InDevice.GetMemoryManager().Alloc(MemoryRequirements.size, MemoryRequirements.memoryTypeBits, MemProps, __FILE__, __LINE__);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("** vkBindImageMemory Buf %p MemHandle %p MemOffset %d Size %u\n"), (void*)Image, (void*)Allocation->GetHandle(), (uint32)0, (uint32)MemoryRequirements.size);
		VERIFYVULKANRESULT(VulkanRHI::vkBindImageMemory(Device->GetInstanceHandle(), Image, Allocation->GetHandle(), 0));
	}


	Tiling = ImageCreateInfo.tiling;
	check(Tiling == VK_IMAGE_TILING_LINEAR || Tiling == VK_IMAGE_TILING_OPTIMAL);

	// get a unique key for this surface's format, only needed for RTs
	if (UEFlags & (TexCreate_RenderTargetable /*| TexCreate_DepthStencilTargetable*/ | TexCreate_ResolveTargetable))
	{
		static TMap<VkFormat, uint8> PixelFormatKeyMap;
		static uint8 NextKey = 0;
		uint8* Key = PixelFormatKeyMap.Find(InternalFormat);
		if (Key == NULL)
		{
			Key = &PixelFormatKeyMap.Add(InternalFormat, NextKey++);
			GVulkanPixelFormatKeys.Add(InternalFormat);

			// only giving NUMBITS_RENDER_TARGET_FORMAT bits to the key
			checkf(NextKey < 16, TEXT("Too many unique pixel formats to fit into the PipelineStateHash"));
		}

		// set the key
		FormatKey = *Key;
	}

	if ((ImageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)))
	{
		const bool bTransitionToPresentable = ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable);
		InitialClear(CreateInfo.ClearValueBinding, bTransitionToPresentable);
	}
}

// This is usually used for the framebuffer image
FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage,
								uint32 InUEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(InImage)
	, InternalFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, Format(InFormat)
	, UEFlags(InUEFlags)
	, FormatKey(0)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(false)
	, Allocation(nullptr)
	, NumMips(0)
	, NumSamples(1)	// This is defined by the MSAA setting...
	, FullAspectMask(0)
	, PartialAspectMask(0)
{
	InternalFormat = (VkFormat)GPixelFormats[Format].PlatformFormat;
	FullAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(Format, true, true);
	PartialAspectMask = VulkanRHI::GetAspectMaskFromUEFormat(Format, false, true);

	// Purely informative patching, we know that "TexCreate_Presentable" uses optimal tiling
	if ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable && GetTiling() == VK_IMAGE_TILING_MAX_ENUM)
	{
		Tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	
	if (Image != VK_NULL_HANDLE)
	{
		if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
		{
			const bool bTransitionToPresentable = ((UEFlags & TexCreate_Presentable) == TexCreate_Presentable);
			InitialClear(CreateInfo.ClearValueBinding, bTransitionToPresentable);
		}
	}
}

FVulkanSurface::~FVulkanSurface()
{
	check(Device)
	Destroy();
}

void FVulkanSurface::Destroy()
{
	// Verify all buffers are unmapped
	for(const auto& MappingPair : MipMapMapping)
	{
		checkf(MappingPair.Value == nullptr, TEXT("The buffer needs to be unmapped, before a surface can be destroyed"));
	}
	
	check(Device)

	// An image can be instances.
	// - Instances VkImage has "bIsImageOwner" set to "false".
	// - Owner of VkImage has "bIsImageOwner" set to "true".
	if (bIsImageOwner)
	{
		bIsImageOwner = false;

		if (Image != VK_NULL_HANDLE)
		{
			Device->GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Image, Image);
			Image = VK_NULL_HANDLE;
		}

		if (Allocation)
		{
			Device->GetMemoryManager().Free(Allocation);
			Allocation = nullptr;
		}
	}
}

#if 0
void* FVulkanSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	DestStride = 0;

	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	// Verify all buffers are unmapped
	auto& Data = MipMapMapping.FindOrAdd(MipIndex);
	checkf(Data == nullptr, TEXT("The buffer needs to be unmapped, before it can be mapped"));

	// Get the layout of the subresource
	VkImageSubresource ImageSubResource;
	FMemory::Memzero(ImageSubResource);

	ImageSubResource.aspectMask = GetAspectMask();
	ImageSubResource.mipLevel = MipIndex;
	ImageSubResource.arrayLayer = ArrayIndex;

	// Get buffer size
	// Pitch can be only retrieved from linear textures.
	VkSubresourceLayout SubResourceLayout;
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	// Set linear row-pitch
	GetMipStride(MipIndex, DestStride);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		// Verify pitch if linear
		check(DestStride == SubResourceLayout.rowPitch);

		// Map buffer to a pointer
		Data = Allocation->Map(SubResourceLayout.size, SubResourceLayout.offset);
		return Data;
	}

	// From here on, the code is dedicated to optimal textures

	// Verify all buffers are unmapped
	TRefCountPtr<FVulkanBuffer>& LinearBuffer = MipMapBuffer.FindOrAdd(MipIndex);
	checkf(LinearBuffer == nullptr, TEXT("The buffer needs to be unmapped, before it can be mapped"));

	// Create intermediate buffer which is going to be used to perform buffer to image copy
	// The copy buffer is always one face and single mip level.
	const uint32 Layers = 1;
	const uint32 Bytes = SubResourceLayout.size * Layers;

	VkBufferUsageFlags Usage = 0;
	Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VkMemoryPropertyFlags Flags = 0;
	Flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT;

	LinearBuffer = new FVulkanBuffer(*Device, Bytes, Usage, Flags, false, __FILE__, __LINE__);

	void* DataPtr = LinearBuffer->Lock(Bytes);
	check(DataPtr);

	return DataPtr;
}

void FVulkanSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		void*& Data = MipMapMapping.FindOrAdd(MipIndex);
		checkf(Data != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));

		Allocation->Unmap();
		Data = nullptr;
		return;
	}

	TRefCountPtr<FVulkanBuffer>& LinearBuffer = MipMapBuffer.FindOrAdd(MipIndex);
	checkf(LinearBuffer != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));
	LinearBuffer->Unlock();

	VkImageSubresource ImageSubResource;
	FMemory::Memzero(ImageSubResource);
	ImageSubResource.aspectMask = GetAspectMask();
	ImageSubResource.mipLevel = MipIndex;
	ImageSubResource.arrayLayer = ArrayIndex;

	VkSubresourceLayout SubResourceLayout;
	
	VulkanRHI::vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	Region.bufferOffset = 0;
	Region.bufferRowLength = (Width >> MipIndex);
	Region.bufferImageHeight = (Height >> MipIndex);

	// The data/image is always parsed per one face.
	// Meaning that a cubemap will have atleast 6 locks/unlocks
	Region.imageSubresource.baseArrayLayer = ArrayIndex;	// Layer/face copy destination
	Region.imageSubresource.layerCount = 1;	// Indicates number of arrays in the buffer, this is also the amount of "faces/layers" to be copied
	Region.imageSubresource.aspectMask = GetAspectMask();
	Region.imageSubresource.mipLevel = MipIndex;

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = 1;

	LinearBuffer->CopyTo(*this, Region, nullptr);

	// Release buffer
	LinearBuffer = nullptr;
}
#endif

uint32 FVulkanSurface::GetMemorySize()
{
	//#todo-rco: Slow!
	VkMemoryRequirements MemReqs;
	VulkanRHI::vkGetImageMemoryRequirements(Device->GetInstanceHandle(), Image, &MemReqs);

	return MemReqs.size;
}

void FVulkanSurface::GetMipStride(uint32 MipIndex, uint32& Stride)
{
	// Calculate the width of the MipMap.
	const uint32 BlockSizeX = GPixelFormats[Format].BlockSizeX;
	const uint32 MipSizeX = FMath::Max(Width >> MipIndex, BlockSizeX);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;

	if (Format == PF_PVRTC2 || Format == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
	}

	const uint32 BlockBytes = GPixelFormats[Format].BlockBytes;

	Stride = NumBlocksX * BlockBytes;
}

void FVulkanSurface::GetMipOffset(uint32 MipIndex, uint32& Offset)
{
	uint32 offset = Offset = 0;
	for(uint32 i = 0; i < MipIndex; i++)
	{
		GetMipSize(i, offset);
		Offset += offset;
	}
}

void FVulkanSurface::GetMipSize(uint32 MipIndex, uint32& MipBytes)
{
	// Calculate the dimensions of mip-map level.
	const uint32 BlockSizeX = GPixelFormats[Format].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[Format].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[Format].BlockBytes;
	const uint32 MipSizeX = FMath::Max(Width >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(Height >> MipIndex, BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;

	if (Format == PF_PVRTC2 || Format == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}

	// Size in bytes
	MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
}

void FVulkanSurface::InitialClear(const FClearValueBinding& ClearValueBinding, bool bTransitionToPresentable)
{
	VkImageSubresourceRange Range;
	FMemory::Memzero(Range);
	Range.aspectMask = FullAspectMask;
	Range.baseMipLevel = 0;
	Range.levelCount = NumMips;
	Range.baseArrayLayer = 0;
	Range.layerCount = ViewType == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;

	//#todo-rco: This function is only used during loading currently, if used for regular RHIClear then use the ActiveCmdBuffer
	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	if (Range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
	{
		VkClearColorValue Color;
		FMemory::Memzero(Color);
		Color.float32[0] = ClearValueBinding.Value.Color[0];
		Color.float32[1] = ClearValueBinding.Value.Color[1];
		Color.float32[2] = ClearValueBinding.Value.Color[2];
		Color.float32[3] = ClearValueBinding.Value.Color[3];
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &Range);
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_GENERAL, bTransitionToPresentable ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	else
	{
		check(FullAspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
		check(!bTransitionToPresentable);
		VkClearDepthStencilValue Value;
		FMemory::Memzero(Value);
		Value.depth = ClearValueBinding.Value.DSValue.Depth;
		Value.stencil = ClearValueBinding.Value.DSValue.Stencil;
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, FullAspectMask);
		VulkanRHI::vkCmdClearDepthStencilImage(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_GENERAL, &Value, 1, &Range);
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, FullAspectMask);
	}
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FVulkanDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
}

bool FVulkanDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	return false;
}

uint32 FVulkanDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return FVulkanTextureBase::Cast(TextureRHI)->Surface.GetMemorySize();
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTexture2DRHIRef FVulkanDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTexture2D(*Device, (EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogVulkan, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	VULKAN_SIGNAL_UNIMPLEMENTED(); // Unsupported atm
	return FTexture2DRHIRef();
}

void FVulkanDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

FTexture2DArrayRHIRef FVulkanDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTexture2DArray(*Device, (EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTexture3DRHIRef FVulkanDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FVulkanTexture3D* Tex3d = new FVulkanTexture3D(*Device, (EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);

	return Tex3d;
}

void FVulkanDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	if (FRHITexture2D* Tex2D = Ref->GetTexture2D())
	{
		OutInfo.VRamAllocation.AllocationSize = ((FVulkanTexture2D*)Tex2D)->Surface.GetMemorySize();
	}
	else if (FRHITextureCube* TexCube = Ref->GetTextureCube())
	{
		OutInfo.VRamAllocation.AllocationSize = ((FVulkanTextureCube*)TexCube)->Surface.GetMemorySize();
	}
	else if (FRHITexture3D* Tex3D = Ref->GetTexture3D())
	{
		OutInfo.VRamAllocation.AllocationSize = ((FVulkanTexture3D*)Tex3D)->Surface.GetMemorySize();
	}
}

void FVulkanDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SourceSurfaceRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	FVulkanTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	return nullptr;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	return TexRealloc_Failed;
}

ETextureReallocationStatus FVulkanDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	VULKAN_SIGNAL_UNIMPLEMENTED();

	return TexRealloc_Failed;
}

void* FVulkanDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(TextureRHI);
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(TextureRHI, StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	EPixelFormat Format = Texture->Surface.Format;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;
	VulkanSetImageLayout(StagingCommandBuffer, Texture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	//Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;
	VulkanRHI::vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VulkanSetImageLayout(StagingCommandBuffer, Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SubresourceRange);

	Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
}

void* FVulkanDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;//Texture->Surface.Lock(MipIndex, TextureIndex, LockMode, DestStride);
}

void FVulkanDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	VULKAN_SIGNAL_UNIMPLEMENTED();
	//Texture->Surface.Unlock(MipIndex, TextureIndex);
}

void FVulkanDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	FVulkanTexture3D* Texture = ResourceCast(TextureRHI);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);

	VkFormat Format = UEToVkFormat(PixelFormat, false);

	// TO DO - add appropriate offsets to source data when necessary
	check(UpdateRegion.SrcX == 0);
	check(UpdateRegion.SrcY == 0);
	check(UpdateRegion.SrcZ == 0);

	FVulkanCommandListContext& Context = Device->GetImmediateContext();
	const VkPhysicalDeviceLimits& Limits = Device->GetLimits();
	const uint32 AlignedSourcePitch = Align(SourceRowPitch, Limits.optimalBufferCopyRowPitchAlignment);
	const uint32 BufferSize = Align(UpdateRegion.Height * UpdateRegion.Depth * AlignedSourcePitch, Limits.minMemoryMapAlignment);

	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);
	void* Memory = StagingBuffer->GetMappedPointer();

	uint32 Size = UpdateRegion.Height * UpdateRegion.Depth * SourceRowPitch;
	check(Size <= BufferSize);

	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = 1;
	VulkanSetImageLayout(StagingCommandBuffer, Texture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

	//#todo-rco: Probably needs some work (see D3D12)
	FMemory::Memcpy(Memory, SourceData, Size);
	//FMemory::Memzero(Memory, Size);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	check(UpdateRegion.SrcX == 0);
	check(UpdateRegion.SrcY == 0);
	check(UpdateRegion.SrcZ == 0);
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = 1;
	Region.imageOffset.x = UpdateRegion.SrcX;
	Region.imageOffset.y = UpdateRegion.SrcY;
	Region.imageOffset.z = UpdateRegion.SrcZ;
	Region.imageExtent.width = UpdateRegion.Width;
	Region.imageExtent.height = UpdateRegion.Height;
	Region.imageExtent.depth = UpdateRegion.Depth;
	VulkanRHI::vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VulkanSetImageLayout(StagingCommandBuffer, Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SubresourceRange);

	Device->GetStagingManager().ReleaseBuffer(nullptr, StagingBuffer);
}


VkImageView FVulkanTextureView::StaticCreate(FVulkanDevice& Device, VkImage Image, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle)
{
	VkImageView View = VK_NULL_HANDLE;

	VkImageViewCreateInfo ViewInfo;
	FMemory::Memzero(ViewInfo);
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.pNext = nullptr;
	ViewInfo.image = Image;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = Format;
	if (bUseIdentitySwizzle)
	{
		// VK_COMPONENT_SWIZZLE_IDENTITY == 0 and this was memzero'd already
	}
	else
	{
		ViewInfo.components = Device.GetFormatComponentMapping(UEFormat);
	}

	uint32 FaceCount = (ViewType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;
	ViewInfo.subresourceRange.aspectMask = AspectFlags;
	ViewInfo.subresourceRange.baseMipLevel = FirstMip;
	ViewInfo.subresourceRange.levelCount = NumMips;
	ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
	ViewInfo.subresourceRange.layerCount = NumArraySlices * FaceCount;

	//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	if (UEFormat == PF_X24_G8)
	{
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	VERIFYVULKANRESULT(VulkanRHI::vkCreateImageView(Device.GetInstanceHandle(), &ViewInfo, nullptr, &View));

	return View;
}

void FVulkanTextureView::Create(FVulkanDevice& Device, VkImage Image, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices)
{
	View = StaticCreate(Device, Image, ViewType, AspectFlags, UEFormat, Format, FirstMip, NumMips, ArraySliceIndex, NumArraySlices);
/*
	switch (AspectFlags)
	{
	case VK_IMAGE_ASPECT_DEPTH_BIT:
	case VK_IMAGE_ASPECT_STENCIL_BIT:
	case VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT:
		Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		break;
	default:
		ensure(0);
	case VK_IMAGE_ASPECT_COLOR_BIT:
		Layout = VK_IMAGE_LAYOUT_GENERAL;
		break;
	}
*/
}

void FVulkanTextureView::Destroy(FVulkanDevice& Device)
{
	if (View)
	{
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ImageView, View);
		View = VK_NULL_HANDLE;
	}
}

FVulkanTextureBase* FVulkanTextureBase::Cast(FRHITexture* Texture)
{
	check(Texture);
	FVulkanTextureBase* OutTexture = nullptr;

	if(Texture->GetTexture2D())
	{
		OutTexture = static_cast<FVulkanTexture2D*>(Texture->GetTexture2D());
	}
	else if(Texture->GetTexture2DArray())
	{
		OutTexture = static_cast<FVulkanTexture2DArray*>(Texture->GetTexture2DArray());
	}
	else if(Texture->GetTexture3D())
	{
		OutTexture = static_cast<FVulkanTexture3D*>(Texture->GetTexture3D());
	}
	else if(Texture->GetTextureCube())
	{
		OutTexture = static_cast<FVulkanTextureCube*>(Texture->GetTextureCube());
	}
	else if(Texture->GetTextureReference())
	{
		FVulkanTextureReference* ref = static_cast<FVulkanTextureReference*>(Texture->GetTextureReference());
		OutTexture = FVulkanTextureBase::Cast(ref->GetReferencedTexture());
	}

	check(OutTexture);
	return OutTexture;
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	 : Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, NumSamples, UEFlags, CreateInfo)
	#else
	 : Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, (UEFlags & TexCreate_DepthStencilTargetable) ? NumSamples : 1, UEFlags, CreateInfo)
	 , MSAASurface(nullptr)
	#endif
	, PartialView(nullptr)
{
	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Surface.Format, Surface.InternalFormat, 0, FMath::Max(NumMips, 1u), 0, FMath::Max(1u, ArraySize));
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Create MSAA surface. The surface above is the resolve target
	if (NumSamples > 1 && (UEFlags & TexCreate_RenderTargetable) && !(UEFlags & TexCreate_DepthStencilTargetable))
	{
		MSAASurface = new FVulkanSurface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, NumSamples, UEFlags, CreateInfo);
		if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
		{
			MSAAView.Create(Device, MSAASurface->Image, ResourceType, MSAASurface->GetFullAspectMask(), MSAASurface->Format, MSAASurface->InternalFormat, FMath::Max(NumMips, 1u), FMath::Max(1u, ArraySize));
		}
	}
#endif

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.Format, Surface.InternalFormat, 0, FMath::Max(NumMips, 1u), 0, FMath::Max(1u, ArraySize));
	}

	if (!CreateInfo.BulkData)
	{
		return;
	}

	// Transfer bulk data
	VulkanRHI::FStagingBuffer* StagingBuffer = Device.GetStagingManager().AcquireBuffer(CreateInfo.BulkData->GetResourceBulkDataSize());
	void* Data = StagingBuffer->GetMappedPointer();

	// Do copy
	FMemory::Memcpy(Data, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());

	FVulkanCmdBuffer* CmdBuffer = Device.GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Use real Buffer offset when switching to suballocations!
	Region.bufferOffset = 0;
	Region.bufferRowLength = Surface.Width;
	Region.bufferImageHeight = Surface.Height;
	
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = ArraySize;
	Region.imageSubresource.aspectMask = Surface.GetFullAspectMask();

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = Surface.Depth;

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//SubresourceRange.baseMipLevel = 0;
	SubresourceRange.levelCount = Surface.GetNumMips();
	//SubresourceRange.baseArrayLayer = 0;
	SubresourceRange.layerCount = ArraySize;
	VulkanSetImageLayout(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

	// Copy buffer to image
	VulkanRHI::vkCmdCopyBufferToImage(CmdBuffer->GetHandle(), StagingBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VulkanSetImageLayout(CmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SubresourceRange);

	Device.GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo):
	 Surface(Device, ResourceType, Format, SizeX, SizeY, SizeZ, InImage, UEFlags, CreateInfo)
	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	, MSAASurface(nullptr)
	#endif
{
	check(InMem == VK_NULL_HANDLE);
	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM && Surface.Image != VK_NULL_HANDLE)
	{
		DefaultView.Create(Device, Surface.Image, ResourceType, Surface.GetFullAspectMask(), Format, Surface.InternalFormat, 0, 1, 0, 1);
	}

	if (Surface.FullAspectMask == Surface.PartialAspectMask)
	{
		PartialView = &DefaultView;
	}
	else
	{
		PartialView = new FVulkanTextureView;
		check(SizeZ == 1 && ResourceType == VK_IMAGE_VIEW_TYPE_2D);
		PartialView->Create(Device, Surface.Image, Surface.ViewType, Surface.PartialAspectMask, Surface.Format, Surface.InternalFormat, 0, FMath::Max(Surface.NumMips, 1u), 0, 1u);
	}
}

FVulkanTextureBase::~FVulkanTextureBase()
{
	if (PartialView != &DefaultView)
	{
		PartialView->Destroy(*Surface.Device);
		delete PartialView;
	}

	DefaultView.Destroy(*Surface.Device);

	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (MSAASurface)
	{
		delete MSAASurface;
		MSAASurface = nullptr;
	}
	#endif
}

VkImageView FVulkanTextureBase::CreateRenderTargetView(uint32 MipIndex, uint32 ArraySliceIndex)
{
	return FVulkanTextureView::StaticCreate(*Surface.Device, Surface.Image, Surface.GetViewType(), Surface.GetFullAspectMask(), Surface.Format, Surface.InternalFormat, MipIndex, 1, ArraySliceIndex, 1, true);
}


FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
:	FRHITexture2D(SizeX, SizeY, FMath::Max(NumMips, 1u), NumSamples, InFormat, UEFlags, CreateInfo.ClearValueBinding)
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, InFormat, SizeX, SizeY, 1, /*bArray=*/ false, 1, FMath::Max(NumMips, 1u), NumSamples, UEFlags, CreateInfo)
{
}

FVulkanTexture2D::FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags,const FRHIResourceCreateInfo& CreateInfo)
:	FRHITexture2D(SizeX, SizeY, 1, 1, Format, UEFlags, CreateInfo.ClearValueBinding)
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D, Format, SizeX, SizeY, 1, Image, VK_NULL_HANDLE, UEFlags)
{
}

FVulkanTexture2D::~FVulkanTexture2D()
{
	if ((GetFlags() & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(this);
	}
}


FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags)
	: FVulkanTexture2D(Device, Format, SizeX, SizeY, Image, UEFlags, FRHIResourceCreateInfo())
{
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	DefaultView.View = VK_NULL_HANDLE;
	Surface.Image = VK_NULL_HANDLE;
}


void FVulkanTextureReference::SetReferencedTexture(FRHITexture* InTexture)
{
	FRHITextureReference::SetReferencedTexture(InTexture);
}


FVulkanTextureCube::FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
:	 FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
	//#todo-rco: Array/slices count
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_CUBE, Format, Size, Size, 1, bArray, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTextureCube::~FVulkanTextureCube()
{
	if ((GetFlags() & (TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable)) != 0)
	{
		Surface.Device->NotifyDeletedRenderTarget(this);
	}

	Destroy(*Surface.Device);
}

void FVulkanTextureCube::Destroy(FVulkanDevice& Device)
{
	DefaultView.Destroy(Device);
	Surface.Destroy();
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTextureCube(*Device, (EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTextureCubeRHIRef FVulkanDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanTextureCube(*Device, (EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void* FVulkanDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VulkanRHI::FStagingBuffer** StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingBuffer = &GPendingLockedBuffers.FindOrAdd(TextureCubeRHI);
		checkf(!*StagingBuffer, TEXT("Can't lock the same texture twice!"));
	}

	uint32 BufferSize = 0;
	DestStride = 0;
	Texture->Surface.GetMipSize(MipIndex, BufferSize);
	Texture->Surface.GetMipStride(MipIndex, DestStride);
	*StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize);

	void* Data = (*StagingBuffer)->GetMappedPointer();
	return Data;
}

void FVulkanDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GPendingLockedBuffers.RemoveAndCopyValue(TextureCubeRHI, StagingBuffer);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	VkCommandBuffer StagingCommandBuffer = CmdBuffer->GetHandle();
	EPixelFormat Format = Texture->Surface.Format;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageSubresourceRange SubresourceRange;
	FMemory::Memzero(SubresourceRange);
	SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SubresourceRange.baseMipLevel = MipIndex;
	SubresourceRange.levelCount = 1;
	SubresourceRange.baseArrayLayer = FaceIndex;
	SubresourceRange.layerCount = 1;
	VulkanSetImageLayout(StagingCommandBuffer, Texture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	//#todo-rco: Might need an offset here?
	//Region.bufferOffset = 0;
	//Region.bufferRowLength = 0;
	//Region.bufferImageHeight = 0;
	Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.imageSubresource.mipLevel = MipIndex;
	Region.imageSubresource.baseArrayLayer = FaceIndex;
	Region.imageSubresource.layerCount = 1;
	Region.imageExtent.width = MipWidth;
	Region.imageExtent.height = MipHeight;
	Region.imageExtent.depth = 1;
	VulkanRHI::vkCmdCopyBufferToImage(StagingCommandBuffer, StagingBuffer->GetHandle(), Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VulkanSetImageLayout(StagingCommandBuffer, Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SubresourceRange);

	Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_DUMP_LAYER
	{
		if (FRHITexture2D* Tex2d = TextureRHI->GetTexture2D())
		{
			FVulkanTexture2D* VTex2d = (FVulkanTexture2D*)Tex2d;
			VulkanRHI::PrintfBegin(FString::Printf(TEXT("vkDebugMarkerSetObjectNameEXT(%p=%s)"), VTex2d->Surface.Image, Name));
		}
		else if (FRHITextureCube* TexCube = TextureRHI->GetTextureCube())
		{
			FVulkanTextureCube* VTexCube = (FVulkanTextureCube*)TexCube;
			VulkanRHI::PrintfBegin(FString::Printf(TEXT("vkDebugMarkerSetObjectNameEXT(%p=%s)"), VTexCube->Surface.Image, Name));
		}
		else if (FRHITexture3D* Tex3d = TextureRHI->GetTexture3D())
		{
			FVulkanTexture3D* VTex3d = (FVulkanTexture3D*)Tex3d;
			VulkanRHI::PrintfBegin(FString::Printf(TEXT("vkDebugMarkerSetObjectNameEXT(%p=%s)"), VTex3d->Surface.Image, Name));
		}
}
#endif

#if VULKAN_ENABLE_DRAW_MARKERS
	//if (Device->SupportsDebugMarkers())
	/*
	{
		if (FRHITexture2D* Tex2d = TextureRHI->GetTexture2D())
		{
			// Lambda so the char* pointer is valid
			auto DoCall = [](PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectName, VkDevice VulkanDevice, FVulkanTexture2D* VulkanTexture, const char* ObjectName)
			{
				VkDebugMarkerObjectNameInfoEXT Info;
				FMemory::Memzero(Info);
				Info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
				Info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
				Info.object = (uint64)VulkanTexture->Surface.Image;
				Info.pObjectName = ObjectName;
				DebugMarkerSetObjectName(VulkanDevice, &Info);
			};
			FVulkanTexture2D* VulkanTexture = (FVulkanTexture2D*)Tex2d;
			DoCall(Device->DebugMarkerSetObjectName, Device->GetInstanceHandle(), VulkanTexture, TCHAR_TO_ANSI(Name));
		}
	}*/
#endif
	FName DebugName(Name);
	TextureRHI->SetName(DebugName);
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
{
#if VULKAN_ENABLE_DRAW_MARKERS
	//if (Device->SupportsDebugMarkers())
	{
		//if (FRHITexture2D* Tex2d = UnorderedAccessViewRHI->GetTexture2D())
		//{
		//	FVulkanTexture2D* VulkanTexture = (FVulkanTexture2D*)Tex2d;
		//	VkDebugMarkerObjectTagInfoEXT Info;
		//	FMemory::Memzero(Info);
		//	Info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		//	Info.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		//	Info.object = VulkanTexture->Surface.Image;
		//	vkDebugMarkerSetObjectNameEXT(Device->GetInstanceHandle(), &Info);
		//}
	}
#endif
}


void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

uint64 FVulkanDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
#if 0
	check(Device);

	VkFormat InternalFormat;
	VkImageCreateInfo CreateInfo;
	VkMemoryRequirements MemReq;

	EPixelFormat PixelFormat = (EPixelFormat)Format;

	// Create temporary image to measure the memory requiremants
	VkImage TmpImage = FVulkanSurface::CreateImage(
		*Device, VK_IMAGE_VIEW_TYPE_2D,
		PixelFormat, SizeX, SizeY, 1,
		false, 0, NumMips, Flags,
		&InternalFormat, &CreateInfo, &MemReq);

	VulkanRHI::vkDestroyImage(Device->GetInstanceHandle(), TmpImage, nullptr);

	OutAlign = MemReq.alignment;

	return MemReq.size;
#else
	OutAlign = 0;
	return 0;
#endif
}

uint64 FVulkanDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return 0;
}

uint64 FVulkanDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return 0;
}

FTextureReferenceRHIRef FVulkanDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return new FVulkanTextureReference(*Device, LastRenderTime);
}

void FVulkanCommandListContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
	//#todo-rco: Implementation needs to be verified
	FVulkanTextureReference* VulkanTextureRef = (FVulkanTextureReference*)TextureRef;
	if (VulkanTextureRef)
	{
		VulkanTextureRef->SetReferencedTexture(NewTexture);
	}
}
