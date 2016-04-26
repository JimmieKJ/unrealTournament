// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanVertexBuffer.cpp: Vulkan texture RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
static FCriticalSection GTextureMapLock;
static TMap<FRHIResource*, VulkanRHI::FStagingImage*> GTextureMapEntries;

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
	auto& DeviceProperties = InDevice.GetDeviceProperties();

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

	ImageCreateInfo.format = (VkFormat)GPixelFormats[InFormat].PlatformFormat;
	if ((UEFlags & TexCreate_SRGB) == TexCreate_SRGB)
	{
		switch (ImageCreateInfo.format)
		{
		case VK_FORMAT_B8G8R8A8_UNORM:			ImageCreateInfo.format = VK_FORMAT_B8G8R8A8_SRGB; break;
		case VK_FORMAT_R8G8B8A8_UNORM:			ImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB; break;
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:		ImageCreateInfo.format = VK_FORMAT_BC1_RGB_SRGB_BLOCK; break;
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:	ImageCreateInfo.format = VK_FORMAT_BC1_RGBA_SRGB_BLOCK; break;
		case VK_FORMAT_BC2_UNORM_BLOCK:			ImageCreateInfo.format = VK_FORMAT_BC2_SRGB_BLOCK; break;
		case VK_FORMAT_BC3_UNORM_BLOCK:			ImageCreateInfo.format = VK_FORMAT_BC3_SRGB_BLOCK; break;
		case VK_FORMAT_BC7_UNORM_BLOCK:			ImageCreateInfo.format = VK_FORMAT_BC7_SRGB_BLOCK; break;
		default:	break;
		}
	}

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

