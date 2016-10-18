// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanState.h"
#include "VulkanUtil.h"
#include "BoundShaderStateCache.h"
#include "VulkanShaderResources.h"
#include "VulkanState.h"
#include "VulkanMemory.h"

class FVulkanDevice;
class FVulkanQueue;
class FVulkanDescriptorPool;
class FVulkanDescriptorSetsLayout;
struct FVulkanDescriptorSets;
class FVulkanCmdBuffer;
class FVulkanGlobalUniformPool;
class FVulkanBuffer;
class FVulkanBufferCPU;
struct FVulkanTextureBase;
class FVulkanTexture2D;
struct FVulkanBufferView;
class FVulkanResourceMultiBuffer;
struct FVulkanSemaphore;
class FVulkanPendingState;

namespace VulkanRHI
{
	class FDeviceMemoryAllocation;
	class FOldResourceAllocation;
}

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FVulkanVertexDeclaration : public FRHIVertexDeclaration
{
public:
	FVertexDeclarationElementList Elements;

	FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements);

	static void EmptyCache();
};


class FVulkanShader
{
public:
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

#if 0
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
#endif

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

	// Full includes Depth+Stencil
	inline VkImageAspectFlags GetFullAspectMask() const
	{
		return FullAspectMask;
	}

	// Only Depth or Stencil
	inline VkImageAspectFlags GetPartialAspectMask() const
	{
		return PartialAspectMask;
	}

	FVulkanDevice* Device;

	VkImage Image;
	
	VkFormat InternalFormat;
	uint32 Width, Height, Depth;
	TMap<uint32, void*>	MipMapMapping;
	EPixelFormat Format;
	uint32 UEFlags;
	VkMemoryPropertyFlags MemProps;

	// format->index key, used with the pipeline state object key
	uint8 FormatKey;
private:

	// Used to clear render-target objects on creation
	void InitialClear(const FClearValueBinding& ClearValueBinding, bool bTransitionToPresentable);

private:
	VkImageTiling Tiling;
	VkImageViewType	ViewType;

	bool bIsImageOwner;
	VulkanRHI::FDeviceMemoryAllocation* Allocation;
	TRefCountPtr<VulkanRHI::FOldResourceAllocation> ResourceAllocation;

	uint32 NumMips;

	uint32 NumSamples;

	VkImageAspectFlags FullAspectMask;
	VkImageAspectFlags PartialAspectMask;

	friend struct FVulkanTextureBase;
};


struct FVulkanTextureView
{
	FVulkanTextureView()
		: View(VK_NULL_HANDLE)
	{
	}

	static VkImageView StaticCreate(FVulkanDevice& Device, VkImage Image, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle = false);

	void Create(FVulkanDevice& Device, VkImage Image, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices);
	void Destroy(FVulkanDevice& Device);

	VkImageView View;
};

/** The base class of resources that may be bound as shader resources. */
class FVulkanBaseShaderResource : public IRefCountedObject
{
};

struct FVulkanTextureBase : public FVulkanBaseShaderResource
{
	static FVulkanTextureBase* Cast(FRHITexture* Texture);

	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo = FRHIResourceCreateInfo());
	virtual ~FVulkanTextureBase();

	VkImageView CreateRenderTargetView(uint32 MipIndex, uint32 ArraySliceIndex);

	FVulkanSurface Surface;

	// View with all mips/layers
	FVulkanTextureView DefaultView;
	// View with all mips/layers, but if it's a Depth/Stencil, only the Depth view
	FVulkanTextureView* PartialView;

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Surface and view for MSAA render target, valid only when created with NumSamples > 1
	FVulkanSurface* MSAASurface;
	FVulkanTextureView MSAAView;
#endif

private:
};

class FVulkanBackBuffer;
class FVulkanTexture2D : public FRHITexture2D, public FVulkanTextureBase
{
public:
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	virtual ~FVulkanTexture2D();

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

	virtual FVulkanBackBuffer* GetBackBuffer()
	{
		return nullptr;
	}

protected:
	// Only used for back buffer
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
};

