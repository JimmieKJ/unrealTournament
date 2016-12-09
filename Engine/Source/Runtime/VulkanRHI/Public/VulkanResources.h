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
class FVulkanPendingGfxState;

namespace VulkanRHI
{
	class FDeviceMemoryAllocation;
	class FOldResourceAllocation;
}

enum
{
	NUM_OCCLUSION_QUERIES_PER_POOL = 4096,

	NUM_TIMESTAMP_QUERIES_PER_POOL = 1024,
};


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

	void Create(EShaderFrequency Frequency, const TArray<uint8>& InCode);

	inline const VkShaderModule& GetHandle() const
	{
		return ShaderModule;
	}

protected:
	/** External bindings for this shader. */
	FVulkanCodeHeader CodeHeader;

	VkShaderModule ShaderModule;

	uint32* Code;
	uint32 CodeSize;
	FString DebugName;

	TArray<ANSICHAR> GlslSource;

	FVulkanDevice* Device;

	friend class FVulkanCommandListContext;
	friend class FVulkanPipelineStateCache;
	friend class FVulkanComputeShaderState;
	friend class FVulkanBoundShaderState;
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
		VkFormat* OutStorageFormat = nullptr,
		VkFormat* OutViewFormat = nullptr,
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
	
	// Removes SRGB if requested, used to upload data
	VkFormat StorageFormat;
	// Format for SRVs, render targets
	VkFormat ViewFormat;
	uint32 Width, Height, Depth;
	// UE format
	EPixelFormat PixelFormat;
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
	inline static FVulkanTextureBase* Cast(FRHITexture* Texture)
	{
		check(Texture);
		FVulkanTextureBase* OutTexture = (FVulkanTextureBase*)Texture->GetTextureBaseRHI();
		check(OutTexture);
		return OutTexture;
	}


	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo = FRHIResourceCreateInfo());
	virtual ~FVulkanTextureBase();

	VkImageView CreateRenderTargetView(uint32 MipIndex, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices);

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

	virtual void* GetTextureBaseRHI() override final
	{
		FVulkanTextureBase* Base = static_cast<FVulkanTextureBase*>(this);
		return Base;
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

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}
};

class FVulkanTexture3D : public FRHITexture3D, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTexture3D();

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

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}

	FVulkanTexture2DArray* Texture2DArray;
};

class FVulkanTextureCube : public FRHITextureCube, public FVulkanTextureBase
{
public:
	FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTextureCube();

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

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
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

	virtual void* GetTextureBaseRHI() override final
	{
		return GetReferencedTexture()->GetTextureBaseRHI();
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

class FVulkanQueryPool : public VulkanRHI::FDeviceChild
{
public:
	FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InNumQueries, VkQueryType InQueryType);
	virtual ~FVulkanQueryPool();

	virtual void Destroy();

	void Reset(FVulkanCmdBuffer* CmdBuffer);

	inline VkQueryPool GetHandle() const
	{
		return QueryPool;
	}

protected:
	VkQueryPool QueryPool;
	const uint32 NumQueries;
	const VkQueryType QueryType;

	TArray<uint64> QueryOutput;

	friend class FVulkanDynamicRHI;
};


class FVulkanTimestampPool : public FVulkanQueryPool
{
public:
	FVulkanTimestampPool(FVulkanDevice* InDevice, uint32 InNumQueries);

	void Begin(FVulkanCmdBuffer* CmdBuffer);
	void End(FVulkanCmdBuffer* CmdBuffer);
	bool ReadResults(uint64* OutBeginEnd);

	enum class EState : uint8
	{
		Undefined,
		WrittenBegin,
		WrittenEnd,
		Read,
	};

	struct FInfo
	{
		FVulkanCmdBuffer* CmdBuffer;
		uint64 FenceCounter;
		EState State;

		FInfo()
			: CmdBuffer(nullptr)
			, FenceCounter(0)
			, State(EState::Undefined)
		{
		}
	};
	uint32 BeginCounter;
	double SecondsPerTimestamp;
	double TimeStampsPerSeconds;
	FInfo Infos[NUM_RENDER_BUFFERS];
};

class FVulkanOcclusionQueryPool : public FVulkanQueryPool
{
public:
	FVulkanOcclusionQueryPool(FVulkanDevice* InDevice, uint32 InNumQueries)
		: FVulkanQueryPool(InDevice, InNumQueries, VK_QUERY_TYPE_OCCLUSION)
		, LastBeginIndex(0)
	{
		QueryOutput.SetNum(InNumQueries);
		UsedQueryBits.AddZeroed((InNumQueries + 63) / 64);
		ReadResultsBits.AddZeroed((InNumQueries + 63) / 64);
	}