#if VULKAN_STRICT_TEXTURE_FLAGS
	if (ImageCreateInfo.tiling == VK_IMAGE_TILING_LINEAR)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].linearTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
	else if (ImageCreateInfo.tiling == VK_IMAGE_TILING_OPTIMAL)
	{
		VkFormatFeatureFlags FormatFlags = InDevice.GetFormatProperties()[ImageCreateInfo.format].optimalTilingFeatures;
		if ((FormatFlags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if ((FormatFlags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		{
			ImageCreateInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}
#endif

	VkImage Image;
	VERIFYVULKANRESULT(vkCreateImage(InDevice.GetInstanceHandle(), &ImageCreateInfo, nullptr, &Image));

	// Fetch image size
	VkMemoryRequirements tmpMemReq;
	VkMemoryRequirements& MemReq = OutMemoryRequirements ? *OutMemoryRequirements : tmpMemReq;
	vkGetImageMemoryRequirements(InDevice.GetInstanceHandle(), Image, &MemReq);

	return Image;
}


static TArray<VkFormat> GVulkanPixelFormatKeys;

FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 InNumMips,
								uint32 InNumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(VK_NULL_HANDLE)
	, ImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	, InternalFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, Format(InFormat)
	, FormatKey(0)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(true)
#if VULKAN_USE_MEMORY_SYSTEM
	, Allocation(nullptr)
#else
	, DeviceMemory(VK_NULL_HANDLE)
#endif
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, AspectMask(0)
{
	VkImageCreateInfo ImageCreateInfo;	// Zeroed inside CreateImage
	VkMemoryRequirements MemoryRequirements; // Zeroed inside CreateImage
	Image = FVulkanSurface::CreateImage(InDevice, ResourceType,
		InFormat, SizeX, SizeY, SizeZ,
		bArray, ArraySize, NumMips, NumSamples, UEFlags,
		&InternalFormat,
		&ImageCreateInfo, &MemoryRequirements);

	AspectMask = GetAspectMask(InternalFormat);

	// If VK_IMAGE_TILING_OPTIMAL is specified,
	// memoryTypeBits in vkGetImageMemoryRequirements will become 1
	// which does not support VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT.
	if (ImageCreateInfo.tiling != VK_IMAGE_TILING_OPTIMAL)
	{
		MemProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}

#if VULKAN_USE_MEMORY_SYSTEM
	Allocation = InDevice.GetMemoryManager().Alloc(MemoryRequirements.size, MemoryRequirements.memoryTypeBits, MemProps, __FILE__, __LINE__);
	VERIFYVULKANRESULT(vkBindImageMemory(Device->GetInstanceHandle(), Image, Allocation->GetHandle(), 0));
#else
	VkMemoryAllocateInfo AllocInfo;
	FMemory::Memzero(AllocInfo);
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.pNext = nullptr;
	AllocInfo.allocationSize = MemoryRequirements.size;
	AllocInfo.memoryTypeIndex = 0;
	VkResult Result = Device->GetMemoryManager().GetMemoryTypeFromProperties(MemoryRequirements.memoryTypeBits, MemProps, &AllocInfo.memoryTypeIndex);
	if (Result != VK_SUCCESS)
	{
		// We are currently only hitting this case when trying to alloc CPU writable 2d texture arrays
		if ((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && CreateInfo.BulkData)
		{
			// Try again without CPU access, let's assume that since we have BulkData we can fill it in via DMA
			MemProps &= ~VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			Result = Device->GetMemoryManager().GetMemoryTypeFromProperties(MemoryRequirements.memoryTypeBits, MemProps, &AllocInfo.memoryTypeIndex);
		}
	}
	VERIFYVULKANRESULT_EXPANDED(Result);

	// Allocate/Free memory should have a wrapper function, which tracks "GVulkanBufferByteBalance"
	VERIFYVULKANRESULT_EXPANDED(vkAllocateMemory(Device->GetInstanceHandle(), &AllocInfo, nullptr, &DeviceMemory));
	VERIFYVULKANRESULT(vkBindImageMemory(Device->GetInstanceHandle(), Image, DeviceMemory, 0));
#endif


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
		ClearBlocking(CreateInfo.ClearValueBinding);
	}
}

// This is usually used for the framebuffer image
FVulkanSurface::FVulkanSurface(FVulkanDevice& InDevice, VkImageViewType ResourceType, EPixelFormat InFormat,
								uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage,
								uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo)
	: Device(&InDevice)
	, Image(InImage)
	, ImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	, InternalFormat(VK_FORMAT_UNDEFINED)
	, Width(SizeX)
	, Height(SizeY)
	, Depth(SizeZ)
	, Format(InFormat)
	, FormatKey(0)
	, MemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	, Tiling(VK_IMAGE_TILING_MAX_ENUM)	// Can be expanded to a per-platform definition
	, ViewType(ResourceType)
	, bIsImageOwner(false)
#if VULKAN_USE_MEMORY_SYSTEM
	, Allocation(nullptr)
#else
	, DeviceMemory(VK_NULL_HANDLE)
#endif
	, NumMips(0)
	, NumSamples(1)	// This is defined by the MSAA setting...
	, AspectMask(0)
{
	InternalFormat = (VkFormat)GPixelFormats[Format].PlatformFormat;
	AspectMask = GetAspectMask(InternalFormat);

	// Purely informative patching, we know that "TexCreate_Presentable" uses optimal tiling
	if (UEFlags & TexCreate_Presentable && GetTiling() == VK_IMAGE_TILING_MAX_ENUM)
	{
		Tiling = VK_IMAGE_TILING_OPTIMAL;
	}
	
	if (Image != VK_NULL_HANDLE)
	{
		if (UEFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable))
		{
			ClearBlocking(CreateInfo.ClearValueBinding);
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
			vkDestroyImage(Device->GetInstanceHandle(), Image, nullptr);
			Image = VK_NULL_HANDLE;
			ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

#if VULKAN_USE_MEMORY_SYSTEM
		Device->GetMemoryManager().Free(Allocation);
		Allocation = nullptr;
#else
		if (DeviceMemory != VK_NULL_HANDLE)
		{
			vkFreeMemory(Device->GetInstanceHandle(), DeviceMemory, nullptr);
			DeviceMemory = VK_NULL_HANDLE;
		}		
#endif
	}
}

void* FVulkanSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
#if VULKAN_USE_MEMORY_SYSTEM
#else
	checkf(DeviceMemory != VK_NULL_HANDLE, TEXT("Attempting to map unallocated image-memory"));
#endif

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
	vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

	// Set linear row-pitch
	GetMipStride(MipIndex, DestStride);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		// Verify pitch if linear
		check(DestStride == SubResourceLayout.rowPitch);

		// Map buffer to a pointer
#if VULKAN_USE_MEMORY_SYSTEM
		Data = Allocation->Map(SubResourceLayout.size, SubResourceLayout.offset);
#else
		VERIFYVULKANRESULT(vkMapMemory(Device->GetInstanceHandle(), DeviceMemory, SubResourceLayout.offset, SubResourceLayout.size, 0, &Data));
#endif
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
	Flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	LinearBuffer = new FVulkanBuffer(*Device, Bytes, Usage, Flags, false, __FILE__, __LINE__);

	void* DataPtr = LinearBuffer->Lock(Bytes);
	check(DataPtr);

	return DataPtr;
}

void FVulkanSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
#if VULKAN_USE_MEMORY_SYSTEM
#else
	checkf(DeviceMemory != VK_NULL_HANDLE, TEXT("Attempting to unmap unallocated image-memory"));
#endif

	check((MemProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0 ? Tiling == VK_IMAGE_TILING_OPTIMAL : Tiling == VK_IMAGE_TILING_LINEAR);

	if(Tiling == VK_IMAGE_TILING_LINEAR)
	{
		auto& Data = MipMapMapping.FindOrAdd(MipIndex);
		checkf(Data != nullptr, TEXT("The buffer needs to be mapped, before it can be unmapped"));

#if VULKAN_USE_MEMORY_SYSTEM
		Allocation->Unmap();
#else
		vkUnmapMemory(Device->GetInstanceHandle(), DeviceMemory);
#endif
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
	
	vkGetImageSubresourceLayout(Device->GetInstanceHandle(), Image, &ImageSubResource, &SubResourceLayout);

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

uint32 FVulkanSurface::GetMemorySize()
{
	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(Device->GetInstanceHandle(), Image, &mem_req);

	return mem_req.size;
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

VkImageAspectFlags FVulkanSurface::GetAspectMask(VkFormat Format)
{
	VkImageAspectFlags mask = 0;

	// Check for depth and stencil formats
	mask |= Format == VK_FORMAT_D16_UNORM ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	mask |= Format == VK_FORMAT_X8_D24_UNORM_PACK32 ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	mask |= Format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	mask |= Format == VK_FORMAT_S8_UINT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
	mask |= Format == VK_FORMAT_D16_UNORM_S8_UINT ? (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT) : 0;
	mask |= Format == VK_FORMAT_D24_UNORM_S8_UINT ? (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT) : 0;
	mask |= Format == VK_FORMAT_D32_SFLOAT_S8_UINT ? (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT) : 0;

	// If no depth or stencil is present, then its a color format
	mask = mask == 0 ? VK_IMAGE_ASPECT_COLOR_BIT : mask;

	return mask;
}

VkImageAspectFlags FVulkanSurface::GetAspectMask() const
{
	check(AspectMask != 0);
	return AspectMask;
}

// Fixes the issue:
//	"vkCmdBeginRenderPass(): Cannot read invalid swapchain image 1243cdf0, please fill the memory before using."
void FVulkanSurface::ClearBlocking(const FClearValueBinding& ClearValueBinding)
{
#if VULKAN_CLEAR_SURFACE_ON_CREATE
	VkImageSubresourceRange range;
	FMemory::Memzero(range);
	range.aspectMask = GetAspectMask();
	range.baseMipLevel = 0;
	range.levelCount = NumMips;
	range.baseArrayLayer = 0;
	range.layerCount = ViewType == VK_IMAGE_VIEW_TYPE_CUBE ? 6 : 1;

	//#todo-rco: FIX ME!
	FVulkanCmdBuffer* cmd = Device->GetImmediateContext().GetCommandBufferManager()->Create();
	cmd->Begin();
	
	if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
	{
		VkClearColorValue color;
		FMemory::Memzero(color);
		color.float32[0] = ClearValueBinding.Value.Color[0];
		color.float32[1] = ClearValueBinding.Value.Color[1];
		color.float32[2] = ClearValueBinding.Value.Color[2];
		color.float32[3] = ClearValueBinding.Value.Color[3];
		VulkanSetImageLayout(cmd->GetHandle(), Image, range.aspectMask, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vkCmdClearColorImage(cmd->GetHandle(), Image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
	}
	else
	{
		check(range.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
		VkClearDepthStencilValue value;
		FMemory::Memzero(value);
		value.depth = ClearValueBinding.Value.DSValue.Depth;
		value.stencil = ClearValueBinding.Value.DSValue.Stencil;
		VulkanSetImageLayout(cmd->GetHandle(), Image, range.aspectMask, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		vkCmdClearDepthStencilImage(cmd->GetHandle(), Image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &value, 1, &range);
	}

	Device->EndCommandBufferBlock(cmd);
#endif
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
	VULKAN_SIGNAL_UNIMPLEMENTED();
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

	EPixelFormat Format = Texture->Surface.Format;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);
#if VULKAN_USE_NEW_STAGING_BUFFERS
	VulkanRHI::FStagingImage** StagingImage = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingImage = &GTextureMapEntries.FindOrAdd(TextureRHI);
		checkf(!*StagingImage, TEXT("Can't lock the same texture twice!"));
	}

	*StagingImage = Device->GetStagingManager().AcquireImage(Texture->Surface.InternalFormat, MipWidth, MipHeight);

	VkImageSubresource Subresource;
	FMemory::Memzero(Subresource);
	Subresource.aspectMask = Texture->Surface.GetAspectMask();
	Subresource.mipLevel = 0;
	Subresource.arrayLayer = 0;
	VkSubresourceLayout SubresourceLayout;
	vkGetImageSubresourceLayout(Device->GetInstanceHandle(), (*StagingImage)->GetHandle(), &Subresource, &SubresourceLayout);
	DestStride = SubresourceLayout.rowPitch;

	return (*StagingImage)->Map();
#else
	FVulkanDevice* VulkanDevice = Texture->Surface.Device;
	VkDevice LogicalDevice = VulkanDevice->GetInstanceHandle();

	VkExtent3D StagingExtent;
	FMemory::Memzero(StagingExtent);
	StagingExtent.width = MipWidth;
	StagingExtent.height = MipHeight;
	StagingExtent.depth = 1;

	VkImageCreateInfo StagingImageCreateInfo;
	FMemory::Memzero(StagingImageCreateInfo);
	StagingImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	StagingImageCreateInfo.pNext = nullptr;
	StagingImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	StagingImageCreateInfo.format = Texture->Surface.InternalFormat;
	StagingImageCreateInfo.extent = StagingExtent;
	StagingImageCreateInfo.mipLevels = 1;
	StagingImageCreateInfo.arrayLayers = 1;
	StagingImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	StagingImageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
	StagingImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VERIFYVULKANRESULT(vkCreateImage(LogicalDevice, &StagingImageCreateInfo, nullptr, &Texture->StagingImage));

	VkMemoryRequirements MemReqs;
	vkGetImageMemoryRequirements(LogicalDevice, Texture->StagingImage, &MemReqs);

	VkMemoryAllocateInfo AllocInfo;
	FMemory::Memzero(AllocInfo);
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemReqs.size;

	VkMemoryPropertyFlags MemoryTypeFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	VERIFYVULKANRESULT(VulkanDevice->GetMemoryManager().GetMemoryTypeFromProperties(MemReqs.memoryTypeBits, MemoryTypeFlags, &AllocInfo.memoryTypeIndex));
	#if VULKAN_USE_MEMORY_SYSTEM
		check(0);
	#else
		VERIFYVULKANRESULT(vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &Texture->PixelBufferMemory));
	#endif
	VERIFYVULKANRESULT(vkBindImageMemory(LogicalDevice, Texture->StagingImage, Texture->PixelBufferMemory, 0));

	VkImageSubresource Subresource;
	FMemory::Memzero(Subresource);
	Subresource.aspectMask = Texture->Surface.GetAspectMask();
	Subresource.mipLevel = 0;
	Subresource.arrayLayer = 0;

	VkSubresourceLayout SubresourceLayout;
	vkGetImageSubresourceLayout(LogicalDevice, Texture->StagingImage, &Subresource, &SubresourceLayout);
	DestStride = SubresourceLayout.rowPitch;

	void* MappedData = nullptr;
	VERIFYVULKANRESULT(vkMapMemory(LogicalDevice, Texture->PixelBufferMemory, 0, AllocInfo.allocationSize, 0, &MappedData));

	return MappedData;
#endif
}

void FVulkanDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

#if VULKAN_USE_FENCE_MANAGER
	auto* Fence = Device->GetFenceManager().AllocateFence();
#else
	VkFence Fence;
	VkFenceCreateInfo FenceCreateInfo;
	FMemory::Memzero(FenceCreateInfo);
	FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VERIFYVULKANRESULT(vkCreateFence(LogicalDevice, &FenceCreateInfo, nullptr, &Fence));
#endif

#if VULKAN_USE_NEW_STAGING_BUFFERS
	VulkanRHI::FStagingImage* StagingImage = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GTextureMapEntries.RemoveAndCopyValue(TextureRHI, StagingImage);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	StagingImage->Unmap();

	//#todo-rco: TEMP!
	Texture->StagingImage = StagingImage->GetHandle();

	//#todo-rco: Put into immediate cmd list
	{

	}
#else
	//@TODO - share this with other texture unlocks
	vkUnmapMemory(LogicalDevice, Texture->PixelBufferMemory);

	// @TODO: set image layout?
#endif

	VkCommandBuffer StagingCommandBuffer;
	VkCommandBufferAllocateInfo BufferCreateInfo;
	FMemory::Memzero(BufferCreateInfo);
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	BufferCreateInfo.commandPool = Device->GetImmediateContext().GetCommandBufferManager()->GetHandle();
	BufferCreateInfo.commandBufferCount = 1;
	BufferCreateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VERIFYVULKANRESULT(vkAllocateCommandBuffers(LogicalDevice, &BufferCreateInfo, &StagingCommandBuffer));

	VkCommandBufferBeginInfo BufferBeginInfo;
	FMemory::Memzero(BufferBeginInfo);
	BufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VERIFYVULKANRESULT(vkBeginCommandBuffer(StagingCommandBuffer, &BufferBeginInfo));

	EPixelFormat Format = Texture->Surface.Format;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageCopy Region;
	FMemory::Memzero(Region);

	Region.srcSubresource.aspectMask = Texture->Surface.GetAspectMask();
	Region.srcSubresource.mipLevel = 0;
	Region.srcSubresource.baseArrayLayer = 0;
	Region.srcSubresource.layerCount = 1;
	Region.srcOffset.x = 0;
	Region.srcOffset.y = 0;
	Region.srcOffset.z = 0;

	// dst image is expected to have the same dimensions/setup
	Region.dstSubresource = Region.srcSubresource;
	Region.dstSubresource.mipLevel = MipIndex;
	
	Region.extent.width = MipWidth;
	Region.extent.height = MipHeight;
	Region.extent.depth = 1;

	vkCmdCopyImage(StagingCommandBuffer, Texture->StagingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VERIFYVULKANRESULT(vkEndCommandBuffer(StagingCommandBuffer));

	VkSubmitInfo SubmitInfo;
	FMemory::Memzero(SubmitInfo);
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &StagingCommandBuffer;

#if VULKAN_USE_FENCE_MANAGER
	VkResult Result = vkQueueSubmit(Device->GetQueue()->GetHandle(), 1, &SubmitInfo, Fence->GetHandle());
#else
	VkResult Result = vkQueueSubmit(Device->GetQueue()->GetHandle(), 1, &SubmitInfo, Fence);
#endif
	checkf(Result == VK_SUCCESS, TEXT("vkQueueSubmit failed with error %d, on texture with UE format %d, size %dx%d, mip %d"), (int32)Result, (int32)Format, MipWidth, MipHeight, MipIndex);

#if VULKAN_USE_FENCE_MANAGER
	bool bSuccess = Device->GetFenceManager().WaitForFence(Fence, 0xFFFFFFFF);
	check(bSuccess);

	Device->GetFenceManager().ReleaseFence(Fence);
#else
	VERIFYVULKANRESULT(vkWaitForFences(LogicalDevice, 1, &Fence, true, 0xFFFFFFFF));
	VERIFYVULKANRESULT(vkResetFences(LogicalDevice, 1, &Fence));
	vkDestroyFence(LogicalDevice, Fence, nullptr);
#endif

	VERIFYVULKANRESULT(vkResetCommandBuffer(StagingCommandBuffer, 0x0));

	//#todo-rco: FIX ME!
	vkFreeCommandBuffers(LogicalDevice, Device->GetImmediateContext().GetCommandBufferManager()->GetHandle(), 1, &StagingCommandBuffer);
#if VULKAN_USE_NEW_STAGING_BUFFERS
	Device->GetStagingManager().ReleaseImage(StagingImage);
#else
	vkDestroyImage(LogicalDevice, Texture->StagingImage, nullptr);
	vkFreeMemory(LogicalDevice, Texture->PixelBufferMemory, nullptr);
#endif
}

void* FVulkanDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, TextureIndex, LockMode, DestStride);
}

void FVulkanDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTexture2DArray* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, TextureIndex);
}

void FVulkanDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
#if 0
	FVulkanTexture2D* Texture = ResourceCast(TextureRHI);
#endif

}

void FVulkanDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{	
	VULKAN_SIGNAL_UNIMPLEMENTED();
#if 0
	FVulkanTexture3D* Texture = ResourceCast(TextureRHI);
#endif
}

void FVulkanTextureView::Create(FVulkanDevice& Device, FVulkanSurface& Surface, VkImageViewType ViewType, VkFormat Format, uint32 NumMips)
{
	VkImageViewCreateInfo ViewInfo;
	FMemory::Memzero(ViewInfo);
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.pNext = nullptr;
	ViewInfo.image = Surface.Image;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = Format;
	ViewInfo.components = Device.GetFormatComponentMapping(Surface.Format);

	uint32 ArraySize = 1;	//@TODO: Fix for arrays
	uint32 FaceCount = (ViewType == VK_IMAGE_VIEW_TYPE_CUBE) ? 6 : 1;
	ViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, NumMips, 0, ArraySize*FaceCount };
	ViewInfo.subresourceRange.aspectMask = Surface.GetAspectMask();

	VERIFYVULKANRESULT(vkCreateImageView(Device.GetInstanceHandle(), &ViewInfo, nullptr, &View));
}

void FVulkanTextureView::Destroy(FVulkanDevice& Device)
{
	if (View)
	{
		vkDestroyImageView(Device.GetInstanceHandle(), View, nullptr);
		View = VK_NULL_HANDLE;
	}
}

#if VULKAN_HAS_DEBUGGING_ENABLED
	static int32 GVulkanTextureBaseAllocationCount = 0;
	static int32 GVulkanAllocationBalance = 0;
#endif

#if VULKAN_HAS_DEBUGGING_ENABLED
int32 FVulkanTextureBase::GetAllocatedCount()
{
	return GVulkanTextureBaseAllocationCount;
}
#endif

#if VULKAN_HAS_DEBUGGING_ENABLED
int32 FVulkanTextureBase::GetAllocationBalance()
{
	return GVulkanAllocationBalance;
}
#endif

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

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat InFormat, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo) :
	#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	 Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, NumSamples, UEFlags, CreateInfo)
	#else
	 Surface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, bArray, ArraySize, NumMips, (UEFlags & TexCreate_DepthStencilTargetable) ? NumSamples : 1, UEFlags, CreateInfo)
	 , MSAASurface(nullptr)
	#endif
	, StagingImage(VK_NULL_HANDLE)
	, PixelBufferMemory(VK_NULL_HANDLE)
	#if VULKAN_HAS_DEBUGGING_ENABLED
	, AllocationNumber(0)
	#endif
{
	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		View.Create(Device, Surface, ResourceType, Surface.InternalFormat, FMath::Max(NumMips, 1u));
	}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Create MSAA surface. The surface above is the resolve target
	if (NumSamples > 1 && (UEFlags & TexCreate_RenderTargetable) && !(UEFlags & TexCreate_DepthStencilTargetable))
	{
		MSAASurface = new FVulkanSurface(Device, ResourceType, InFormat, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, NumSamples, UEFlags, CreateInfo);
		if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
		{
			MSAAView.Create(Device, *MSAASurface, ResourceType, MSAASurface->InternalFormat, FMath::Max(NumMips, 1u));
		}
	}
