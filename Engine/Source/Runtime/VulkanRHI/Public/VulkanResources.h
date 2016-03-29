// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanRHI.h"
#include "BoundShaderStateCache.h"
#include "VulkanShaderResources.h"
#include "VulkanState.h"

struct FVulkanDevice;
struct FVulkanQueue;
class FVulkanDescriptorPool;
struct FVulkanDescriptorSetsLayout;
struct FVulkanDescriptorSets;
class FVulkanCmdBuffer;
class FVulkanGlobalUniformPool;
class FVulkanBuffer;
class FVulkanBufferCPU;
struct FVulkanTextureBase;
class FVulkanTexture2D;
struct FVulkanBufferView;

namespace VulkanRHI
{
	class FDeviceMemoryAllocation;
	class FResourceAllocationInfo;
}

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FVulkanVertexDeclaration : public FRHIVertexDeclaration
{
public:
	FVertexDeclarationElementList Elements;

	FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements);

	static void EmptyCache();
};


struct FVulkanShader
{
	FVulkanShader(FVulkanDevice* InDevice) :
		ShaderModule(VK_NULL_HANDLE),
		Code(nullptr),
		CodeSize(0),
		Device(InDevice)
	{
	}

	virtual ~FVulkanShader();

	/** External bindings for this shader. */
	FVulkanCodeHeader CodeHeader;

	VkShaderModule ShaderModule;

	uint32* Code;
	uint32 CodeSize;
	FString DebugName;

	TArray<ANSICHAR> GlslSource;

	void Create(EShaderFrequency Frequency, const TArray<uint8>& InCode);

	const VkShaderModule& GetHandle() const { check(ShaderModule != VK_NULL_HANDLE); return ShaderModule; }

	enum EShaderSourceType
	{
		SHADER_TYPE_UNINITIALIZED	= -1,
		SHADER_TYPE_SPIRV			= 0,
		SHADER_TYPE_GLSL			= 1,
		SHADER_TYPE_ESSL			= 2,
	};

	static EShaderSourceType GetShaderType();

	inline const FVulkanShaderBindingTable& GetBindingTable() const
	{
		return BindingTable;
	}

	inline uint32 GetNumDescriptors() const
	{
		return BindingTable.NumDescriptors;
	}

	inline uint32 GetNumDescriptorsExcludingPackedUniformBuffers() const
	{
		return BindingTable.NumDescriptorsWithoutPackedUniformBuffers;
	}

private:
	FVulkanDevice* Device;
	FVulkanShaderBindingTable BindingTable;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, EShaderFrequency ShaderType>
class TVulkanBaseShader : public BaseResourceType, public IRefCountedObject, public FVulkanShader
{
public:
	TVulkanBaseShader(FVulkanDevice* InDevice) :
		FVulkanShader(InDevice)
	{
	}

	enum { StaticFrequency = ShaderType };

	void Create(const TArray<uint8>& InCode);

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

typedef TVulkanBaseShader<FRHIVertexShader, SF_Vertex> FVulkanVertexShader;
typedef TVulkanBaseShader<FRHIPixelShader, SF_Pixel> FVulkanPixelShader;
typedef TVulkanBaseShader<FRHIHullShader, SF_Hull> FVulkanHullShader;
typedef TVulkanBaseShader<FRHIDomainShader, SF_Domain> FVulkanDomainShader;
typedef TVulkanBaseShader<FRHIComputeShader, SF_Compute> FVulkanComputeShader;
typedef TVulkanBaseShader<FRHIGeometryShader, SF_Geometry> FVulkanGeometryShader;


/** Texture/RT wrapper. */
class FVulkanSurface
{
public:

	// Seperate method for creating image, this can be used to measure image size
	// After VkImage is no longer needed, dont forget to destroy/release it 
	static VkImage CreateImage(
		FVulkanDevice& InDevice,
		VkImageViewType ResourceType,
		EPixelFormat InFormat,
		uint32 SizeX, uint32 SizeY, uint32 SizeZ,
		bool bArray, uint32 ArraySize,
		uint32 NumMips,
		uint32 NumSamples,
		uint32 UEFlags,
		VkFormat* OutFormat = nullptr,
		VkImageCreateInfo* OutInfo = nullptr,
		VkMemoryRequirements* OutMemoryRequirements = nullptr,
		bool bForceLinearTexture = false);