	bool AcquireQuery(uint32& OutIndex)
	{
		const uint64 AllUsedMask = (uint64)-1;
		for (int32 WordIndex = LastBeginIndex / 64; WordIndex < UsedQueryBits.Num(); ++WordIndex)
		{
			uint64 BeginQueryWord = UsedQueryBits[WordIndex];
			if (BeginQueryWord != AllUsedMask)
			{
				OutIndex = 0;
				while ((BeginQueryWord & 1) == 1)
				{
					++OutIndex;
					BeginQueryWord >>= 1;
				}
				OutIndex += WordIndex * 64;
				uint64 Bit = (uint64)1 << (uint64)OutIndex;
				UsedQueryBits[WordIndex] = UsedQueryBits[WordIndex] | Bit;
				ReadResultsBits[WordIndex] &= ~Bit;
				LastBeginIndex = OutIndex + 1;
				return true;
			}
		}

		// Full!
		return false;
	}

	void ReleaseQuery(uint32 QueryIndex)
	{
		uint32 Word = QueryIndex / 64;
		uint64 Bit = (uint64)1 << (QueryIndex % 64);
		UsedQueryBits[Word] = UsedQueryBits[Word] & ~Bit;
		ReadResultsBits[Word] = ReadResultsBits[Word] & ~Bit;
		if (QueryIndex < LastBeginIndex)
		{
			// Use the lowest word available
			const uint64 AllUsedMask = (uint64)-1;
			if (UsedQueryBits[LastBeginIndex / 64] == AllUsedMask)
			{
				LastBeginIndex = QueryIndex;
			}
		}
	}

	void ResetIfRead(VkCommandBuffer CmdBuffer, uint32 QueryIndex)
	{
		uint32 Word = QueryIndex / 64;
		uint64 Bit = (uint64)1 << (QueryIndex % 64);
		if ((ReadResultsBits[Word] & Bit) == Bit)
		{
			VulkanRHI::vkCmdResetQueryPool(CmdBuffer, QueryPool, QueryIndex, 1);
			ReadResultsBits[Word] = ReadResultsBits[Word] & ~Bit;
		}
	}

	bool GetResults(class FVulkanCommandListContext& Context, class FVulkanRenderQuery* Query, bool bWait, uint64& OutNumPixels);

	bool HasRoom() const
	{
		const uint64 AllUsedMask = (uint64)-1;
		if (LastBeginIndex < UsedQueryBits.Num() * 64)
		{
			check((UsedQueryBits[LastBeginIndex / 64] & AllUsedMask) != AllUsedMask);
			return true;
		}

		return false;
	}

	bool HasExpired() const {return false;}

protected:
	TArray<uint64> UsedQueryBits;
	TArray<uint64> ReadResultsBits;

	// Last potentially free index in the pool
	uint64 LastBeginIndex;

	friend class FVulkanCommandListContext;
};

class FVulkanRenderQuery : public FRHIRenderQuery
{
public:
	FVulkanRenderQuery(FVulkanDevice* Device, ERenderQueryType InQueryType);
	virtual ~FVulkanRenderQuery();

private:
	// Actual index and pool filled in after RHIBeginOcclusionQueryBatch
	FVulkanQueryPool* QueryPool;
	int32 QueryIndex;

	ERenderQueryType QueryType;
	FVulkanCmdBuffer* CurrentCmdBuffer;

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
	friend class FVulkanOcclusionQueryPool;

	void Begin(FVulkanCmdBuffer* CmdBuffer);
	void End(FVulkanCmdBuffer* CmdBuffer);
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
	FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags);
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

struct FVulkanUniformBufferUploader : public VulkanRHI::FDeviceChild
{
public:
	FVulkanUniformBufferUploader(FVulkanDevice* InDevice, uint64 TotalSize);
	~FVulkanUniformBufferUploader();

	uint8* GetCPUMappedPointer()
	{
		return (uint8*)CPUBuffer->GetMappedPointer();
	}

	uint64 AllocateMemory(uint64 Size, uint32 Alignment)
	{
		return CPUBuffer->AllocateMemory(Size, Alignment);
	}

	VkBuffer GetCPUBufferHandle() const
	{
		return CPUBuffer->GetHandle();
	}