#endif

	#if VULKAN_HAS_DEBUGGING_ENABLED
		GVulkanAllocationBalance++;
		AllocationNumber = GVulkanTextureBaseAllocationCount++;
	#endif

	// For now only 3D textures can have bulk-data parsed on creation time.
	if(!CreateInfo.BulkData || Surface.GetTiling() != VK_IMAGE_TILING_OPTIMAL)
	{
		return;
	}

	// Transfer bulk data

	VkBufferImageCopy Region;
	FMemory::Memzero(Region);
	Region.bufferOffset = 0;
	Region.bufferRowLength = Surface.Width;
	Region.bufferImageHeight = Surface.Height;
	
	Region.imageSubresource.mipLevel = 0;
	Region.imageSubresource.baseArrayLayer = 0;
	Region.imageSubresource.layerCount = ArraySize;
	Region.imageSubresource.aspectMask = Surface.GetAspectMask();

	Region.imageExtent.width = Region.bufferRowLength;
	Region.imageExtent.height = Region.bufferImageHeight;
	Region.imageExtent.depth = Surface.Depth;

	// Temporary buffer which will hold bulkdata
	VkBufferCreateInfo BufferCreateInfo;
	FMemory::Memzero(BufferCreateInfo);
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.flags = 0;
	BufferCreateInfo.pQueueFamilyIndices = NULL;
	BufferCreateInfo.queueFamilyIndexCount = 0;
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	BufferCreateInfo.size = CreateInfo.BulkData->GetResourceBulkDataSize();
	BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VkBuffer Buffer;
	VERIFYVULKANRESULT(vkCreateBuffer(Device.GetInstanceHandle(), &BufferCreateInfo, NULL, &Buffer));

	VkMemoryRequirements MemReqs;
	vkGetBufferMemoryRequirements(Device.GetInstanceHandle(), Buffer, &MemReqs);

	// Allocate device gmem
	VkDeviceMemory Mem;