class FVulkanBackBuffer : public FVulkanTexture2D
{
public:
	FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags);
	virtual ~FVulkanBackBuffer();

	virtual FVulkanBackBuffer* GetBackBuffer() override final
	{
		return this;
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
	void WriteEndFrame(FVulkanCmdBuffer* CmdBuffer);

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

	FVulkanCmdBuffer* LastEndCmdBuffer;
	uint64 LastFenceSignaledCounter;
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

struct FVulkanBufferView : public FRHIResource, public VulkanRHI::FDeviceChild
{
	FVulkanBufferView(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
		, View(VK_NULL_HANDLE)
		, Flags(0)
	{
	}

	virtual ~FVulkanBufferView()
	{
		Destroy();
	}

	void Create(FVulkanBuffer& Buffer, EPixelFormat Format, uint32 Offset, uint32 Size);
	void Create(FVulkanResourceMultiBuffer* Buffer, EPixelFormat Format, uint32 Offset, uint32 Size);
	void Destroy();

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

	inline const VkFlags& GetFlags() const { return Usage; }

private:
	FVulkanDevice& Device;
	VkBuffer Buf;
	VulkanRHI::FDeviceMemoryAllocation* Allocation;
	uint32 Size;
	VkFlags Usage;

	void* BufferPtr;	
	VkMappedMemoryRange MappedRange;

	bool bAllowMultiLock;
	int32 LockStack;
};

struct FVulkanRingBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage);
	~FVulkanRingBuffer();

	// allocate some space in the ring buffer
	uint64 AllocateMemory(uint64 Size, uint32 Alignment);

	inline uint32 GetBufferOffset() const
	{
		return BufferSuballocation->GetOffset();
	}

	inline VkBuffer GetHandle() const
	{
		return BufferSuballocation->GetHandle();
	}

	inline void* GetMappedPointer()
	{
		return BufferSuballocation->GetMappedPointer();
	}

protected:
	uint64 BufferSize;
	uint64 BufferOffset;
	uint32 MinAlignment;
	VulkanRHI::FBufferSuballocation* BufferSuballocation;
};

class FVulkanResourceMultiBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, VkBufferUsageFlags InBufferUsageFlags, uint32 InSize, uint32 InUEUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual ~FVulkanResourceMultiBuffer();

	inline VkBuffer GetHandle() const
	{
		if (NumBuffers == 0)
		{
			return VolatileLockInfo.GetHandle();
		}
		return Buffers[DynamicBufferIndex]->GetHandle();
	}

	inline bool IsDynamic() const
	{
		return NumBuffers > 1;
	}

	inline bool IsVolatile() const
	{
		return NumBuffers == 0;
	}

	inline uint32 GetVolatileLockCounter() const
	{
		check(IsVolatile());
		return VolatileLockInfo.LockCounter;
	}

	// Offset used for Binding a VkBuffer
	inline uint32 GetOffset() const
	{
		if (NumBuffers == 0)
		{
			return VolatileLockInfo.GetBindOffset();
		}
		return Buffers[DynamicBufferIndex]->GetOffset();
	}

	VkBufferUsageFlags GetBufferUsageFlags() const
	{
		return BufferUsageFlags;
	}

	void* Lock(EResourceLockMode LockMode, uint32 Size, uint32 Offset);
	void Unlock();

protected:
	uint32 UEUsage;
	VkBufferUsageFlags BufferUsageFlags;
	uint32 NumBuffers;
	uint32 DynamicBufferIndex;
	TArray<TRefCountPtr<VulkanRHI::FBufferSuballocation>> Buffers;
	VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo VolatileLockInfo;
	friend class FVulkanCommandListContext;
};

class FVulkanIndexBuffer : public FRHIIndexBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanIndexBuffer(FVulkanDevice* InDevice, uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);

	inline VkIndexType GetIndexType() const
	{
		return IndexType;
	}

private:
	VkIndexType IndexType;
};

class FVulkanVertexBuffer : public FRHIVertexBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanVertexBuffer(FVulkanDevice* InDevice, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
};

class FVulkanUniformBuffer : public FRHIUniformBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage);

	~FVulkanUniformBuffer();

	TArray<uint8> ConstantData;

	const TArray<TRefCountPtr<FRHIResource>>& GetResourceTable() const { return ResourceTable; }

private:
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

	FVulkanUnorderedAccessView()
		: MipIndex(0)
	{
	}

	uint32 MipIndex;
};