	inline uint32 GetCPUBufferOffset() const
	{
		return CPUBuffer->GetBufferOffset();
	}

protected:
	FVulkanRingBuffer* CPUBuffer;
	FVulkanRingBuffer* GPUBuffer;
	friend class FVulkanCommandListContext;
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



class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public VulkanRHI::FDeviceChild
{
public:
	// the potential resources to refer to with the UAV object
	TRefCountPtr<FVulkanStructuredBuffer> SourceStructuredBuffer;

	FVulkanUnorderedAccessView(FVulkanDevice* Device)
		: VulkanRHI::FDeviceChild(Device)
		, MipLevel(0)
		, BufferViewFormat(PF_Unknown)
		, VolatileLockCounter(MAX_uint32)
	{
	}

	void UpdateView();

	// The texture that this UAV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	uint32 MipLevel;

	// The vertex buffer this UAV comes from (can be null)
	TRefCountPtr<FVulkanVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FVulkanBufferView> BufferView;
	EPixelFormat BufferViewFormat;

protected:
	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
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


class FNEWVulkanShaderDescriptorState
{
public:
	FNEWVulkanShaderDescriptorState()
		: WriteDescriptors(nullptr)
		, DirtyMask(0)
		, FullMask(0)
		, NumWrites(0)
	{
	}

	void ResetDirty()
	{
		DirtyMask = 0;
	}

	void MarkAllDirty()
	{
		DirtyMask = FullMask;
	}

	void SetUniformBuffer(uint32 DescriptorIndex, VkBuffer Buffer, VkDeviceSize Offset, VkDeviceSize Range)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
		VkDescriptorBufferInfo* BufferInfo = const_cast<VkDescriptorBufferInfo*>(WriteDescriptors[DescriptorIndex].pBufferInfo);
		check(BufferInfo);
		BufferInfo->buffer = Buffer;
		BufferInfo->offset = Offset;
		BufferInfo->range = Range;
		DirtyMask = DirtyMask | ((uint64)1 << DescriptorIndex);
	}

	void SetSampler(uint32 DescriptorIndex, VkSampler Sampler)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		ImageInfo->sampler = Sampler;
		DirtyMask = DirtyMask | ((uint64)1 << DescriptorIndex);
	}

	void SetImage(uint32 DescriptorIndex, VkImageView ImageView, VkImageLayout Layout)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		ImageInfo->imageView = ImageView;
		ImageInfo->imageLayout = Layout;
		DirtyMask = DirtyMask | ((uint64)1 << DescriptorIndex);
	}

	void SetStorageImage(uint32 DescriptorIndex, VkImageView ImageView, VkImageLayout Layout)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		VkDescriptorImageInfo* ImageInfo = const_cast<VkDescriptorImageInfo*>(WriteDescriptors[DescriptorIndex].pImageInfo);
		check(ImageInfo);
		ImageInfo->imageView = ImageView;
		ImageInfo->imageLayout = Layout;
		DirtyMask = DirtyMask | ((uint64)1 << DescriptorIndex);
	}

	void SetStorageTexelBuffer(uint32 DescriptorIndex, FVulkanBufferView* View)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
		WriteDescriptors[DescriptorIndex].pTexelBufferView = &View->View;
		DirtyMask = DirtyMask | ((uint64)1 << DescriptorIndex);
		BuffersViewReferences[DescriptorIndex] = View;
	}

	void SetUniformTexelBuffer(uint32 DescriptorIndex, FVulkanBufferView* View)
	{
		check(DescriptorIndex < NumWrites);
		check(WriteDescriptors[DescriptorIndex].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
		WriteDescriptors[DescriptorIndex].pTexelBufferView = &View->View;
		DirtyMask = DirtyMask | ((uint64)1 << DescriptorIndex);
		BuffersViewReferences[DescriptorIndex] = View;
	}

	void ClearBufferView(uint32 DescriptorIndex)
	{
		BuffersViewReferences[DescriptorIndex] = nullptr;
	}

	void SetDescriptorSet(VkDescriptorSet DescriptorSet)
	{
		for (uint32 Index = 0; Index < NumWrites; ++Index)
		{
			WriteDescriptors[Index].dstSet = DescriptorSet;
		}
	}

protected:
	VkWriteDescriptorSet* WriteDescriptors;
	uint64 DirtyMask;
	uint64 FullMask;
	uint32 NumWrites;
	TArray<TRefCountPtr<FVulkanBufferView>> BuffersViewReferences;

	void SetupDescriptorWrites(const FNEWVulkanShaderDescriptorInfo& Info, VkWriteDescriptorSet* InWriteDescriptors, VkDescriptorImageInfo* InImageInfo, VkDescriptorBufferInfo* InBufferInfo)
	{
		DebugCopyInfo = Info;
		WriteDescriptors = InWriteDescriptors;
		NumWrites = Info.DescriptorTypes.Num();
		FullMask = ((uint64)1 << (uint64)Info.DescriptorTypes.Num()) - (uint64)1;

		BuffersViewReferences.Empty(NumWrites);
		BuffersViewReferences.AddDefaulted(NumWrites);

		checkf(Info.DescriptorTypes.Num() <= sizeof(FullMask) * 8, TEXT("Out of dirty mask bits! Need %d"), Info.DescriptorTypes.Num());

		MarkAllDirty();

		for (int32 Index = 0; Index < Info.DescriptorTypes.Num(); ++Index)
		{
			InWriteDescriptors->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			InWriteDescriptors->dstBinding = Index;
			InWriteDescriptors->descriptorCount = 1;
			InWriteDescriptors->descriptorType = Info.DescriptorTypes[Index];

			switch (Info.DescriptorTypes[Index])
			{
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
				InWriteDescriptors->pBufferInfo = InBufferInfo++;
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				InWriteDescriptors->pImageInfo = InImageInfo++;
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				break;
			default:
				checkf(0, TEXT("Unsupported descriptor type %d"), Info.DescriptorTypes[Index]);
				break;
			}
			++InWriteDescriptors;
		}
	}

	FNEWVulkanShaderDescriptorInfo DebugCopyInfo;
	friend class FVulkanBoundShaderState;
	friend class FVulkanComputeShaderState;
};