#if 0//VULKAN_USE_NEW_STAGING_BUFFERS
	//#todo-rco: USe StagingBuffers
#else
	#if VULKAN_USE_MEMORY_SYSTEM
		VulkanRHI::FDeviceMemoryAllocation* Allocation = Device.GetMemoryManager().Alloc(MemReqs.size, MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, __FILE__, __LINE__);
		Mem = Allocation->GetHandle();
	#else
		VkMemoryAllocateInfo AllocInfo;
		FMemory::Memzero(AllocInfo);
		AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		AllocInfo.pNext = NULL;
		AllocInfo.allocationSize = MemReqs.size;
		AllocInfo.memoryTypeIndex = 0;

		VERIFYVULKANRESULT(Device.GetMemoryManager().GetMemoryTypeFromProperties(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &AllocInfo.memoryTypeIndex));
		VERIFYVULKANRESULT(vkAllocateMemory(Device.GetInstanceHandle(), &AllocInfo, NULL, &Mem));
	#endif

	VERIFYVULKANRESULT(vkBindBufferMemory(Device.GetInstanceHandle(), Buffer, Mem, 0));

	void* Data = nullptr;
	VERIFYVULKANRESULT(vkMapMemory(Device.GetInstanceHandle(), Mem, 0, MemReqs.size, 0, &Data));
