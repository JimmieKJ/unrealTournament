// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanDevice.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanDevice.h"
#include "VulkanPendingState.h"
#include "VulkanManager.h"
#include "VulkanContext.h"

FVulkanDevice::FVulkanDevice(VkPhysicalDevice InGpu)
	: Gpu(InGpu)
	, Device(VK_NULL_HANDLE)
	, DescriptorPool(nullptr)
	, DefaultSampler(VK_NULL_HANDLE)
	, Queue(nullptr)
	, PendingState(nullptr)
	, VBIBRingBuffer(nullptr)
	, ImmediateContext(nullptr)
	, UBRingBuffer(nullptr)
#if VULKAN_ENABLE_DRAW_MARKERS
	, VkCmdDbgMarkerBegin(nullptr)
	, VkCmdDbgMarkerEnd(nullptr)
#endif
	, FrameCounter(0)
#if VULKAN_ENABLE_PIPELINE_CACHE
	, PipelineStateCache(nullptr)
#endif
{
	FMemory::Memzero(GpuProps);
	FMemory::Memzero(Features);
	FMemory::Memzero(FormatProperties);
	FMemory::Memzero(TimestampQueryPool);
	FMemory::Memzero(PixelFormatComponentMapping);
}

FVulkanDevice::~FVulkanDevice()
{
	if (Device != VK_NULL_HANDLE)
	{
		Destroy();
		Device = VK_NULL_HANDLE;
	}
}