class FNEWVulkanComputeState : public FNEWVulkanShaderDescriptorState
{
public:
	FNEWVulkanComputeState(const FNEWVulkanShaderDescriptorInfo& Info, VkSampler DefaultSampler)
	{
		AllWriteDescriptors.AddZeroed(Info.DescriptorTypes.Num());
		AllBufferInfos.AddZeroed(Info.NumBufferInfos);
		AllImageInfos.AddZeroed(Info.NumImageInfos);

		SetupDescriptorWrites(Info, AllWriteDescriptors.GetData(), AllImageInfos.GetData(), AllBufferInfos.GetData());
	}

protected:
	TArray<VkWriteDescriptorSet> AllWriteDescriptors;
	TArray<VkDescriptorBufferInfo> AllBufferInfos;
	TArray<VkDescriptorImageInfo> AllImageInfos;
};



// Common functionality for shader state (BSS for Gfx and Compute)
class FVulkanShaderState
{
public:
	FVulkanShaderState(FVulkanDevice* InDevice);
	virtual ~FVulkanShaderState();

	inline const FVulkanDescriptorSetsLayout& GetDescriptorSetsLayout() const
	{
		return *Layout;
	}

protected:
	TArray<VkDescriptorImageInfo> DescriptorImageInfo;
	TArray<VkDescriptorBufferInfo> DescriptorBufferInfo;
	TArray<VkWriteDescriptorSet> DescriptorWrites;

	FVulkanDevice* Device;
	mutable VkPipelineLayout PipelineLayout;

	FVulkanDescriptorSetsLayout* Layout;
	FVulkanDescriptorSets* CurrDescriptorSets;
	VkPipeline LastBoundPipeline;

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

	void UpdateDescriptorSetsForStage(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer,
		FVulkanGlobalUniformPool* GlobalUniformPool, VkDescriptorSet DescriptorSet, FVulkanShader* StageShader,
		uint32 RemainingGlobalUniformMask, VkDescriptorBufferInfo* BufferInfo, TArray<uint8>* PackedUniformBuffer,
		FVulkanRingBuffer* RingBuffer, uint8* RingBufferBase, int32& WriteIndex);

	// Querying GetPipelineLayout() generates VkPipelineLayout on the first time
	VkPipelineLayout GetPipelineLayout() const;

	struct FNEWPackedUniformBufferStaging
	{
		TArray<TArray<uint8>> PackedUniformBuffers;
	};