#endif
	// Do copy
	FMemory::Memcpy(Data, CreateInfo.BulkData->GetResourceBulkData(), CreateInfo.BulkData->GetResourceBulkDataSize());

	vkUnmapMemory(Device.GetInstanceHandle(), Mem);

	//#todo-rco: FIX ME?
	FVulkanCmdBuffer* Cmd = Device.GetImmediateContext().GetCommandBufferManager()->Create();
	Cmd->Begin();

	// Copy buffer to image
	vkCmdCopyBufferToImage(Cmd->GetHandle(), Buffer, Surface.Image, Surface.ImageLayout, 1, &Region);

	Device.GetQueue()->SubmitBlocking(Cmd);
	//#todo-rco: FIX ME!
	Device.GetImmediateContext().GetCommandBufferManager()->Destroy(Cmd);

	vkDestroyBuffer(Device.GetInstanceHandle(), Buffer, nullptr);

	#if VULKAN_USE_MEMORY_SYSTEM
		Device.GetMemoryManager().Free(Allocation);
	#else
		vkFreeMemory(Device.GetInstanceHandle(), Mem, nullptr);
	#endif
}

FVulkanTextureBase::FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo):
	 Surface(Device, ResourceType, Format, SizeX, SizeY, SizeZ, InImage, UEFlags, CreateInfo)
	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	, MSAASurface(nullptr)
	#endif
	, StagingImage(VK_NULL_HANDLE)
	, PixelBufferMemory(VK_NULL_HANDLE)
	#if VULKAN_HAS_DEBUGGING_ENABLED
	, AllocationNumber(0)
	#endif
{
	check(InMem == VK_NULL_HANDLE);
	if (ResourceType != VK_IMAGE_VIEW_TYPE_MAX_ENUM)
	{
		View.Create(Device, Surface, ResourceType, Surface.InternalFormat, 1);
	}

	#if VULKAN_HAS_DEBUGGING_ENABLED
		GVulkanAllocationBalance++;
		AllocationNumber = GVulkanTextureBaseAllocationCount++;
	#endif
}