	FVulkanSurface(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format,
					uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize,
					uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	FVulkanSurface(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format,
					uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage,
					uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	virtual ~FVulkanSurface();

	void Destroy();

	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize();

	/**
	 * Returns one of the texture's mip-maps stride.
	 */
	void GetMipStride(uint32 MipIndex, uint32& Stride);

	/*
	 * Returns the memory offset to the texture's mip-map.
	 */
	void GetMipOffset(uint32 MipIndex, uint32& Offset);

	/**
	* Returns how much memory a single mip uses.
	*/
	void GetMipSize(uint32 MipIndex, uint32& MipBytes);

	inline VkImageViewType GetViewType() const { return ViewType; }

	inline VkImageTiling GetTiling() const { return Tiling; }

	inline uint32 GetNumMips() const { return NumMips; }

	static VkImageAspectFlags GetAspectMask(VkFormat Format);

	VkImageAspectFlags GetAspectMask() const;

	FVulkanDevice* Device;

	VkImage Image;
	
	VkImageLayout ImageLayout;
	VkFormat InternalFormat;
	uint32 Width, Height, Depth;
	TMap<uint32, void*>	MipMapMapping;
	EPixelFormat Format;
	VkMemoryPropertyFlags MemProps;

	// format->index key, used with the pipeline state object key
	uint8 FormatKey;
private:

	// Used to clear render-target objects on creation
	void ClearBlocking(const FClearValueBinding& ClearValueBinding);

private:
	// Linear or Optimal. Based on tiling, map/unmap is handled differently.
	// Optimal	Image memory cannot be directly mapped to a pointer (opposite to linear tiling).
	//			Therefore an intermediate buffer is used to execute buffet to image copy command.
	//			Keep in mind, current implementation is write-only. Reading data from the mapped buffer does not
	//			represent the data inside the image.
	//
	// Linear	Allows direct buffer mapping to a pointer, without needing to create an intermediate buffer.
	//			NOTE (NVIDIA and ??): linear-cubemaps are not supported.
	//
	// Note:	We should try keep all tilings on all platform the same and try to have as least exceptions as possible
	//			Perhaps some sort of setup-map should be used to create desired configuration.
	VkImageTiling Tiling;

	VkImageViewType	ViewType;	// 2D, CUBE, 2DArray and etc..

	bool bIsImageOwner;
#if VULKAN_USE_MEMORY_SYSTEM
	VulkanRHI::FDeviceMemoryAllocation* Allocation;
#else
	VkDeviceMemory DeviceMemory;
#endif


	// Used for optimal tiling mode
	// Intermediate buffer, used to copy image data into the optimal image-buffer.
	TMap<uint32, TRefCountPtr<FVulkanBuffer>> MipMapBuffer;

	uint32 NumMips;

	uint32 NumSamples;

	VkImageAspectFlags AspectMask;
};


struct FVulkanTextureView
{
	FVulkanTextureView() :
		View(VK_NULL_HANDLE)
	{
	}

	void Create(FVulkanDevice& Device, FVulkanSurface& Surface, VkImageViewType ViewType, VkFormat Format, uint32 NumMips);

	void Destroy(FVulkanDevice& Device);

	VkImageView View;
};

/** The base class of resources that may be bound as shader resources. */
class FVulkanBaseShaderResource : public IRefCountedObject
{
};

struct FVulkanTextureBase : public FVulkanBaseShaderResource
{
	#if VULKAN_HAS_DEBUGGING_ENABLED
		static int32 GetAllocatedCount();

		static int32 GetAllocationBalance();

		inline int32 GetAllocationNumber() const { return AllocationNumber; }
	#endif

	static FVulkanTextureBase* Cast(FRHITexture* Texture);

	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo = FRHIResourceCreateInfo());
	virtual ~FVulkanTextureBase();

	FVulkanSurface Surface;
	FVulkanTextureView View;

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Surface and view for MSAA render target, valid only when created with NumSamples > 1
	FVulkanSurface* MSAASurface;
	FVulkanTextureView MSAAView;
#endif

	VkImage StagingImage;
	VkDeviceMemory PixelBufferMemory;

private:
#if VULKAN_HAS_DEBUGGING_ENABLED
	int32 AllocationNumber;
#endif
};

class FVulkanTexture2D : public FRHITexture2D, public FVulkanTextureBase
{
public:
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	virtual ~FVulkanTexture2D();