class FVulkanShaderResourceView : public FRHIShaderResourceView, public VulkanRHI::FDeviceChild
{
public:
	FVulkanShaderResourceView(FVulkanDevice* Device)
		: VulkanRHI::FDeviceChild(Device)
		, BufferViewFormat(PF_Unknown)
		, MipLevel(0)
		, NumMips(-1)
		, VolatileLockCounter(MAX_uint32)
	{
	}

	void UpdateView();

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FVulkanBufferView> BufferView;
	TRefCountPtr<FVulkanVertexBuffer> SourceVertexBuffer;
	EPixelFormat BufferViewFormat;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	uint32 MipLevel;
	uint32 NumMips;

	~FVulkanShaderResourceView();

protected:
	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
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

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool);

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
	void SetTextureView(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureView& TextureView);

	void SetUniformBufferConstantData(FVulkanPendingState& PendingState, EShaderFrequency Stage, uint32 BindPoint, const TArray<uint8>& ConstantData);
	void SetUniformBuffer(FVulkanPendingState& PendingState, EShaderFrequency Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer);

	void SetSRV(EShaderFrequency Stage, uint32 TextureIndex, FVulkanShaderResourceView* SRV);

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

	void BindPipeline(VkCommandBuffer CmdBuffer, VkPipeline NewPipeline);

private:
	void InitGlobalUniforms(EShaderFrequency Stage, const FVulkanShaderSerializedBindings& SerializedBindings);

	void GenerateLayoutBindings();

	void GenerateLayoutBindings(EShaderFrequency Stage, const FVulkanShaderSerializedBindings& SerializedBindings);

	void GenerateVertexInputStateInfo();

	void GetDescriptorInfoCounts(uint32& OutCombinedSamplerCount, uint32& OutSamplerBufferCount, uint32& OutUniformBufferCount);

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
	VkDescriptorImageInfo* DescriptorSamplerImageInfoForStage[SF_Compute];

	TArray< VkWriteDescriptorSet* > SRVWriteInfoForStage[SF_Compute];
	bool DirtySRVs[SF_Compute];

#if VULKAN_ENABLE_RHI_DEBUGGING
	struct FDebugInfo
	{
		const FVulkanTextureBase* Textures[SF_Compute][32];
		const FVulkanSamplerState* SamplerStates[SF_Compute][32];
		union
		{
			const FVulkanTextureView* SRVIs[SF_Compute][32];
			const FVulkanBufferView* SRVBs[SF_Compute][32];
		};
		const FVulkanUniformBuffer* UBs[SF_Compute][32];
	};
	FDebugInfo DebugInfo;
#endif

	// Vertex Input
	// Attributes and Bindings are filled in void FVulkanBoundShaderState::GenerateVertexInputStateInfo()
	// InOutMask is used from VertexShader-Header to filter out active attributs
	void ProcessVertexElementForBinding(const FVertexElement& Element);

	void CreateDescriptorWriteInfo(uint32 UniformCombinedSamplerCount, uint32 UniformSamplerBufferCount, uint32 UniformBufferCount);

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

	// Members in Tmp are normally allocated each frame
	// To reduce the allocation, we reuse these array
	struct FTmp
	{
		TArray<VkBuffer> VertexBuffers;
		TArray<VkDeviceSize> VertexOffsets;
	} Tmp;

	// these are the cache pipeline state objects used in this BSS; RefCounts must be manually controlled for the FVulkanPipelines!
	TMap<FVulkanPipelineGraphicsKey, FVulkanPipeline* > PipelineCache;

	struct FDescriptorSetsPair
	{
		uint64 FenceCounter;
		FVulkanDescriptorSets* DescriptorSets;

		FDescriptorSetsPair()
			: FenceCounter(0)
			, DescriptorSets(nullptr)
		{
		}

		~FDescriptorSetsPair();
	};

	struct FDescriptorSetsEntry
	{
		FVulkanCmdBuffer* CmdBuffer;
		TArray<FDescriptorSetsPair> Pairs;

		FDescriptorSetsEntry(FVulkanCmdBuffer* InCmdBuffer)
			: CmdBuffer(InCmdBuffer)
		{
		}
	};
	TArray<FDescriptorSetsEntry*> DescriptorSetsEntries;

	FVulkanDescriptorSets* RequestDescriptorSets(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer);
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