FVulkanTextureBase::~FVulkanTextureBase()
{
	View.Destroy(*Surface.Device);

	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (MSAASurface)
	{
		delete MSAASurface;
		MSAASurface = nullptr;
	}
	#endif

	#if VULKAN_HAS_DEBUGGING_ENABLED
		GVulkanAllocationBalance--;
		check(GVulkanAllocationBalance >= 0);
	#endif
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
	Destroy(*Surface.Device);
}

void FVulkanTextureReference::SetReferencedTexture(FRHITexture* InTexture)
{
	FRHITextureReference::SetReferencedTexture(InTexture);
}

void FVulkanTexture2D::Destroy(FVulkanDevice& Device)
{
	View.Destroy(Device);
	Surface.Destroy();
}

FVulkanTextureCube::FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
:	 FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
	//#todo-rco: Array/slices count
,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_CUBE, Format, Size, Size, 1, bArray, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
{
}

FVulkanTextureCube::~FVulkanTextureCube()
{
	Destroy(*Surface.Device);
}

void FVulkanTextureCube::Destroy(FVulkanDevice& Device)
{
	View.Destroy(Device);
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
	
	EPixelFormat Format = Texture->Surface.Format;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

#if VULKAN_USE_NEW_STAGING_BUFFERS
		
	VulkanRHI::FStagingImage** StagingImage = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		StagingImage = &GTextureMapEntries.FindOrAdd(TextureCubeRHI);
		checkf(!*StagingImage, TEXT("Can't lock the same texture twice!"));
	}

	*StagingImage = Device->GetStagingManager().AcquireImage(Texture->Surface.InternalFormat, MipWidth, MipHeight);

	VkImageSubresource Subresource;
	FMemory::Memzero(Subresource);
	Subresource.aspectMask = Texture->Surface.GetAspectMask();
	Subresource.mipLevel = 0;
	Subresource.arrayLayer = 0;
	VkSubresourceLayout SubresourceLayout;
	vkGetImageSubresourceLayout(Device->GetInstanceHandle(), (*StagingImage)->GetHandle(), &Subresource, &SubresourceLayout);
	DestStride = SubresourceLayout.rowPitch;

	return (*StagingImage)->Map();