	void Destroy(FVulkanDevice& Device);

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

class FVulkanTexture2DArray : public FRHITexture2DArray, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Format, SizeX, SizeY, 1, /*bArray=*/ true, ArraySize, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
	{
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

class FVulkanTexture3D : public FRHITexture3D, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
	:	FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_3D, Format, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, /*NumSamples=*/ 1, Flags, BulkData)
	{
	
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

class FVulkanTextureCube : public FRHITextureCube, public FVulkanTextureBase
{
public:
	FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTextureCube();

	void Destroy(FVulkanDevice& Device);

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

class FVulkanTextureReference : public FRHITextureReference, public FVulkanTextureBase
{
public:
	explicit FVulkanTextureReference(FVulkanDevice& Device, FLastRenderTimeContainer* InLastRenderTime)
	:	FRHITextureReference(InLastRenderTime)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_MAX_ENUM, PF_Unknown, 0, 0, 0, VK_NULL_HANDLE, VK_NULL_HANDLE, 0)
	{}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}

	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}

	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	void SetReferencedTexture(FRHITexture* InTexture);
};

/** Given a pointer to a RHI texture that was created by the Vulkan RHI, returns a pointer to the FVulkanTextureBase it encapsulates. */
inline FVulkanTextureBase* GetVulkanTextureFromRHITexture(FRHITexture* Texture)
{
	if (!Texture)
	{
		return NULL;
	}
	else if (Texture->GetTexture2D())
	{
		return static_cast<FVulkanTexture2D*>(Texture);
	}
	else if (Texture->GetTextureReference())
	{
		return static_cast<FVulkanTextureReference*>(Texture);
	}
	else if (Texture->GetTexture2DArray())
	{
		check(false);	//@TODO: Not supported yet!
		return static_cast<FVulkanTexture2DArray*>(Texture);
	}
	else if (Texture->GetTexture3D())
	{
		return static_cast<FVulkanTexture3D*>(Texture);
	}
	else if (Texture->GetTextureCube())
	{
		return static_cast<FVulkanTextureCube*>(Texture);
	}
	else
	{
		UE_LOG(LogVulkanRHI, Fatal, TEXT("Unknown Vulkan RHI texture type"));
		return NULL;
	}
}

class FVulkanQueryPool
{
public:
	FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InNumQueries, VkQueryType InQueryType);
	virtual ~FVulkanQueryPool(){}
	virtual void Destroy();

	VkQueryPool QueryPool;
	const uint32 NumQueries;
	const VkQueryType QueryType;

	FVulkanDevice* Device;
};

class FVulkanTimestampQueryPool : public FVulkanQueryPool
{
public:
	enum
	{
		StartFrame = 0,
		EndFrame = 1,
		StartUser = 2,

		//#todo-rco: What is the limit?
		MaxNumUser = 62,

		TotalQueries = MaxNumUser + StartUser,
	};

	FVulkanTimestampQueryPool(FVulkanDevice* InDevice);
	virtual ~FVulkanTimestampQueryPool(){}

	void WriteStartFrame(VkCommandBuffer CmdBuffer);
	void WriteEndFrame(VkCommandBuffer CmdBuffer);

	void CalculateFrameTime();

	inline double GetTimeStampsPerSecond() const
	{
		return TimeStampsPerSeconds;
	}

	inline double GetSecondsPerTimestamp() const
	{
		return SecondsPerTimestamp;
	}

	void ResetUserQueries();

	// Returns -1 if there is no more room
	int32 AllocateUserQuery();

	void WriteTimestamp(VkCommandBuffer CmdBuffer, int32 UserQuery, VkPipelineStageFlagBits PipelineStageBits);