	static inline void InitGlobalUniformsForStage(const FVulkanCodeHeader& CodeHeader, FVulkanShaderState::FNEWPackedUniformBufferStaging& NEWPackedUniformBufferStaging, uint64& NEWPackedUniformBufferStagingDirty)
	{
		NEWPackedUniformBufferStaging.PackedUniformBuffers.AddDefaulted(CodeHeader.NEWPackedGlobalUBSizes.Num());
		for (int32 Index = 0; Index < CodeHeader.NEWPackedGlobalUBSizes.Num(); ++Index)
		{
			NEWPackedUniformBufferStaging.PackedUniformBuffers[Index].AddUninitialized(CodeHeader.NEWPackedGlobalUBSizes[Index]);
		}

		NEWPackedUniformBufferStagingDirty = ((uint64)1 << (uint64)CodeHeader.NEWPackedGlobalUBSizes.Num()) - 1;
	}

	static void GenerateLayoutBindingsForStage(VkShaderStageFlagBits StageFlags, EDescriptorSetStage DescSet, const FVulkanCodeHeader& CodeHeader, FVulkanDescriptorSetsLayout* OutLayout);

	static void UpdatePackedUniformBuffers(FVulkanDevice* Device, const FVulkanCodeHeader& CodeHeader, FNEWPackedUniformBufferStaging& NEWPackedUniformBufferStaging,
		FNEWVulkanShaderDescriptorState& NEWShaderDescriptorState, FVulkanUniformBufferUploader* UniformBufferUploader, uint8* CPURingBufferBase, uint64 RemainingPackedUniformsMask);

	static void SetUniformBufferConstantDataForStage(const FVulkanCodeHeader& CodeHeader, uint32 BindPoint, const TArray<uint8>& ConstantData, FNEWPackedUniformBufferStaging& NEWPackedUniformBufferStaging, uint64& NEWPackedUniformBufferStagingDirty);

	static inline void SetPackedGlobalParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue, FNEWPackedUniformBufferStaging& NEWPackedUniformBufferStaging, uint64& NEWPackedUniformBufferStagingDirty)
	{
		TArray<uint8>& StagingBuffer = NEWPackedUniformBufferStaging.PackedUniformBuffers[BufferIndex];
		check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
		FMemory::Memcpy(StagingBuffer.GetData() + ByteOffset, NewValue, NumBytes);
		NEWPackedUniformBufferStagingDirty = NEWPackedUniformBufferStagingDirty | ((uint64)1 << (uint64)BufferIndex);
}

#if VULKAN_KEEP_CREATE_INFO
	mutable VkPipelineLayoutCreateInfo CreateInfo;
#endif
};

class FVulkanComputeShaderState : public FVulkanShaderState, public FRHIResource
{
public:
	FVulkanComputeShaderState(FVulkanDevice* InDevice, FComputeShaderRHIParamRef InComputeShader);

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer, FVulkanGlobalUniformPool* GlobalUniformPool);

	void BindDescriptorSets(FVulkanCmdBuffer* Cmd);

	void BindPipeline(VkCommandBuffer CmdBuffer, VkPipeline NewPipeline)
	{
		if (LastBoundPipeline != NewPipeline)
		{
			VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, NewPipeline);
			LastBoundPipeline = NewPipeline;
		}
	}

	void ResetState();

	inline void SetShaderParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, NEWPackedUniformBufferStaging, NEWPackedUniformBufferStagingDirty);
	}

	inline void SetTexture(uint32 BindPoint, const FVulkanTextureBase* VulkanTextureBase)
	{
		check(VulkanTextureBase);
		NEWShaderDescriptorState.SetImage(BindPoint, VulkanTextureBase->PartialView->View,
			(VulkanTextureBase->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL);
	}

	inline void SetSamplerState(uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		check(Sampler);
		NEWShaderDescriptorState.SetSampler(BindPoint, Sampler->Sampler);
	}

	inline void SetSRVBufferViewState(uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
		NEWShaderDescriptorState.SetUniformTexelBuffer(BindPoint, View);
	}

	inline void SetUAVBufferViewState(uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
		NEWShaderDescriptorState.SetStorageTexelBuffer(BindPoint, View);
	}

	void SetSRVTextureView(uint32 BindPoint, const FVulkanTextureView& TextureView);

	inline void SetUAVTextureView(uint32 BindPoint, const FVulkanTextureView& TextureView)
	{
		NEWShaderDescriptorState.SetStorageImage(BindPoint, TextureView.View, VK_IMAGE_LAYOUT_GENERAL);
	}

	inline void SetUniformBufferConstantData(uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		SetUniformBufferConstantDataForStage(ComputeShader->CodeHeader, BindPoint, ConstantData, NEWPackedUniformBufferStaging, NEWPackedUniformBufferStagingDirty);
	}

	void SetUniformBuffer(uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer);

	void SetSRV(uint32 TextureIndex, FVulkanShaderResourceView* SRV);
	void SetUAV(uint32 UAVIndex, FVulkanUnorderedAccessView* UAV);

	class FVulkanComputePipeline* PrepareForDispatch(const struct FVulkanComputePipelineState& State);

	bool DoesShaderMatch(FVulkanComputeShader* InShader) const
	{
		return InShader == ComputeShader;
	}

protected:
	TRefCountPtr<FVulkanComputeShader> ComputeShader;
#if VULKAN_ENABLE_PIPELINE_CACHE
	FSHAHash ShaderHash;
#endif

	FNEWPackedUniformBufferStaging NEWPackedUniformBufferStaging;
	uint64 NEWPackedUniformBufferStagingDirty;
	FNEWVulkanShaderDescriptorState NEWShaderDescriptorState;

//#todo-rco
	TMap<FVulkanComputeShader*, TRefCountPtr<FVulkanComputePipeline>> ComputePipelines;

	void CreateDescriptorWriteInfos();

	friend class FVulkanComputePipeline;
	friend class FVulkanCommandListContext;
};