#else
	//@TODO - share this with other texture locks

	FVulkanDevice* VulkanDevice = Texture->Surface.Device;
	VkDevice LogicalDevice = VulkanDevice->GetInstanceHandle();


	VkExtent3D StagingExtent;
	FMemory::Memzero(StagingExtent);
	StagingExtent.width = MipWidth;
	StagingExtent.height = MipHeight;
	StagingExtent.depth = 1;

	VkImageCreateInfo StagingImageCreateInfo;
	FMemory::Memzero(StagingImageCreateInfo);
	StagingImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	StagingImageCreateInfo.pNext = nullptr;
	StagingImageCreateInfo.imageType = VK_IMAGE_TYPE_2D; // @TODO: check cube is 2D
	StagingImageCreateInfo.format = Texture->Surface.InternalFormat;
	StagingImageCreateInfo.extent = StagingExtent;
	StagingImageCreateInfo.mipLevels = 1;
	StagingImageCreateInfo.arrayLayers = 1;
	StagingImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	StagingImageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
	StagingImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	
	VERIFYVULKANRESULT(vkCreateImage(LogicalDevice, &StagingImageCreateInfo, nullptr, &Texture->StagingImage));

	VkMemoryRequirements MemReqs;
	vkGetImageMemoryRequirements(LogicalDevice, Texture->StagingImage, &MemReqs);

	VkMemoryAllocateInfo AllocInfo;
	FMemory::Memzero(AllocInfo);
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.allocationSize = MemReqs.size;

	VkMemoryPropertyFlags MemoryTypeFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	VERIFYVULKANRESULT(VulkanDevice->GetMemoryManager().GetMemoryTypeFromProperties(MemReqs.memoryTypeBits, MemoryTypeFlags, &AllocInfo.memoryTypeIndex));
	#if VULKAN_USE_MEMORY_SYSTEM
		check(0);
	#else
		VERIFYVULKANRESULT(vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &Texture->PixelBufferMemory));
	#endif

	VERIFYVULKANRESULT(vkBindImageMemory(LogicalDevice, Texture->StagingImage, Texture->PixelBufferMemory, 0));

	VkImageSubresource Subresource;
	FMemory::Memzero(Subresource);
	Subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Subresource.mipLevel = 0;
	Subresource.arrayLayer = 0;

	VkSubresourceLayout SubresourceLayout;
	vkGetImageSubresourceLayout(LogicalDevice, Texture->StagingImage, &Subresource, &SubresourceLayout);
	DestStride = SubresourceLayout.rowPitch;

	void* MappedData = nullptr;
	VERIFYVULKANRESULT(vkMapMemory(LogicalDevice, Texture->PixelBufferMemory, 0, AllocInfo.allocationSize, 0, &MappedData));

	return MappedData;
	#endif
}

void FVulkanDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FVulkanTextureCube* Texture = ResourceCast(TextureCubeRHI);
	check(Texture);

	VkDevice LogicalDevice = Device->GetInstanceHandle();