	uint32 CalculateTimeFromUserQueries(int32 UserBegin, int32 UserEnd, bool bWait);

protected:
	double TimeStampsPerSeconds;
	double SecondsPerTimestamp;
	int32 UsedUserQueries;
	bool bFirst;
};

/** Vulkan occlusion query */
class FVulkanRenderQuery : public FRHIRenderQuery
{
public:

	/** Initialization constructor. */
	FVulkanRenderQuery(ERenderQueryType InQueryType);

	~FVulkanRenderQuery();

	/**
	 * Kick off an occlusion test 
	 */
	void Begin();

	/**
	 * Finish up an occlusion test 
	 */
	void End();

};

struct FVulkanBufferView : public FRHIResource
{
	FVulkanBufferView() :
		View(VK_NULL_HANDLE),
		Flags(0)
	{
	}

	void Create(FVulkanDevice& Device, FVulkanBuffer& Buffer, EPixelFormat Format, uint32 Offset, uint32 Size);
	void Destroy(FVulkanDevice& Device);

	VkBufferView View;
	VkFlags Flags;
};

class FVulkanBuffer : public FRHIResource
{
public:
	FVulkanBuffer(FVulkanDevice& Device, uint32 InSize, VkFlags InUsage, VkMemoryPropertyFlags InMemPropertyFlags, bool bAllowMultiLock, const char* File, int32 Line);
	virtual ~FVulkanBuffer();

	inline const VkBuffer& GetBufferHandle() const { return Buf; }

	inline uint32 GetSize() const { return Size; }

	void* Lock(uint32 InSize, uint32 InOffset = 0);

	void Unlock();

	// By not providing a pending state, the copy is occuring immediately and is blocking..
	void CopyTo(FVulkanSurface& Surface, const VkBufferImageCopy& CopyDesc, struct FVulkanPendingState* State = nullptr);

	inline const VkFlags& GetFlags() const { return Usage; }

private:
	FVulkanDevice& Device;
	VkBuffer Buf;
#if VULKAN_USE_MEMORY_SYSTEM
	#if VULKAN_USE_BUFFER_HEAPS
		VulkanRHI::FResourceAllocationInfo* MemoryInfo;
	#else
		VulkanRHI::FDeviceMemoryAllocation* Allocation;
	#endif
#else
	VkDeviceMemory Mem;
#endif
	uint32 Size;
	VkFlags Usage;

	void* BufferPtr;	
	VkMappedMemoryRange MappedRange;

	bool bAllowMultiLock;
	int32 LockStack;
};

struct FVulkanRingBuffer
{
public:
	FVulkanRingBuffer(FVulkanDevice* Device, uint64 TotalSize, VkFlags Usage);
	~FVulkanRingBuffer();

	// allocate some space in the ring buffer
	uint64 AllocateMemory(uint64 Size, uint32 Alignment);

	// track the allocations
	FVulkanBuffer* Buffer;

protected:
	uint64 BufferSize;
	uint64 BufferOffset;
	FVulkanDevice* Device;
};

class FVulkanMultiBuffer
{
public:
	FVulkanMultiBuffer(FVulkanDevice& InDevice, uint32 InSize, EBufferUsageFlags Usage, VkBufferUsageFlagBits VkUsage);
	virtual ~FVulkanMultiBuffer();

	FVulkanBuffer* GetBuffer();
	inline uint64 GetOffset()
	{
		// for non-ring buffer allocations, this will always be 0, which is what we want, no need for extra checking
		return RingBufferOffset;
	}

	inline bool IsDynamic()
	{
		// 1 buffer means static, 0 or N is dynamic of some sort
		return NumBuffers != 1;
	}
	void* Lock(EResourceLockMode LockMode, uint32 Size = 0, uint32 Offset = 0);
	void Unlock();

protected:
	FVulkanDevice& Device;

	// 0+ premade buffers (0 for volatile)
	FVulkanBuffer* Buffers[NUM_RENDER_BUFFERS];
	// how many there are
	uint8 NumBuffers;
	// current buffer for dynamic
	uint8 DynamicBufferIndex;
	// size needed for volatile allocations
	uint32 BufferSize;
	// current ring buffer allocation for volatile
	uint64 RingBufferOffset;
};

/** Index buffer resource class that stores stride information. */
class FVulkanIndexBuffer : public FRHIIndexBuffer, public FVulkanMultiBuffer
{
public:
	FVulkanIndexBuffer(FVulkanDevice& Device, uint32 InStride, uint32 InSize, uint32 InUsage);