/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FVulkanBoundShaderState : public FRHIBoundShaderState, public FVulkanShaderState
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

	virtual ~FVulkanBoundShaderState();

	// Called when the shader is set by the engine as current pending state
	// After the shader is set, it is expected that all require states are provided/set by the engine
	void ResetState();

	class FVulkanGfxPipeline* PrepareForDraw(class FVulkanRenderPass* RenderPass, const struct FVulkanPipelineGraphicsKey& PipelineKey, uint32 VertexInputKey, const struct FVulkanGfxPipelineState& State);

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
	inline void SetShaderParameter(EShaderFrequency Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanSetShaderParamTime);
#endif
		SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, NEWPackedUniformBufferStaging[Stage], NEWPackedUniformBufferStagingDirty[Stage]);
	}

	inline void SetTexture(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureBase* VulkanTextureBase)
	{
		check(VulkanTextureBase);
		NEWShaderDescriptorState[Stage].SetImage(BindPoint, VulkanTextureBase->PartialView->View,
			(VulkanTextureBase->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL);
	}

	inline void SetSamplerState(EShaderFrequency Stage, uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		check(Sampler);
		NEWShaderDescriptorState[Stage].SetSampler(BindPoint, Sampler->Sampler);
	}

	inline void SetBufferViewState(EShaderFrequency Stage, uint32 BindPoint, FVulkanBufferView* View)
	{
		check(View && (View->Flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) == VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
		NEWShaderDescriptorState[Stage].SetUniformTexelBuffer(BindPoint, View);
	}

	inline void SetTextureView(EShaderFrequency Stage, uint32 BindPoint, const FVulkanTextureView& TextureView)
	{
		NEWShaderDescriptorState[Stage].SetImage(BindPoint, TextureView.View, VK_IMAGE_LAYOUT_GENERAL);
	}

	inline void SetUniformBufferConstantData(EShaderFrequency Stage, uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		FVulkanShader* Shader = GetShaderPtr(Stage);
		SetUniformBufferConstantDataForStage(Shader->CodeHeader, BindPoint, ConstantData, NEWPackedUniformBufferStaging[Stage], NEWPackedUniformBufferStagingDirty[Stage]);
	}

	void SetUniformBuffer(EShaderFrequency Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer);

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

	inline void BindPipeline(VkCommandBuffer CmdBuffer, VkPipeline NewPipeline)
	{
		if (LastBoundPipeline != NewPipeline)
		{
			VulkanRHI::vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, NewPipeline);
			LastBoundPipeline = NewPipeline;
		}
	}

private:
	void GenerateLayoutBindings(EShaderFrequency Stage, const FVulkanCodeHeader& CodeHeader);
	void GenerateVertexInputStateInfo();

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

	// For debugging only
	int32 ID;

	bool bDirtyVertexStreams;

	FNEWVulkanShaderDescriptorState NEWShaderDescriptorState[SF_Compute];

	FNEWPackedUniformBufferStaging NEWPackedUniformBufferStaging[SF_Compute];
	uint64 NEWPackedUniformBufferStagingDirty[SF_Compute];

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

	void CreateDescriptorWriteInfos();

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
	TMap<FVulkanPipelineGraphicsKey, FVulkanGfxPipeline* > PipelineCache;
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