#if VULKAN_USE_FENCE_MANAGER
	auto* Fence = Device->GetFenceManager().AllocateFence();
#else
	VkFence Fence;
	VkFenceCreateInfo FenceCreateInfo;
	FMemory::Memzero(FenceCreateInfo);
	FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VERIFYVULKANRESULT(vkCreateFence(LogicalDevice, &FenceCreateInfo, nullptr, &Fence));
#endif

#if VULKAN_USE_NEW_STAGING_BUFFERS
	VulkanRHI::FStagingImage* StagingImage = nullptr;
	{
		FScopeLock Lock(&GTextureMapLock);
		bool bFound = GTextureMapEntries.RemoveAndCopyValue(TextureCubeRHI, StagingImage);
		checkf(bFound, TEXT("Texture was not locked!"));
	}

	StagingImage->Unmap();

	//#todo-rco: TEMP!
	Texture->StagingImage = StagingImage->GetHandle();

	//#todo-rco: Put into immediate cmd list
	{
	}

#else
	//@TODO - share this with other texture unlocks
	vkUnmapMemory(LogicalDevice, Texture->PixelBufferMemory);

	// @TODO: set image layout?
#endif

	VkCommandBuffer StagingCommandBuffer;
	VkCommandBufferAllocateInfo BufferCreateInfo;
	FMemory::Memzero(BufferCreateInfo);
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	BufferCreateInfo.commandPool = Device->GetImmediateContext().GetCommandBufferManager()->GetHandle();
	BufferCreateInfo.commandBufferCount = 1;
	BufferCreateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VERIFYVULKANRESULT(vkAllocateCommandBuffers(LogicalDevice, &BufferCreateInfo, &StagingCommandBuffer));

	VkCommandBufferBeginInfo BufferBeginInfo;
	FMemory::Memzero(BufferBeginInfo);
	BufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VERIFYVULKANRESULT(vkBeginCommandBuffer(StagingCommandBuffer, &BufferBeginInfo));

	EPixelFormat Format = Texture->Surface.Format;
	uint32 MipWidth = FMath::Max<uint32>(Texture->Surface.Width >> MipIndex, GPixelFormats[Format].BlockSizeX);
	uint32 MipHeight = FMath::Max<uint32>(Texture->Surface.Height >> MipIndex, GPixelFormats[Format].BlockSizeY);

	VkImageCopy Region;
	FMemory::Memzero(Region);
	Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.srcSubresource.mipLevel = 0;
	Region.srcSubresource.baseArrayLayer = 0;
	Region.srcSubresource.layerCount = 1;
	Region.srcOffset.x = 0;
	Region.srcOffset.y = 0;
	Region.srcOffset.z = 0;
	Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.dstSubresource.mipLevel = MipIndex;
	Region.dstSubresource.baseArrayLayer = FaceIndex;
	Region.dstSubresource.layerCount = 1;
	Region.dstOffset.x = 0;
	Region.dstOffset.y = 0;
	Region.dstOffset.z = 0;
	Region.extent.width = MipWidth;
	Region.extent.height = MipHeight;
	Region.extent.depth = 1;

	vkCmdCopyImage(StagingCommandBuffer, Texture->StagingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Texture->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

	VERIFYVULKANRESULT(vkEndCommandBuffer(StagingCommandBuffer));

	VkSubmitInfo SubmitInfo;
	FMemory::Memzero(SubmitInfo);
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &StagingCommandBuffer;

#if VULKAN_USE_FENCE_MANAGER
	VERIFYVULKANRESULT(vkQueueSubmit(Texture->Surface.Device->GetQueue()->GetHandle(), 1, &SubmitInfo, Fence->GetHandle()));

	bool bSuccess = Device->GetFenceManager().WaitForFence(Fence, 0xFFFFFFFF);
	check(bSuccess);

	Device->GetFenceManager().ReleaseFence(Fence);
#else
	VERIFYVULKANRESULT(vkQueueSubmit(Texture->Surface.Device->GetQueue()->GetHandle(), 1, &SubmitInfo, Fence));
	
	VERIFYVULKANRESULT(vkWaitForFences(LogicalDevice, 1, &Fence, true, 0xFFFFFFFF));
	VERIFYVULKANRESULT(vkResetFences(LogicalDevice, 1, &Fence));
	vkDestroyFence(LogicalDevice, Fence, nullptr);
#endif

	VERIFYVULKANRESULT(vkResetCommandBuffer(StagingCommandBuffer, 0x0));

	//#todo-rco: FIX ME!
	vkFreeCommandBuffers(LogicalDevice, Device->GetImmediateContext().GetCommandBufferManager()->GetHandle(), 1, &StagingCommandBuffer);
#if VULKAN_USE_NEW_STAGING_BUFFERS
	Device->GetStagingManager().ReleaseImage(StagingImage);
#else
	vkDestroyImage(LogicalDevice, Texture->StagingImage, nullptr);
	vkFreeMemory(LogicalDevice, Texture->PixelBufferMemory, nullptr);
#endif
}

void FVulkanDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
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

	vkDestroyImage(Device->GetInstanceHandle(), TmpImage, nullptr);

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