	VkIndexType IndexType;
};

struct FVulkanDynamicLockInfo
{
	FVulkanDynamicLockInfo()
		: Data(nullptr)
		, Buffer(nullptr)
		, Offset(0)
		, PoolElementIndex(-1)
	{
	}
	void* Data;
	FVulkanBuffer* Buffer;
	VkDeviceSize Offset;
	int32 PoolElementIndex;
};

class FVulkanDynamicPooledBuffer
{
public:
	FVulkanDynamicPooledBuffer(FVulkanDevice& Device, uint32 NumPoolElements, const VkDeviceSize* PoolElements, VkBufferUsageFlagBits UsageFlags);
	virtual ~FVulkanDynamicPooledBuffer();

	FVulkanDynamicLockInfo Lock(VkDeviceSize InSize);

	inline void Unlock(FVulkanDynamicLockInfo LockInfo)
	{
		Elements[LockInfo.PoolElementIndex].Unlock(LockInfo);
	}

	void EndFrame();

protected:
	enum
	{
		NUM_FRAMES = 3,
	};

	struct FPoolElement
	{
		VkDeviceSize NextFreeOffset;
		VkDeviceSize LastLockSize;
		FVulkanBuffer* Buffers[NUM_FRAMES];
		VkDeviceSize Size;

		FPoolElement();

		inline bool CanLock(VkDeviceSize InSize) const
		{
			return NextFreeOffset + InSize <= Size;
		}

		FVulkanDynamicLockInfo Lock(VkDeviceSize InSize);
		void Unlock(FVulkanDynamicLockInfo LockInfo);
	};

	TArray<FPoolElement> Elements;
};

/** Vertex buffer resource class that stores usage type. */
class FVulkanVertexBuffer : public FRHIVertexBuffer, public FVulkanMultiBuffer
{
public:
	FVulkanVertexBuffer(FVulkanDevice& InDevice, uint32 InSize, uint32 InUsage);

	inline VkFlags GetFlags() const { return Flags; }

private:
	VkFlags Flags;
};

class FVulkanUniformBuffer : public FRHIUniformBuffer
{
public:
	FVulkanUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage);

	~FVulkanUniformBuffer();

	TArray<uint8> ConstantData;

	const TArray<TRefCountPtr<FRHIResource>>& GetResourceTable() const { return ResourceTable; }

	inline FVulkanBuffer* GetRealUB() const { return Buffer.GetReference(); }

private:
	TRefCountPtr<FVulkanBuffer> Buffer;
	TArray<TRefCountPtr<FRHIResource>> ResourceTable;
};


class FVulkanStructuredBuffer : public FRHIStructuredBuffer
{
public:
	FVulkanStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage);

	~FVulkanStructuredBuffer();

};



class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView
{
public:

	// the potential resources to refer to with the UAV object
	TRefCountPtr<FVulkanStructuredBuffer> SourceStructuredBuffer;
	TRefCountPtr<FVulkanVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FRHITexture> SourceTexture;
};


class FVulkanShaderResourceView : public FRHIShaderResourceView
{
public:

	void UpdateView(FVulkanDevice* Device);

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FVulkanBufferView> BufferView;
	TRefCountPtr<FVulkanVertexBuffer> SourceVertexBuffer;
	EPixelFormat BufferViewFormat;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;

	~FVulkanShaderResourceView();

};

class FVulkanVertexInputStateInfo
{
public:
	FVulkanVertexInputStateInfo();

	void Create(uint32 BindingsNum, VkVertexInputBindingDescription* VertexInputBindings, uint32 AttributesNum, VkVertexInputAttributeDescription* Attributes);

	inline uint32 GetHash() const
	{
		check(Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
		return Hash;
	}

	inline const VkPipelineVertexInputStateCreateInfo& GetInfo() const
	{
		return Info;
	}

protected:
	VkPipelineVertexInputStateCreateInfo Info;
	uint32 Hash;

	friend class FVulkanBoundShaderState;
#if VULKAN_ENABLE_PIPELINE_CACHE
	friend class FVulkanPipelineStateCache;
#endif
};

struct FVulkanPipelineGraphicsKey
{
	uint64 Key[2];