void FVulkanDevice::InitDevice()
{
	VkDeviceCreateInfo DeviceInfo;
	FMemory::Memzero(DeviceInfo);
	DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	TArray<const ANSICHAR*> DeviceExtensions;
	TArray<const ANSICHAR*> ValidationLayers;
	GetDeviceExtensions(DeviceExtensions, ValidationLayers);

	DeviceInfo.enabledExtensionCount = DeviceExtensions.Num();
	DeviceInfo.ppEnabledExtensionNames = DeviceExtensions.GetData();

	DeviceInfo.enabledLayerCount = ValidationLayers.Num();
	DeviceInfo.ppEnabledLayerNames = (DeviceInfo.enabledLayerCount > 0) ? ValidationLayers.GetData() : nullptr;

	TArray<VkDeviceQueueCreateInfo> QueueInfos;
	QueueInfos.AddZeroed(QueueProps.Num());
	int32 GfxQueueIndex = -1;
	UE_LOG(LogVulkanRHI, Display, TEXT("Found %d Queues"), QueueProps.Num());
	TArray<float> QueuePriorities;
	for (int32 Index = 0; Index < QueueProps.Num(); ++Index)
	{
		const VkQueueFamilyProperties& CurrProps = QueueProps[Index];
		VkDeviceQueueCreateInfo& CurrQueue = QueueInfos[Index];
		CurrQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		CurrQueue.pNext = NULL;
		CurrQueue.queueFamilyIndex = Index;
		CurrQueue.queueCount = CurrProps.queueCount;
		int32 StartIndex = QueuePriorities.Num();
		QueuePriorities.AddUninitialized(CurrProps.queueCount);
		for (int32 QueueIndex = 0; QueueIndex < (int32)CurrProps.queueCount; ++QueueIndex)
		{
			QueuePriorities[QueueIndex + StartIndex] = 1.0f;
		}
		CurrQueue.pQueuePriorities = QueuePriorities.GetData() + StartIndex;

		if ((CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
		{
			if (GfxQueueIndex == -1)
			{
				GfxQueueIndex = Index;
			}
			else
			{
				//#todo-rco: Support for multi-gpu/choose the best gpu!
			}
		}

		auto GetQueueInfoString = [&]()
			{
				FString Info;
				if ((CurrProps.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
				{
					Info += TEXT(" Gfx");
				}
				if ((CurrProps.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
				{
					Info += TEXT(" Compute");
				}
				if ((CurrProps.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
				{
					Info += TEXT(" Xfer");
				}
				if ((CurrProps.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) == VK_QUEUE_SPARSE_BINDING_BIT)
				{
					Info += TEXT(" Sparse");
				}

				return Info;
			};
		UE_LOG(LogVulkanRHI, Display, TEXT("Queue %d: %s"), Index, *GetQueueInfoString());
	}

	DeviceInfo.queueCreateInfoCount = QueueProps.Num();
	DeviceInfo.pQueueCreateInfos = QueueInfos.GetData();

	VERIFYVULKANRESULT(vkCreateDevice(Gpu, &DeviceInfo, nullptr, &Device));

	MemoryManager.SetDevice(this->Device);

	// Map formats
	{
		for (uint32 Index = 0; Index < VK_FORMAT_RANGE_SIZE; ++Index)
		{
			const VkFormat Format = (VkFormat)Index;
			FMemory::Memzero(FormatProperties[Index]);
			vkGetPhysicalDeviceFormatProperties(Gpu, Format, &FormatProperties[Index]);
		}

		static_assert(sizeof(VkFormat) <= sizeof(GPixelFormats[0].PlatformFormat), "PlatformFormat must be increased!");

		// Initialize the platform pixel format map.
		for (int32 Index = 0; Index < PF_MAX; ++Index)
		{
			GPixelFormats[Index].PlatformFormat = VK_FORMAT_UNDEFINED;
			GPixelFormats[Index].Supported = false;

			// Set default component mapping
			VkComponentMapping& ComponentMapping = PixelFormatComponentMapping[Index];
			ComponentMapping.r = VK_COMPONENT_SWIZZLE_R;
			ComponentMapping.g = VK_COMPONENT_SWIZZLE_G;
			ComponentMapping.b = VK_COMPONENT_SWIZZLE_B;
			ComponentMapping.a = VK_COMPONENT_SWIZZLE_A;
		}

		// Default formats
		MapFormatSupport(PF_B8G8R8A8,		VK_FORMAT_B8G8R8A8_UNORM);
		SetComponentMapping(PF_B8G8R8A8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_G8,				VK_FORMAT_R8_UNORM);
		SetComponentMapping(PF_G8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_FloatRGB, VK_FORMAT_B10G11R11_UFLOAT_PACK32);
		SetComponentMapping(PF_FloatRGB, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_FloatRGBA,		VK_FORMAT_R16G16B16A16_SFLOAT,	8);
		SetComponentMapping(PF_FloatRGBA, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_DepthStencil,	VK_FORMAT_D24_UNORM_S8_UINT);
		if (!GPixelFormats[PF_DepthStencil].Supported)
		{
			MapFormatSupport(PF_DepthStencil, VK_FORMAT_D32_SFLOAT_S8_UINT);
		}
		SetComponentMapping(PF_DepthStencil, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_ShadowDepth,	VK_FORMAT_D16_UNORM);
		SetComponentMapping(PF_ShadowDepth, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		// Requirement for GPU particles
		MapFormatSupport(PF_G32R32F,		VK_FORMAT_R32G32_SFLOAT, 8);
		SetComponentMapping(PF_G32R32F, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_A32B32G32R32F,	VK_FORMAT_R32G32B32A32_SFLOAT, 16);
		SetComponentMapping(PF_A32B32G32R32F, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_G16R16F,		VK_FORMAT_R16G16_SFLOAT);
		SetComponentMapping(PF_G16R16F, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_R32_UINT,		VK_FORMAT_R32_UINT);
		SetComponentMapping(PF_R32_UINT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);
		
		#if PLATFORM_ANDROID
			MapFormatSupport(PF_D24,			VK_FORMAT_X8_D24_UNORM_PACK32);			
		#elif PLATFORM_DESKTOP
			MapFormatSupport(PF_D24,			VK_FORMAT_D24_UNORM_S8_UINT);	// VK_FORMAT_D24_UNORM_X8 seems to be unsupported
			if (!GPixelFormats[PF_D24].Supported)
			{
				MapFormatSupport(PF_D24, VK_FORMAT_D32_SFLOAT_S8_UINT);
				if (!GPixelFormats[PF_D24].Supported)
				{
					MapFormatSupport(PF_D24, VK_FORMAT_D32_SFLOAT);
					if (!GPixelFormats[PF_D24].Supported)
					{
						MapFormatSupport(PF_D24, VK_FORMAT_D16_UNORM_S8_UINT);
						if (!GPixelFormats[PF_D24].Supported)
						{
							MapFormatSupport(PF_D24, VK_FORMAT_D16_UNORM);
						}
					}
				}
			}
		#else
			#error Unsupported Vulkan platform
		#endif
		SetComponentMapping(PF_D24, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);
		
		MapFormatSupport(PF_R16F,			VK_FORMAT_R16_SFLOAT);
		SetComponentMapping(PF_R16F, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_FloatR11G11B10,	VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4);
		SetComponentMapping(PF_FloatR11G11B10, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_A2B10G10R10,	VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4);
		SetComponentMapping(PF_A2B10G10R10, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_A8,				VK_FORMAT_R8_UNORM);
		SetComponentMapping(PF_A8, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R);

		MapFormatSupport(PF_R8G8B8A8,		VK_FORMAT_R8G8B8A8_UNORM);
		SetComponentMapping(PF_R8G8B8A8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

		MapFormatSupport(PF_R8G8,			VK_FORMAT_R8G8_UNORM);
		SetComponentMapping(PF_R8G8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		MapFormatSupport(PF_R32_FLOAT,		VK_FORMAT_R32_SFLOAT);
		SetComponentMapping(PF_R32_FLOAT, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO);

		#if PLATFORM_DESKTOP
			MapFormatSupport(PF_DXT1,			VK_FORMAT_BC1_RGB_UNORM_BLOCK);	// Also what OpenGL expects (RGBA instead RGB, but not SRGB)
			SetComponentMapping(PF_DXT1, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE);

			MapFormatSupport(PF_DXT3,			VK_FORMAT_BC2_UNORM_BLOCK);
			SetComponentMapping(PF_DXT3, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_DXT5,			VK_FORMAT_BC3_UNORM_BLOCK);
			SetComponentMapping(PF_DXT5, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_BC5,			VK_FORMAT_BC5_UNORM_BLOCK);
			SetComponentMapping(PF_BC5, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		#elif PLATFORM_ANDROID
			MapFormatSupport(PF_ASTC_4x4,		VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
			SetComponentMapping(PF_ASTC_4x4, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_ASTC_6x6,		VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
			SetComponentMapping(PF_ASTC_6x6, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_ASTC_8x8,		VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
			SetComponentMapping(PF_ASTC_8x8, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_ASTC_10x10,		VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
			SetComponentMapping(PF_ASTC_10x10, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_ASTC_12x12,		VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
			SetComponentMapping(PF_ASTC_12x12, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);

			MapFormatSupport(PF_ETC2_RGB,		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
			SetComponentMapping(PF_ETC2_RGB, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE);

			MapFormatSupport(PF_ETC2_RGBA,		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
			SetComponentMapping(PF_ETC2_RGBA, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
		#endif
	}

	// Create Graphics Queue, here we submit command buffers for execution
	Queue = new FVulkanQueue(this, GfxQueueIndex);

	{
		FSamplerStateInitializerRHI Default(SF_Point);
		DefaultSampler = new FVulkanSamplerState(Default, *this);
	}
}

void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat)
{
	FPixelFormatInfo& FormatInfo = GPixelFormats[UEFormat];
	FormatInfo.PlatformFormat = VulkanFormat;
	FormatInfo.Supported = IsFormatSupported(VulkanFormat);

	if(!FormatInfo.Supported)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("EPixelFormat(%d) is not supported"), (int32)UEFormat);
	}
}

void FVulkanDevice::SetComponentMapping(EPixelFormat UEFormat, VkComponentSwizzle r, VkComponentSwizzle g, VkComponentSwizzle b, VkComponentSwizzle a)
{
	// Please ensure that we support the mapping, otherwise there is no point setting it.
	check(GPixelFormats[UEFormat].Supported);
	VkComponentMapping& ComponentMapping = PixelFormatComponentMapping[UEFormat];
	ComponentMapping.r = r;
	ComponentMapping.g = g;
	ComponentMapping.b = b;
	ComponentMapping.a = a;
}

void FVulkanDevice::MapFormatSupport(EPixelFormat UEFormat, VkFormat VulkanFormat, int32 BlockBytes)
{
	MapFormatSupport(UEFormat, VulkanFormat);
	FPixelFormatInfo& FormatInfo = GPixelFormats[UEFormat];
	FormatInfo.BlockBytes = BlockBytes;
}

bool FVulkanDevice::QueryGPU(int32 DeviceIndex)
{
	bool bDiscrete = false;
	vkGetPhysicalDeviceProperties(Gpu, &GpuProps);

	auto GetDeviceTypeString = [&]()
	{
		FString Info;
		switch (GpuProps.deviceType)
		{
		case  VK_PHYSICAL_DEVICE_TYPE_OTHER:
			Info = TEXT("Other");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			Info = TEXT("Integrated GPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			Info = TEXT("Discrete GPU");
			bDiscrete = true;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			Info = TEXT("Virtual GPU");
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			Info = TEXT("CPU");
			break;
		default:
			Info = TEXT("Unknown");
			break;
		}
		return Info;
	};

	UE_LOG(LogVulkanRHI, Display, TEXT("Initializing Device %d"), DeviceIndex);
	UE_LOG(LogVulkanRHI, Display, TEXT("API 0x%x Driver 0x%x VendorId 0x%x"), GpuProps.apiVersion, GpuProps.driverVersion, GpuProps.vendorID);
	UE_LOG(LogVulkanRHI, Display, TEXT("Name %s Device 0x%x Type %s"), ANSI_TO_TCHAR(GpuProps.deviceName), GpuProps.deviceID, *GetDeviceTypeString());
	UE_LOG(LogVulkanRHI, Display, TEXT("Max Descriptor Sets Bound %d Timestamps %d"), GpuProps.limits.maxBoundDescriptorSets, GpuProps.limits.timestampComputeAndGraphics);

	uint32 QueueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &QueueCount, nullptr);
	check(QueueCount >= 1);

	QueueProps.AddUninitialized(QueueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &QueueCount, QueueProps.GetData());

	return bDiscrete;
}

void FVulkanDevice::InitGPU(int32 DeviceIndex)
{
	vkGetPhysicalDeviceFeatures(Gpu, &Features);

	UE_LOG(LogVulkanRHI, Display, TEXT("Geometry %d Tessellation %d"), Features.geometryShader, Features.tessellationShader);
	MemoryManager.Init(this);
#if VULKAN_USE_BUFFER_HEAPS
	ResourceAllocationManager.Init(this);
#endif

	InitDevice();

	DescriptorPool = new FVulkanDescriptorPool(this);

#if VULKAN_USE_FENCE_MANAGER
	FenceManager.Init(this);
#endif

#if VULKAN_USE_NEW_STAGING_BUFFERS
	StagingManager.Init(this, Queue);
#endif

	// allocate ring buffer memory
	VBIBRingBuffer = new FVulkanRingBuffer(this, VULKAN_VBIB_RING_BUFFER_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
	UBRingBuffer = new FVulkanRingBuffer(this, VULKAN_UB_RING_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

#if VULKAN_ENABLE_PIPELINE_CACHE
	PipelineStateCache = new FVulkanPipelineStateCache(this);

	TArray<FString> CacheFilenames;
	if (PLATFORM_ANDROID)
	{
		CacheFilenames.Add(FPaths::GameDir() / TEXT("Build") / TEXT("ShaderCaches") / TEXT("Android") / TEXT("VulkanPSO.cache"));
	}
	CacheFilenames.Add(FPaths::GameSavedDir() / TEXT("VulkanPSO.cache"));
	PipelineStateCache->InitAndLoad(CacheFilenames);
#endif

	bool bSupportsTimestamps = (GpuProps.limits.timestampComputeAndGraphics == VK_TRUE);
	if (bSupportsTimestamps)
	{
		check(!TimestampQueryPool[0]);

		for (int32 Index = 0; Index < NumTimestampPools; ++Index)
		{
			TimestampQueryPool[Index] = new FVulkanTimestampQueryPool(this);
		}
	}
	else
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Timestamps not supported on Device"));
	}

	ImmediateContext = new FVulkanCommandListContext((FVulkanDynamicRHI*)GDynamicRHI, this, true);

	// Create Pending state, contains pipeline states such as current shader and etc..
	PendingState = new FVulkanPendingState(this);

#if VULKAN_ENABLE_DRAW_MARKERS
	VkCmdDbgMarkerBegin = (PFN_vkCmdDbgMarkerBegin)(void*)vkGetDeviceProcAddr(Device, "vkCmdDbgMarkerBegin");
	VkCmdDbgMarkerEnd = (PFN_vkCmdDbgMarkerEnd)(void*)vkGetDeviceProcAddr(Device, "vkCmdDbgMarkerEnd");
#endif
}

void FVulkanDevice::Destroy()
{
	WaitUntilIdle();

	if (TimestampQueryPool[0])
	{
		for (int32 Index = 0; Index < NumTimestampPools; ++Index)
		{
			TimestampQueryPool[Index]->Destroy();
			delete TimestampQueryPool[Index];
			TimestampQueryPool[Index] = nullptr;
		}
	}

	delete PendingState;

	delete VBIBRingBuffer;
	delete UBRingBuffer;

	delete ImmediateContext;
	ImmediateContext = nullptr;

#if VULKAN_ENABLE_PIPELINE_CACHE
	delete PipelineStateCache;
#endif

	delete DefaultSampler;
	DefaultSampler = nullptr;

	delete DescriptorPool;

#if VULKAN_USE_NEW_STAGING_BUFFERS
	StagingManager.Deinit();
#endif
#if VULKAN_USE_BUFFER_HEAPS
	ResourceAllocationManager.Deinit();
#endif

	delete Queue;

#if VULKAN_USE_FENCE_MANAGER
	FenceManager.Deinit();
#endif

	MemoryManager.Deinit();

	vkDestroyDevice(Device, nullptr);
	Device = VK_NULL_HANDLE;
}

void FVulkanDevice::WaitUntilIdle()
{
	VERIFYVULKANRESULT(vkDeviceWaitIdle(Device));
}

void FVulkanDevice::EndCommandBufferBlock(FVulkanCmdBuffer* CmdBuffer)
{
	check(CmdBuffer);
	GetQueue()->SubmitBlocking(CmdBuffer);
}

bool FVulkanDevice::IsFormatSupported(VkFormat Format) const
{
	const VkFormatProperties& Prop = FormatProperties[Format];
	return (Prop.bufferFeatures != 0) || (Prop.linearTilingFeatures != 0) || (Prop.optimalTilingFeatures != 0);
}

const VkComponentMapping& FVulkanDevice::GetFormatComponentMapping(EPixelFormat UEFormat) const
{
	check(GPixelFormats[UEFormat].Supported);
	return PixelFormatComponentMapping[UEFormat];
}

void FVulkanDevice::BindSRV(FVulkanShaderResourceView* SRV, uint32 TextureIndex, EShaderFrequency Stage)
{
	FVulkanPendingState& State = GetPendingState();
	FVulkanBoundShaderState& ShaderState = State.GetBoundShaderState();

	// @todo vulkan: Is this actually important to test? Other RHIs haven't checked this, we'd have to add the shader param to handle this
	// check(&ShaderState.GetShader(SF_Vertex) == ResourceCast(VertexShaderRHI));

	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView(this);
		checkf(SRV->BufferView != VK_NULL_HANDLE, TEXT("Empty SRV"));
		ShaderState.SetBufferViewState(Stage, TextureIndex, SRV->BufferView);
	}
	else
	{
		ShaderState.SetBufferViewState(Stage, TextureIndex, nullptr);
	}
}