	FVulkanPipelineGraphicsKey()
	{
		Key[0] = 0;
		Key[1] = 0;
	}

	bool operator == (const FVulkanPipelineGraphicsKey& In) const
	{
		return Key[0] == In.Key[0] && Key[1] == In.Key[1];
	}
};

inline uint32 GetTypeHash(const FVulkanPipelineGraphicsKey& Key)
{
	return GetTypeHash(Key.Key[0]) ^ GetTypeHash(Key.Key[1]);
}

/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FVulkanBoundShaderState : public FRHIBoundShaderState
{
public:
	FVulkanBoundShaderState(
		FVulkanDevice* InDevice,
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI);

	~FVulkanBoundShaderState();

	// Called when the shader is set by the engine as current pending state
	// After the shader is set, it is expected that all require states are provided/set by the engine
	void ResetState();

	class FVulkanPipeline* PrepareForDraw(const struct FVulkanPipelineGraphicsKey& PipelineKey, uint32 VertexInputKey, const struct FVulkanPipelineState& State);

    void UpdateDescriptorSets(FVulkanGlobalUniformPool* GlobalUniformPool);

	void BindDescriptorSets(FVulkanCmdBuffer* Cmd);

	inline void BindVertexStreams(FVulkanCmdBuffer* Cmd, const void* VertexStreams)
	{
		if (bDirtyVertexStreams)
		{
			InternalBindVertexStreams(Cmd, VertexStreams);
			bDirtyVertexStreams = false;
		}
	}

	inline void MarkDirtyVertexStreams()
	{
		bDirtyVertexStreams = true;
	}

	// Querying GetPipelineLayout() generates VkPipelineLayout on the first time
	const VkPipelineLayout& GetPipelineLayout() const;

	inline const FVulkanDescriptorSetsLayout& GetDescriptorSetsLayout() const
	{
		check(Layout);
		return *Layout;
	}

	inline const FVulkanVertexInputStateInfo& GetVertexInputStateInfo() const
	{
		check(VertexInputStateInfo.Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
		return VertexInputStateInfo;
	}

#if VULKAN_ENABLE_PIPELINE_CACHE
	inline FSHAHash const* GetShaderHashes() const
	{
		return ShaderHashes;
	}
#endif

	// Binding
	void SetShaderParameter(EShaderFrequency Stage, uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValue);

	void SetTexture(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureBase* VulkanTextureBase);

	void SetSamplerState(EShaderFrequency Stage, uint32 BindPoint, FVulkanSamplerState* Sampler);

	void SetBufferViewState(EShaderFrequency Stage, uint32 BindPoint, FVulkanBufferView* View);

	void SetUniformBufferConstantData(FVulkanPendingState& PendingState, EShaderFrequency Stage, uint32 BindPoint, const TArray<uint8>& ConstantData);
	void SetUniformBuffer(FVulkanPendingState& PendingState, EShaderFrequency Stage, uint32 BindPoint, const FVulkanBuffer* UniformBuffer);

	inline const FVulkanShader* GetShaderPtr(EShaderFrequency Stage) const
	{
		switch (Stage)
		{
		case SF_Vertex:		return VertexShader;
		case SF_Hull:		return HullShader;
		case SF_Domain:		return DomainShader;
		case SF_Pixel:		return PixelShader;
		case SF_Geometry:	return GeometryShader;
			//#todo-rco: Really should assert here...
		default:			return nullptr;
		}
	}

	// Has no internal verify.
	// If stage does not exists, returns a "nullptr".
	inline FVulkanShader* GetShaderPtr(EShaderFrequency Stage)
	{
		switch (Stage)
		{
		case SF_Vertex:		return VertexShader;
		case SF_Hull:		return HullShader;
		case SF_Domain:		return DomainShader;
		case SF_Pixel:		return PixelShader;
		case SF_Geometry:	return GeometryShader;
			//#todo-rco: Really should assert here...
		default:			return nullptr;
		}
	}

	inline bool HasShaderStage(EShaderFrequency Stage) const
	{
		return (GetShaderPtr(Stage) != nullptr);
	}

	// Verifies if the shader stage exists
	inline const FVulkanShader& GetShader(EShaderFrequency Stage) const
	{
		const FVulkanShader* Shader = GetShaderPtr(Stage);
		check(Shader);
		return *Shader;
	}


	#if VULKAN_HAS_DEBUGGING_ENABLED
		// Basic error handling. This allows us to track errors, while also skip draw calls
		// which potentially may crash the driver
		bool HasError() const { return LastError.Len() > 0; }

		const FString& GetLastError() const { return LastError; }

		const VkDescriptorImageInfo* GetStageImageDescriptors(EShaderFrequency Stage, uint32& OutNum) const
		{
			check(Stage < SF_Compute);
			OutNum = ImageDescCount[Stage];
			return DescriptorImageInfoForStage[Stage];
		}
	#endif

		inline void BindPipeline(VkCommandBuffer CmdBuffer, VkPipeline NewPipeline)
		{
			if (LastBoundPipeline != NewPipeline)
			{
				//#todo-rco: Compute
				vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, NewPipeline);
				LastBoundPipeline = NewPipeline;
			}
		}

private:
	void InitGlobalUniforms(EShaderFrequency Stage, const FVulkanShaderSerializedBindings& SerializedBindings);

	void GenerateLayoutBindings();

	void GenerateLayoutBindings(EShaderFrequency Stage, const FVulkanShaderSerializedBindings& SerializedBindings);

	void GenerateVertexInputStateInfo();

	void GetDescriptorInfoCounts(uint32& OutSamplerCount, uint32& OutSamplerBufferCount, uint32& OutUniformBufferCount);

	void InternalBindVertexStreams(FVulkanCmdBuffer* Cmd, const void* VertexStreams);

	FCachedBoundShaderStateLink CacheLink;

	/** Cached vertex structure */
	TRefCountPtr<FVulkanVertexDeclaration> VertexDeclaration;

#if VULKAN_ENABLE_PIPELINE_CACHE
	friend class FVulkanPipelineStateCache;

	FSHAHash ShaderHashes[SF_NumFrequencies];

	TLinkedList<FVulkanBoundShaderState*> GlobalListLink;
#endif

	/** Cached shaders */
	TRefCountPtr<FVulkanVertexShader> VertexShader;
	TRefCountPtr<FVulkanPixelShader> PixelShader;
	TRefCountPtr<FVulkanHullShader> HullShader;
	TRefCountPtr<FVulkanDomainShader> DomainShader;
	TRefCountPtr<FVulkanGeometryShader> GeometryShader;

	TArray<VkDescriptorImageInfo> DescriptorImageInfo;
	TArray<VkDescriptorBufferInfo> DescriptorBufferInfo;
	TArray<VkWriteDescriptorSet> DescriptorWrites;

	mutable VkPipelineLayout PipelineLayout;

	FVulkanDevice* Device;
	FVulkanDescriptorSetsLayout* Layout;
	FVulkanDescriptorSets* CurrDescriptorSets;
	VkPipeline LastBoundPipeline;
	bool bDirtyVertexStreams;

	VkDescriptorBufferInfo* DescriptorBufferInfoForPackedUBForStage[SF_Compute];
	TArray<uint8> PackedUniformBufferStaging[SF_Compute][CrossCompiler::PACKED_TYPEINDEX_MAX];
	uint8 DirtyPackedUniformBufferStaging[SF_Compute];
	// Mask to set all packed UBs to dirty
	uint8 DirtyPackedUniformBufferStagingMask[SF_Compute];

	VkDescriptorBufferInfo* DescriptorBufferInfoForStage[SF_Compute];

	bool DirtyTextures[SF_Compute];

	// At this moment unused, the samplers are mapped together with texture/image
	// we are using "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"
	bool DirtySamplerStates[SF_Compute];
	VkDescriptorImageInfo* DescriptorImageInfoForStage[SF_Compute];

	TArray< VkWriteDescriptorSet* > SRVWriteInfoForStage[SF_Compute];
	bool DirtySRVs[SF_Compute];

#if VULKAN_ENABLE_RHI_DEBUGGING
	struct FDebugInfo
	{
		const FVulkanTextureBase* Textures[SF_Compute][32];
		const FVulkanSamplerState* SamplerStates[SF_Compute][32];
		const FVulkanBufferView* SRVs[SF_Compute][32];
		const FVulkanBuffer* UBs[SF_Compute][32];
	};
	FDebugInfo DebugInfo;
#endif

	// Vertex Input
	// Attributes and Bindings are filled in void FVulkanBoundShaderState::GenerateVertexInputStateInfo()
	// InOutMask is used from VertexShader-Header to filter out active attributs
	void ProcessVertexElementForBinding(const FVertexElement& Element);

	void CreateDescriptorWriteInfo(uint32 UniformSamplerCount, uint32 UniformSamplerBufferCount, uint32 UniformBufferCount);

	uint32 BindingsNum;
	uint32 BindingsMask;

	//#todo-rco: Remove these TMaps
	TMap<uint32, uint32> BindingToStream;
	TMap<uint32, uint32> StreamToBinding;
	VkVertexInputBindingDescription VertexInputBindings[MaxVertexElementCount];
	

	uint32 AttributesNum;
	VkVertexInputAttributeDescription Attributes[MaxVertexElementCount];


	// Vertex input configuration
	FVulkanVertexInputStateInfo VertexInputStateInfo;

	#if VULKAN_HAS_DEBUGGING_ENABLED
		void SetLastError(const FString& Error);

		// If the string is not empty, an error has occurred.
		FString LastError;
		uint32 ImageDescCount[SF_Compute];
	#endif

	// Members in Tmp are normally allocated each frame
	// To reduce the allocation, we reuse these array
	struct FTmp
	{
		TArray<VkBuffer> VertexBuffers;
		TArray<VkDeviceSize> VertexOffsets;
	} Tmp;

	// these are the cache pipeline state objects used in this BSS
	TMap<FVulkanPipelineGraphicsKey, class FVulkanPipeline*> PipelineCache;

	// descriptor set objects used by this BSS
	uint64 LastFrameRendered;
	int32 CurrentDS[VULKAN_NUM_COMMAND_BUFFERS];
	TArray<FVulkanDescriptorSets*> DescriptorSets[VULKAN_NUM_COMMAND_BUFFERS];
};

template<class T>
struct TVulkanResourceTraits
{
};
template<>
struct TVulkanResourceTraits<FRHIVertexDeclaration>
{
	typedef FVulkanVertexDeclaration TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexShader>
{
	typedef FVulkanVertexShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIGeometryShader>
{
	typedef FVulkanGeometryShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIHullShader>
{
	typedef FVulkanHullShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDomainShader>
{
	typedef FVulkanDomainShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIPixelShader>
{
	typedef FVulkanPixelShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIComputeShader>
{
	typedef FVulkanComputeShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIBoundShaderState>
{
	typedef FVulkanBoundShaderState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture3D>
{
	typedef FVulkanTexture3D TConcreteType;
};
//template<>
//struct TVulkanResourceTraits<FRHITexture>
//{
//	typedef FVulkanTexture TConcreteType;
//};
template<>
struct TVulkanResourceTraits<FRHITexture2D>
{
	typedef FVulkanTexture2D TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture2DArray>
{
	typedef FVulkanTexture2DArray TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITextureCube>
{
	typedef FVulkanTextureCube TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRenderQuery>
{
	typedef FVulkanRenderQuery TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUniformBuffer>
{
	typedef FVulkanUniformBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIIndexBuffer>
{
	typedef FVulkanIndexBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIStructuredBuffer>
{
	typedef FVulkanStructuredBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexBuffer>
{
	typedef FVulkanVertexBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIShaderResourceView>
{
	typedef FVulkanShaderResourceView TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUnorderedAccessView>
{
	typedef FVulkanUnorderedAccessView TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHISamplerState>
{
	typedef FVulkanSamplerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRasterizerState>
{
	typedef FVulkanRasterizerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDepthStencilState>
{
	typedef FVulkanDepthStencilState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIBlendState>
{
	typedef FVulkanBlendState TConcreteType;
};

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}
