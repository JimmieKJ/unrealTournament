// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Resources.h: D3D resource RHI definitions.
	=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "D3D12ShaderResources.h"


class FD3D12Resource;
class FD3D12StateCacheBase;
typedef FD3D12StateCacheBase FD3D12StateCache;

class FD3D12PendingResourceBarrier
{
public:

	FD3D12Resource*              Resource;
	D3D12_RESOURCE_STATES        State;
	uint32                       SubResource;
};

class FD3D12CommandListManager;
class FD3D12CommandContext;
class FD3D12CommandListHandle;

class FD3D12CommandAllocator
{
public:
	FD3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);

	// The command allocator is ready to be used (or reset) when the GPU not using it.
	inline bool IsReady() const { return (CommandAllocator != nullptr) && SyncPoint.IsComplete(); }
	inline void SetSyncPoint(const FD3D12SyncPoint& InSyncPoint) { SyncPoint = InSyncPoint; }
	inline void Reset() { check(IsReady()); VERIFYD3D12RESULT(CommandAllocator->Reset()); }
	ID3D12CommandAllocator* GetCommandAllocator() { return CommandAllocator.GetReference(); }
	ID3D12CommandAllocator** GetCommandAllocatorInitReference() { return CommandAllocator.GetInitReference(); }

private:
	void Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);

private:
	TRefCountPtr<ID3D12CommandAllocator> CommandAllocator;
	FD3D12SyncPoint SyncPoint;	// Indicates when the GPU is finished using the command allocator.
};

class FD3D12RefCount
{
public:
	FD3D12RefCount()
	{
	}
	virtual ~FD3D12RefCount()
	{
		check(NumRefs.GetValue() == 0);
	}
	uint32 AddRef() const
	{
		int32 NewValue = NumRefs.Increment();
		check(NewValue > 0);
		return uint32(NewValue);
	}
	uint32 Release() const
	{
		int32 NewValue = NumRefs.Decrement();
		if (NewValue == 0)
		{
			delete this;
		}
		check(NewValue >= 0);
		return uint32(NewValue);
	}
	uint32 GetRefCount() const
	{
		int32 CurrentValue = NumRefs.GetValue();
		check(CurrentValue >= 0);
		return uint32(CurrentValue);
	}
private:
	mutable FThreadSafeCounter NumRefs;
};

/** Find the appropriate depth-stencil typeless DXGI format for the given format. */
inline DXGI_FORMAT FindDepthStencilParentDXGIFormat(DXGI_FORMAT InFormat)
{
	switch (InFormat)
	{
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
#if DEPTH_32_BIT_CONVERSION
		// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;
#endif
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	};
	return InFormat;
}

static uint8 GetPlaneSliceFromViewFormat(DXGI_FORMAT ResourceFormat, DXGI_FORMAT ViewFormat)
{
	// Currently, the only planar resources used are depth-stencil formats
	switch (FindDepthStencilParentDXGIFormat(ResourceFormat))
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		switch (ViewFormat)
		{
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			return 0;
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return 1;
		}
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		switch (ViewFormat)
		{
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			return 0;
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return 1;
		}
		break;
	}

	return 0;
}

static uint8 GetPlaneCount(DXGI_FORMAT Format)
{
	// Currently, the only planar resources used are depth-stencil formats
	// Note there is a D3D12 helper for this, D3D12GetFormatPlaneCount
	switch (FindDepthStencilParentDXGIFormat(Format))
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		return 2;
	default:
		return 1;
	}
}

class FD3D12Heap : public FD3D12RefCount, public FD3D12DeviceChild
{
public:
	FD3D12Heap(FD3D12Device* Parent);
	~FD3D12Heap();

	inline ID3D12Heap* GetHeap() { return Heap.GetReference(); }
	inline void SetHeap(ID3D12Heap* HeapIn) { Heap = HeapIn; }

	void UpdateResidency(FD3D12CommandListHandle& CommandList);

	void BeginTrackingResidency(uint64 Size);

	void Destroy();

	inline FD3D12ResidencyHandle* GetResidencyHandle() { return &ResidencyHandle; }

private:
	TRefCountPtr<ID3D12Heap> Heap;
	FD3D12ResidencyHandle ResidencyHandle;
};

class FD3D12Resource : public FD3D12RefCount, public FD3D12DeviceChild
{
private:
	TRefCountPtr<ID3D12Resource> Resource;
	TRefCountPtr<FD3D12Heap> Heap;

	FD3D12ResidencyHandle ResidencyHandle;

	D3D12_RESOURCE_DESC Desc;
	uint8 PlaneCount;
	uint16 SubresourceCount;
	CResourceState* pResourceState;
	D3D12_RESOURCE_STATES DefaultResourceState;
	D3D12_RESOURCE_STATES ReadableState;
	D3D12_RESOURCE_STATES WritableState;
	bool bRequiresResourceStateTracking;
	bool bDepthStencil;
	D3D12_HEAP_TYPE HeapType;
	D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress;
	FName DebugName;

#if UE_BUILD_DEBUG
	static int64 TotalResourceCount;
	static int64 NoStateTrackingResourceCount;
#endif

public:
	explicit FD3D12Resource(FD3D12Device* ParentDevice, ID3D12Resource* InResource, D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_DESC const& InDesc, FD3D12Heap* InHeap = nullptr, D3D12_HEAP_TYPE InHeapType = D3D12_HEAP_TYPE_DEFAULT)
		: Resource(InResource)
		, Heap(InHeap)
		, Desc(InDesc)
		, PlaneCount(::GetPlaneCount(Desc.Format))
		, SubresourceCount(0)
		, pResourceState(nullptr)
		, DefaultResourceState(D3D12_RESOURCE_STATE_TBD)
		, bRequiresResourceStateTracking(true)
		, bDepthStencil(false)
		, HeapType(InHeapType)
		, GPUVirtualAddress(0)
		, ResidencyHandle()
		, FD3D12DeviceChild(ParentDevice)
	{
#if UE_BUILD_DEBUG
		FPlatformAtomics::_InterlockedIncrement(&TotalResourceCount);
#endif

		if (Resource && Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			GPUVirtualAddress = Resource->GetGPUVirtualAddress();
		}

		InitalizeResourceState(InitialState);
	}

	virtual ~FD3D12Resource();

	operator ID3D12Resource&() { return *Resource; }
	ID3D12Resource* GetResource() const { return Resource.GetReference(); }

	D3D12_RESOURCE_DESC const& GetDesc() const { return Desc; }
	D3D12_HEAP_TYPE GetHeapType() const { return HeapType; }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const { return GPUVirtualAddress; }
	uint16 GetMipLevels() const { return Desc.MipLevels; }
	uint16 GetArraySize() const { return (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : Desc.DepthOrArraySize; }
	uint8 GetPlaneCount() const { return PlaneCount; }
	uint16 GetSubresourceCount() const { return SubresourceCount; }
	CResourceState* GetResourceState()
	{
		// This state is used as the resource's "global" state between command lists. It's only needed for resources that
		// require state tracking.
		return pResourceState;
	}
	D3D12_RESOURCE_STATES GetDefaultResourceState() const { check(!bRequiresResourceStateTracking); return DefaultResourceState; }
	D3D12_RESOURCE_STATES GetWritableState() const { return WritableState; }
	D3D12_RESOURCE_STATES GetReadableState() const { return ReadableState; }
	bool RequiresResourceStateTracking() const { return bRequiresResourceStateTracking; }

	void SetName(const TCHAR* Name)
	{
		DebugName = FName(Name);
		::SetName(Resource, Name);
	}

	FName GetName() const
	{
		return DebugName;
	}

	inline bool IsPlacedResource() const { return Heap.GetReference() != nullptr; }
	inline bool IsDepthStencilResource() const { return bDepthStencil; }

	void StartTrackingForResidency();

	void UpdateResidency(FD3D12CommandListHandle& CommandList);

	inline FD3D12ResidencyHandle* GetResidencyHandle() 
	{
		return (IsPlacedResource()) ? Heap->GetResidencyHandle() : &ResidencyHandle; 
	}

	struct FD3D12ResourceTypeHelper
	{
		FD3D12ResourceTypeHelper(D3D12_RESOURCE_DESC& Desc, D3D12_HEAP_TYPE HeapType) :
			bSRV((Desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0),
			bDSV((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0),
			bRTV((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0),
			bUAV((Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0),
			bWritable(bDSV || bRTV || bUAV),
			bSRVOnly(bSRV && !bWritable),
			bBuffer(Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER),
			bReadBackResource(HeapType == D3D12_HEAP_TYPE_READBACK)
		{}

		const D3D12_RESOURCE_STATES GetOptimalInitialState() const
		{
			if (bSRVOnly)
			{
				return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			}
			else if (bBuffer && !bUAV)
			{
				(bReadBackResource) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
			}
			else if (bWritable) // This things require tracking anyway
			{
				return D3D12_RESOURCE_STATE_COMMON;
			}

			return D3D12_RESOURCE_STATE_COMMON;
		}

		const uint32 bSRV : 1;
		const uint32 bDSV : 1;
		const uint32 bRTV : 1;
		const uint32 bUAV : 1;
		const uint32 bWritable : 1;
		const uint32 bSRVOnly : 1;
		const uint32 bBuffer : 1;
		const uint32 bReadBackResource : 1;
	};

private:
	void InitalizeResourceState(D3D12_RESOURCE_STATES InitialState)
	{
		SubresourceCount = GetMipLevels() * GetArraySize() * GetPlaneCount();
		DetermineResourceStates();

		if (bRequiresResourceStateTracking)
		{
			// Only a few resources (~1%) actually need resource state tracking
			pResourceState = new CResourceState();
			pResourceState->Initialize(SubresourceCount);
			pResourceState->SetResourceState(InitialState);
		}
	}

	void DetermineResourceStates()
	{
		const FD3D12ResourceTypeHelper Type(Desc, HeapType);

		bDepthStencil = Type.bDSV;

		if (Type.bWritable)
		{
			// Determine the resource's write/read states.
			if (Type.bRTV)
			{
				// Note: The resource could also be used as a UAV however we don't store that writable state. UAV's are handled in a separate RHITransitionResources() specially for UAVs so we know the writeable state in that case should be UAV.
				check(!Type.bDSV && !Type.bBuffer);
				WritableState = D3D12_RESOURCE_STATE_RENDER_TARGET;
				ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_CORRUPT;
			}
			else if (Type.bDSV)
			{
				check(!Type.bRTV && !Type.bUAV && !Type.bBuffer);
				WritableState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
				ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_DEPTH_READ;
			}
			else
			{
				check(Type.bUAV && !Type.bRTV && !Type.bDSV);
				WritableState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				ReadableState = Type.bSRV ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_CORRUPT;
			}
		}

		if (Type.bBuffer)
		{
			if (!Type.bWritable)
			{
				// Buffer used for input, like Vertex/Index buffer.
				// Don't bother tracking state for this resource.
#if UE_BUILD_DEBUG
				FPlatformAtomics::_InterlockedIncrement(&NoStateTrackingResourceCount);
#endif
				DefaultResourceState = (HeapType == D3D12_HEAP_TYPE_READBACK) ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_GENERIC_READ;
				bRequiresResourceStateTracking = false;
				return;
			}
		}
		else
		{
			if (Type.bSRVOnly)
			{
				// Texture used only as a SRV.
				// Don't bother tracking state for this resource.
#if UE_BUILD_DEBUG
				FPlatformAtomics::_InterlockedIncrement(&NoStateTrackingResourceCount);
#endif
				DefaultResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				bRequiresResourceStateTracking = false;
				return;
			}
		}
	}
};

// FD3D12ResourceBlockInfo
struct FD3D12ResourceBlockInfo
{
private:
	friend class FD3D12ResourceAllocator;
	friend class FD3D12BuddyAllocator;
	friend class FD3D12LinearAllocator;
	friend class FD3D12BucketAllocator;

	inline uint64 GetOffset() const { return AllocatorValues.AllocationBlockOffset; }
	inline uint32 GetSize() const { return AllocatorValues.Size; }

	// Internal values used by the allocator
	struct AllocationTrackingValues
	{
		AllocationTrackingValues() : AllocationBlockOffset(0), Size(0), UnpaddedSize(0), AllocatorPrivateData(nullptr){};
		uint64 AllocationBlockOffset;
		uint32 Size;
		uint32 UnpaddedSize;

		void* AllocatorPrivateData;
	} AllocatorValues;

public:
	// App facing values
	FD3D12Resource* ResourceHeap;
	void* Address;
	uint64 Offset;// Offset from the start of the resource (including alignment)
	uint64 FrameFence;
	bool IsPlacedResource;

	class FD3D12ResourceAllocator *Allocator;

	FD3D12ResourceBlockInfo() :Allocator(nullptr), ResourceHeap(nullptr), Address(nullptr), Offset(0), FrameFence(0), IsPlacedResource(false){};

	FD3D12ResourceBlockInfo(FD3D12ResourceAllocator* Parent, uint32 InOffset, AllocationTrackingValues TrackingValues) :
		Allocator(Parent)
		, ResourceHeap(nullptr)
		, Address(nullptr)
		, Offset(InOffset)
		, FrameFence(0)
		, AllocatorValues(TrackingValues)
		, IsPlacedResource(false)
	{};

	// Should only be called when the placed resource strategy is in use (i.e. when this owns the resource)
	void Destroy();
};

// FD3D12ResourceLocation
class FD3D12ResourceLocation : public FD3D12RefCount, public FD3D12DeviceChild
{
protected:
	TRefCountPtr<FD3D12Resource> Resource;
	bool IsSubAllocatedFastAlloc;
	D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress;
	uint64 Offset;
	uint64 Padding;
	FD3D12ResourceBlockInfo* BlockInfo;
	uint64 EffectiveBufferSize;	// This member is used to track the size of the buffer the app wanted because we may have increased the size behind the scene (GetDesc11() will return the wrong size).

	friend class FD3D12DynamicHeapAllocator;
	friend class FD3D12DefaultBufferPool;
	friend class FD3D12FastAllocator;
	friend class FD3D12DefaultBufferAllocator;

	void InternalReleaseResource();

public:
	FD3D12ResourceLocation()
		: Resource(nullptr)
		, Offset(0)
		, IsSubAllocatedFastAlloc(false)
		, Padding(0)
		, EffectiveBufferSize(0)
		, BlockInfo(nullptr)
		, FD3D12DeviceChild(nullptr)
	{
		UpdateGPUVirtualAddress();
	}

	FD3D12ResourceLocation(FD3D12Device* InParent)
		: Resource(nullptr)
		, Offset(0)
		, IsSubAllocatedFastAlloc(false)
		, Padding(0)
		, EffectiveBufferSize(0)
		, BlockInfo(nullptr)
		, FD3D12DeviceChild(InParent)
	{
		UpdateGPUVirtualAddress();
	}

	FD3D12ResourceLocation(FD3D12Device* InParent, const uint64 &InEffectiveSize)
		: Resource(nullptr)
		, IsSubAllocatedFastAlloc(false)
		, Offset(0)
		, Padding(0)
		, EffectiveBufferSize(InEffectiveSize)
		, BlockInfo(nullptr)
		, FD3D12DeviceChild(InParent)
	{
		UpdateGPUVirtualAddress();
	}

	FD3D12ResourceLocation(FD3D12Device* InParent, FD3D12Resource* InResource, uint64 InOffset = 0)
		: Resource(InResource)
		, IsSubAllocatedFastAlloc(false)
		, Offset(InOffset)
		, Padding(0)
		, BlockInfo(nullptr)
		, FD3D12DeviceChild(InParent)
	{
		check(nullptr == BlockInfo);
		BlockInfo = nullptr;

		if (InResource)
		{
			D3D12_RESOURCE_DESC const& desc = InResource->GetDesc();
			EffectiveBufferSize = (uint32)desc.Width;
		}
		else
		{
			EffectiveBufferSize = 0;
		}

		UpdateGPUVirtualAddress();
	}

	~FD3D12ResourceLocation();

	void SetAsFastAllocatedSubresource()
	{
		IsSubAllocatedFastAlloc = true;
	}

	void SetBlockInfo(FD3D12ResourceBlockInfo* InBlockInfo, FD3D12StateCache* StateCache)
	{
		const FD3D12Resource* OldResource = Resource;
		FD3D12Resource* NewResource = InBlockInfo->ResourceHeap;

		// There is an edge case in which a ResourceLocation can get allocated
		// as a stand-alone allocation and then later on get pooled. In that case
		// we must defer delete the stand alone allocation.
		if (BlockInfo == nullptr && OldResource != nullptr)
		{
			InternalReleaseResource();
		}

		BlockInfo = InBlockInfo;

		uint64 OldOffset = Offset;
		Offset = InBlockInfo->Offset;

		if (OldResource != NewResource)
		{
			Resource = NewResource;
			if (OldResource)
			{
				if (StateCache != nullptr)
				{
					UpdateStateCache(*StateCache);
				}
			}
		}
		else
		{
			if (OldOffset != Offset)
			{
				if (StateCache != nullptr)
				{
					UpdateStateCache(*StateCache);
				}
			}
		}

		UpdateGPUVirtualAddress();
	}

	void SetBlockInfoNoUpdate(FD3D12ResourceBlockInfo* InBlockInfo)
	{
		Resource = InBlockInfo->ResourceHeap;
		BlockInfo = InBlockInfo;
		Offset = InBlockInfo->Offset;

		UpdateGPUVirtualAddress();
	}

	FD3D12ResourceBlockInfo* GetBlockInfo()
	{
		return BlockInfo;
	}

	FD3D12Resource* GetResource() const
	{
		return Resource;
	}

	uint64 GetOffset() const
	{
		return Offset + Padding;
	}

	void SetPadding(uint64 InPadding)
	{
		Padding = InPadding;

		UpdateGPUVirtualAddress();
	}

	void UpdateGPUVirtualAddress()
	{
		// Note: Making this public because currently there are places that update FD3D12ResourceLocation member variables and we need to force an update of the GPU VA.
		// This will be fixed later (bigger refactor).
		if (Resource)
		{
			GPUVirtualAddress = Resource->GetGPUVirtualAddress() + GetOffset();
		}
		else
		{
			GPUVirtualAddress = 0;
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
	{
		return GPUVirtualAddress;
	}

	void SetFromD3DResource(FD3D12Resource* InResource, uint64 InOffset, uint32 InEffectiveBufferSize);

	void SetEffectiveBufferSize(uint64 InSize)
	{
		EffectiveBufferSize = InSize;
	}

	uint64 GetEffectiveBufferSize() const
	{
		return EffectiveBufferSize;
	}

	void Clear();

	void ClearNoUpdate();

	void UpdateStateCache(FD3D12StateCache& StateCache);
	void UpdateDefaultStateCache();
};

/** Information about a D3D resource that is currently locked. */
struct FD3D12LockedData
{
	TRefCountPtr<FD3D12Resource> StagingResource;
	TRefCountPtr<FD3D12Resource> UploadHeapResource;
	TRefCountPtr<FD3D12ResourceLocation> UploadHeapLocation;
	uint32 Pitch;
	uint32 DepthPitch;

	// constructor
	FD3D12LockedData()
		: bAllocDataWasUsed(false)
	{
	}

	// 16 byte alignment for best performance  (can be 30x faster than unaligned)
	void AllocData(uint32 Size)
	{
		Data = (uint8*)FMemory::Malloc(Size, 16);
		bAllocDataWasUsed = true;
	}

	// Some driver might return aligned memory so we don't enforce the alignment
	void SetData(void* InData)
	{
		check(!bAllocDataWasUsed); Data = (uint8*)InData;
	}

	uint8* GetData() const
	{
		return Data;
	}

	// only call if AllocData() was used
	void FreeData()
	{
		check(bAllocDataWasUsed);
		FMemory::Free(Data);
		Data = 0;
	}

private:
	uint8* Data;
	// then FreeData
	bool bAllocDataWasUsed;
};

/**
* Class for managing dynamic buffers.
*/
class FD3D12DynamicBuffer : public FRenderResource, public FRHIResource, public FD3D12DeviceChild
{
public:
	/** Initialization constructor. */
	FD3D12DynamicBuffer(FD3D12Device* InParent, class FD3D12FastAllocator& Allocator);
	/** Destructor. */
	~FD3D12DynamicBuffer();

	/** Locks the buffer returning at least Size bytes. */
	void* Lock(uint32 Size);
	/** Unlocks the buffer returning the underlying D3D12 buffer to use as a resource. */
	FD3D12ResourceLocation* Unlock();

	// Begin FRenderResource interface.
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	// End FRenderResource interface.

	void ReleaseResourceLocation() { ResourceLocation = nullptr; }

private:
	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation;
	class FD3D12FastAllocator& FastAllocator;
};

class FD3D12DeferredDeletionQueue : public FD3D12DeviceChild
{
	typedef TPair<FD3D12Resource*, uint64> FencedObjectType;
	FThreadsafeQueue<FencedObjectType> DeferredReleaseQueue;

public:
	void EnqueueResource(FD3D12Resource* pResource);

	bool ReleaseResources(bool DeleteImmediately = false);

	void Clear()
	{
		ReleaseResources(true);
	}

	FD3D12DeferredDeletionQueue(FD3D12Device* InParent);
	~FD3D12DeferredDeletionQueue();

	class FD3D12AsyncDeletionWorker : public FD3D12DeviceChild, public FNonAbandonableTask
	{
	public:
		FD3D12AsyncDeletionWorker(FD3D12Device* Device, FThreadsafeQueue<FencedObjectType>* DeletionQueue);

		void DoWork();

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12AsyncDeletionWorker, STATGROUP_ThreadPoolAsyncTasks);
		}

	private:
		TQueue<FencedObjectType> Queue;
	};

private:
	TQueue<FAsyncTask<FD3D12AsyncDeletionWorker>*> DeleteTasks;

};

// Root parameter keys grouped by visibility.
enum ERootParameterKeys
{
	PS_SRVs,
	PS_CBVs,
	PS_Samplers,
	VS_SRVs,
	VS_CBVs,
	VS_Samplers,
	GS_SRVs,
	GS_CBVs,
	GS_Samplers,
	HS_SRVs,
	HS_CBVs,
	HS_Samplers,
	DS_SRVs,
	DS_CBVs,
	DS_Samplers,
	ALL_SRVs,
	ALL_CBVs,
	ALL_Samplers,
	ALL_UAVs,
	RPK_RootParameterKeyCount,
};

class FD3D12RootSigntatureDesc; // forward-declare
class FD3D12RootSignature : public FD3D12DeviceChild
{
private:
	// Struct for all the useful info we want per shader stage.
	struct ShaderStage
	{
		ShaderStage()
		: MaxCBVCount(0u)
		, MaxSRVCount(0u)
		, MaxSamplerCount(0u)
		, MaxUAVCount(0u)
		, bVisible(false)
		{
		}

		uint8 MaxCBVCount;
		uint8 MaxSRVCount;
		uint8 MaxSamplerCount;
		uint8 MaxUAVCount;
		bool bVisible;
	};

public:
	explicit FD3D12RootSignature(FD3D12Device* InParent)
		: FD3D12DeviceChild(InParent)
	{}
	explicit FD3D12RootSignature(FD3D12Device* InParent, const FD3D12QuantizedBoundShaderState& InQBSS)
		: FD3D12DeviceChild(InParent)
	{
		Init(InQBSS);
	}
	explicit FD3D12RootSignature(FD3D12Device* InParent, const D3D12_ROOT_SIGNATURE_DESC& InDesc)
		: FD3D12DeviceChild(InParent)
	{
		Init(InDesc);
	}
	explicit FD3D12RootSignature(FD3D12Device* InParent, ID3DBlob* const InBlob)
		: FD3D12DeviceChild(InParent)
	{
		Init(InBlob);
	}

	void Init(const FD3D12QuantizedBoundShaderState& InQBSS);
	void Init(const D3D12_ROOT_SIGNATURE_DESC& InDesc);
	void Init(ID3DBlob* const InBlob);

	ID3D12RootSignature* GetRootSignature() const { return RootSignature.GetReference(); }
	ID3DBlob* GetRootSignatureBlob() const { return RootSignatureBlob.GetReference(); }

	inline uint32 SamplerRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_Samplers];
		case SF_Pixel: return BindSlotMap[PS_Samplers];
		case SF_Geometry: return BindSlotMap[GS_Samplers];
		case SF_Hull: return BindSlotMap[HS_Samplers];
		case SF_Domain: return BindSlotMap[DS_Samplers];
		case SF_Compute: return BindSlotMap[ALL_Samplers];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 SRVRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_SRVs];
		case SF_Pixel: return BindSlotMap[PS_SRVs];
		case SF_Geometry: return BindSlotMap[GS_SRVs];
		case SF_Hull: return BindSlotMap[HS_SRVs];
		case SF_Domain: return BindSlotMap[DS_SRVs];
		case SF_Compute: return BindSlotMap[ALL_SRVs];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 CBVRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		switch (ShaderStage)
		{
		case SF_Vertex: return BindSlotMap[VS_CBVs];
		case SF_Pixel: return BindSlotMap[PS_CBVs];
		case SF_Geometry: return BindSlotMap[GS_CBVs];
		case SF_Hull: return BindSlotMap[HS_CBVs];
		case SF_Domain: return BindSlotMap[DS_CBVs];
		case SF_Compute: return BindSlotMap[ALL_CBVs];

		default: check(false);
			return UINT_MAX;
		}
	}

	inline uint32 UAVRDTBindSlot(EShaderFrequency ShaderStage) const
	{
		check(ShaderStage == SF_Pixel || ShaderStage == SF_Compute);
		return BindSlotMap[ALL_UAVs];
	}

	inline bool HasUAVs() const { return bHasUAVs; }
	inline bool HasSRVs() const { return bHasSRVs; }
	inline bool HasCBVs() const { return bHasCBVs; }
	inline bool HasSamplers() const { return bHasSamplers; }
	inline bool HasVS() const { return Stage[SF_Vertex].bVisible; }
	inline bool HasHS() const { return Stage[SF_Hull].bVisible; }
	inline bool HasDS() const { return Stage[SF_Domain].bVisible; }
	inline bool HasGS() const { return Stage[SF_Geometry].bVisible; }
	inline bool HasPS() const { return Stage[SF_Pixel].bVisible; }
	inline bool HasCS() const { return Stage[SF_Compute].bVisible; }	// Root signatures can be used for Graphics and/or Compute because they exist in separate bind spaces.
	inline uint32 MaxSamplerCount(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxSamplerCount; }
	inline uint32 MaxSRVCount(uint32 ShaderStage) const { check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxSRVCount; }
	inline uint32 MaxCBVCount(uint32 ShaderStage) const{ check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxCBVCount; }
	inline uint32 MaxUAVCount(uint32 ShaderStage) const{ check(ShaderStage != SF_NumFrequencies); return Stage[ShaderStage].MaxUAVCount; }

private:
	void AnalyzeSignature(const D3D12_ROOT_SIGNATURE_DESC& Desc);
	inline bool HasVisibility(const D3D12_SHADER_VISIBILITY& ParameterVisibility, const D3D12_SHADER_VISIBILITY& Visibility) const
	{
		return ParameterVisibility == D3D12_SHADER_VISIBILITY_ALL || ParameterVisibility == Visibility;
	}

	inline void SetSamplersRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_Samplers]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_Samplers]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_Samplers]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_Samplers]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_Samplers]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_Samplers]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasSamplers = true;
	}

	inline void SetSRVRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_SRVs]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_SRVs]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_SRVs]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_SRVs]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_SRVs]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_SRVs]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasSRVs = true;
	}

	inline void SetCBVRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		uint8* pBindSlot = nullptr;
		switch (SF)
		{
		case SF_Vertex: pBindSlot = &BindSlotMap[VS_CBVs]; break;
		case SF_Pixel: pBindSlot = &BindSlotMap[PS_CBVs]; break;
		case SF_Geometry: pBindSlot = &BindSlotMap[GS_CBVs]; break;
		case SF_Hull: pBindSlot = &BindSlotMap[HS_CBVs]; break;
		case SF_Domain: pBindSlot = &BindSlotMap[DS_CBVs]; break;

		case SF_Compute:
		case SF_NumFrequencies: pBindSlot = &BindSlotMap[ALL_CBVs]; break;

		default: check(false);
			return;
		}

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasCBVs = true;
	}

	inline void SetUAVRDTBindSlot(EShaderFrequency SF, uint8 RootParameterIndex)
	{
		check(SF == SF_Pixel || SF == SF_Compute || SF == SF_NumFrequencies);
		uint8* pBindSlot = &BindSlotMap[ALL_UAVs];

		check(*pBindSlot == 0xFF);
		*pBindSlot = RootParameterIndex;

		bHasUAVs = true;
	}

	inline void SetMaxSamplerCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxSamplerCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxSamplerCount = Count;
		}
	}

	inline void SetMaxSRVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxSRVCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxSRVCount = Count;
		}
	}

	inline void SetMaxCBVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxCBVCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxCBVCount = Count;
		}
	}

	inline void SetMaxUAVCount(EShaderFrequency SF, uint8 Count)
	{
		if (SF == SF_NumFrequencies)
		{
			// Update all counts for all stages.
			for (uint32 s = SF_Vertex; s <= SF_Compute; s++)
			{
				Stage[s].MaxUAVCount = Count;
			}
		}
		else
		{
			Stage[SF].MaxUAVCount = Count;
		}
	}

	TRefCountPtr<ID3D12RootSignature> RootSignature;
	uint8 BindSlotMap[RPK_RootParameterKeyCount];	// This map uses an enum as a key to lookup the root parameter index
	ShaderStage Stage[SF_NumFrequencies];
	bool bHasUAVs;
	bool bHasSRVs;
	bool bHasCBVs;
	bool bHasSamplers;
	TRefCountPtr<ID3DBlob> RootSignatureBlob;
};

class FD3D12RootSignatureManager : public FD3D12DeviceChild
{
public:
	explicit FD3D12RootSignatureManager(FD3D12Device* InParent)
		: FD3D12DeviceChild(InParent)
	{
	}
	~FD3D12RootSignatureManager();
	FD3D12RootSignature* GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS);

private:
	FCriticalSection CS;
	FD3D12RootSignature* CreateRootSignature(const FD3D12QuantizedBoundShaderState& QBSS);

	TMap<FD3D12QuantizedBoundShaderState, FD3D12RootSignature*> RootSignatureMap;
};

template <>
struct TTypeTraits<D3D12_INPUT_ELEMENT_DESC> : public TTypeTraitsBase < D3D12_INPUT_ELEMENT_DESC >
{
	enum { IsBytewiseComparable = true };
};

/** Convenience typedef: preallocated array of D3D12 input element descriptions. */
typedef TArray<D3D12_INPUT_ELEMENT_DESC, TFixedAllocator<MaxVertexElementCount> > FD3D12VertexElements;

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FD3D12VertexDeclaration : public FRHIVertexDeclaration
{
public:
	/** Elements of the vertex declaration. */
	FD3D12VertexElements VertexElements;

	/** Initialization constructor. */
	explicit FD3D12VertexDeclaration(const FD3D12VertexElements& InElements)
		: VertexElements(InElements)
	{
	}
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
class FD3D12VertexShader : public FRHIVertexShader
{
public:
	enum { StaticFrequency = SF_Vertex };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The vertex shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	// TEMP remove with removal of bound shader state
	int32 Offset;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12GeometryShader : public FRHIGeometryShader
{
public:
	enum { StaticFrequency = SF_Geometry };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	/** The shader's stream output description. */
	D3D12_STREAM_OUTPUT_DESC StreamOutput;
	D3D12_SO_DECLARATION_ENTRY* pStreamOutEntries;
	uint32* pStreamOutStrides;

	bool bShaderNeedsStreamOutput;

	FShaderCodePackedResourceCounts ResourceCounts;

	FD3D12GeometryShader()
		: bShaderNeedsStreamOutput(false)
		, pStreamOutEntries(nullptr)
		, pStreamOutStrides(nullptr)
	{
		FMemory::Memzero(&StreamOutput, sizeof(StreamOutput));
	}

	virtual ~FD3D12GeometryShader()
	{
		if (pStreamOutEntries)
		{
			delete[] pStreamOutEntries;
			pStreamOutEntries = nullptr;
		}

		if (pStreamOutStrides)
		{
			delete[] pStreamOutStrides;
			pStreamOutStrides = nullptr;
		}
	}
};

class FD3D12HullShader : public FRHIHullShader
{
public:
	enum { StaticFrequency = SF_Hull };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12DomainShader : public FRHIDomainShader
{
public:
	enum { StaticFrequency = SF_Domain };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	FD3D12ShaderResourceTable ShaderResourceTable;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12PixelShader : public FRHIPixelShader
{
public:
	enum { StaticFrequency = SF_Pixel };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FD3D12ShaderResourceTable ShaderResourceTable;

	FShaderCodePackedResourceCounts ResourceCounts;
};

class FD3D12ComputeShader : public FRHIComputeShader
{
public:
	enum { StaticFrequency = SF_Compute };

	/** The shader's bytecode. */
	FD3D12ShaderBytecode ShaderBytecode;

	/** The shader's bytecode, with custom data in the last byte. */
	TArray<uint8> Code;

	FD3D12ShaderResourceTable ShaderResourceTable;

	FShaderCodePackedResourceCounts ResourceCounts;
	FD3D12RootSignature* pRootSignature;
};

/**
* Combined shader state and vertex definition for rendering geometry.
* Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
*/
class FD3D12BoundShaderState : public FRHIBoundShaderState
{
public:

#if D3D12_SUPPORTS_PARALLEL_RHI_EXECUTE
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
#else
	FCachedBoundShaderStateLink CacheLink;
#endif

	D3D12_INPUT_LAYOUT_DESC InputLayout;

	bool bShaderNeedsGlobalConstantBuffer[SF_NumFrequencies];
	uint64 UniqueID;
	FD3D12RootSignature* pRootSignature;

	/** Initialization constructor. */
	FD3D12BoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI,
		FD3D12Device* InDevice
		);

	virtual ~FD3D12BoundShaderState();

	/**
	* Get the shader for the given frequency.
	*/
	FORCEINLINE FD3D12VertexShader*   GetVertexShader() const   { return (FD3D12VertexShader*)CacheLink.GetVertexShader(); }
	FORCEINLINE FD3D12PixelShader*    GetPixelShader() const    { return (FD3D12PixelShader*)CacheLink.GetPixelShader(); }
	FORCEINLINE FD3D12HullShader*     GetHullShader() const     { return (FD3D12HullShader*)CacheLink.GetHullShader(); }
	FORCEINLINE FD3D12DomainShader*   GetDomainShader() const   { return (FD3D12DomainShader*)CacheLink.GetDomainShader(); }
	FORCEINLINE FD3D12GeometryShader* GetGeometryShader() const { return (FD3D12GeometryShader*)CacheLink.GetGeometryShader(); }
};

/** The base class of resources that may be bound as shader resources. */
class FD3D12BaseShaderResource : public FD3D12DeviceChild, public IRefCountedObject
{
public:
	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation;
	uint32 BufferAlignment;

public:
	FD3D12BaseShaderResource(FD3D12ResourceLocation *InResourceLocation, FD3D12Device* InParent) :
		BufferAlignment(0),
		FD3D12DeviceChild(InParent)
	{
		ResourceLocation = InResourceLocation;
	}

	FD3D12BaseShaderResource(FD3D12Resource *InResource, FD3D12Device* InParent) :
		BufferAlignment(0),
		FD3D12DeviceChild(InParent)
	{
		ResourceLocation = new FD3D12ResourceLocation(GetParentDevice(), InResource);
	}
};

enum ViewSubresourceSubsetFlags
{
	ViewSubresourceSubsetFlags_None = 0x0,
	ViewSubresourceSubsetFlags_DepthOnlyDsv = 0x1,
	ViewSubresourceSubsetFlags_StencilOnlyDsv = 0x2,
	ViewSubresourceSubsetFlags_DepthAndStencilDsv = (ViewSubresourceSubsetFlags_DepthOnlyDsv | ViewSubresourceSubsetFlags_StencilOnlyDsv),
};

/** Class to track subresources in a view */
struct CBufferView {};
class CSubresourceSubset
{
public:
	CSubresourceSubset() {}
	inline explicit CSubresourceSubset(const CBufferView&) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_EndMip(1),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
	}
	inline explicit CSubresourceSubset(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc, DXGI_FORMAT ResourceFormat) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_EndMip(1),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Shader Resource View"); break;

		case (D3D12_SRV_DIMENSION_BUFFER) :
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture1D.MipLevels);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture1DArray.MipLevels);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture2D.MipLevels);
			m_BeginPlane = uint8(Desc.Texture2D.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2D.PlaneSlice + 1);
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture2DArray.MipLevels);
			m_BeginPlane = uint8(Desc.Texture2DArray.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2DArray.PlaneSlice + 1);
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2DMS) :
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY) :
			m_BeginArray = uint16(Desc.Texture2DMSArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DMSArray.ArraySize);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURE3D) :
			m_EndArray = uint16(-1); //all slices
			m_BeginMip = uint8(Desc.Texture3D.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.Texture3D.MipLevels);
			break;

		case (D3D12_SRV_DIMENSION_TEXTURECUBE) :
			m_BeginMip = uint8(Desc.TextureCube.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.TextureCube.MipLevels);
			m_BeginArray = 0;
			m_EndArray = 6;
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;

		case (D3D12_SRV_DIMENSION_TEXTURECUBEARRAY) :
			m_BeginArray = uint16(Desc.TextureCubeArray.First2DArrayFace);
			m_EndArray = uint16(m_BeginArray + Desc.TextureCubeArray.NumCubes * 6);
			m_BeginMip = uint8(Desc.TextureCubeArray.MostDetailedMip);
			m_EndMip = uint8(m_BeginMip + Desc.TextureCubeArray.MipLevels);
			m_BeginPlane = GetPlaneSliceFromViewFormat(ResourceFormat, Desc.Format);
			m_EndPlane = m_BeginPlane + 1;
			break;
		}
	}
	inline explicit CSubresourceSubset(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Unordered Access View"); break;

		case (D3D12_UAV_DIMENSION_BUFFER) : break;

		case (D3D12_UAV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MipSlice);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MipSlice);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2D.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2D.PlaneSlice + 1);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2DArray.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2DArray.PlaneSlice + 1);
			break;

		case (D3D12_UAV_DIMENSION_TEXTURE3D) :
			m_BeginArray = uint16(Desc.Texture3D.FirstWSlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture3D.WSize);
			m_BeginMip = uint8(Desc.Texture3D.MipSlice);
			break;
		}

		m_EndMip = m_BeginMip + 1;
	}
	inline explicit CSubresourceSubset(const D3D12_RENDER_TARGET_VIEW_DESC& Desc) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_BeginPlane(0),
		m_EndPlane(1)
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Render Target View"); break;

		case (D3D12_RTV_DIMENSION_BUFFER) : break;

		case (D3D12_RTV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MipSlice);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MipSlice);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2D.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2D.PlaneSlice + 1);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE2DMS) : break;

		case (D3D12_RTV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MipSlice);
			m_BeginPlane = uint8(Desc.Texture2DArray.PlaneSlice);
			m_EndPlane = uint8(Desc.Texture2DArray.PlaneSlice + 1);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY) :
			m_BeginArray = uint16(Desc.Texture2DMSArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DMSArray.ArraySize);
			break;

		case (D3D12_RTV_DIMENSION_TEXTURE3D) :
			m_BeginArray = uint16(Desc.Texture3D.FirstWSlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture3D.WSize);
			m_BeginMip = uint8(Desc.Texture3D.MipSlice);
			break;
		}

		m_EndMip = m_BeginMip + 1;
	}

	inline explicit CSubresourceSubset(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags Flags) :
		m_BeginArray(0),
		m_EndArray(1),
		m_BeginMip(0),
		m_BeginPlane(0),
		m_EndPlane(GetPlaneCount(ResourceFormat))
	{
		switch (Desc.ViewDimension)
		{
		default: ASSUME(0 && "Corrupt Resource Type on Depth Stencil View"); break;

		case (D3D12_DSV_DIMENSION_TEXTURE1D) :
			m_BeginMip = uint8(Desc.Texture1D.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE1DARRAY) :
			m_BeginArray = uint16(Desc.Texture1DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture1DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture1DArray.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE2D) :
			m_BeginMip = uint8(Desc.Texture2D.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE2DMS) : break;

		case (D3D12_DSV_DIMENSION_TEXTURE2DARRAY) :
			m_BeginArray = uint16(Desc.Texture2DArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DArray.ArraySize);
			m_BeginMip = uint8(Desc.Texture2DArray.MipSlice);
			break;

		case (D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY) :
			m_BeginArray = uint16(Desc.Texture2DMSArray.FirstArraySlice);
			m_EndArray = uint16(m_BeginArray + Desc.Texture2DMSArray.ArraySize);
			break;
		}

		m_EndMip = m_BeginMip + 1;

		if (m_EndPlane == 2)
		{
			if ((Flags & ViewSubresourceSubsetFlags_DepthAndStencilDsv) != ViewSubresourceSubsetFlags_DepthAndStencilDsv)
			{
				if (Flags & ViewSubresourceSubsetFlags_DepthOnlyDsv)
				{
					m_BeginPlane = 0;
					m_EndPlane = 1;
				}
				else if (Flags & ViewSubresourceSubsetFlags_StencilOnlyDsv)
				{
					m_BeginPlane = 1;
					m_EndPlane = 2;
				}
			}
		}
	}

	__forceinline bool DoesNotOverlap(const CSubresourceSubset& other) const
	{
		if (m_EndArray <= other.m_BeginArray)
		{
			return true;
		}

		if (other.m_EndArray <= m_BeginArray)
		{
			return true;
		}

		if (m_EndMip <= other.m_BeginMip)
		{
			return true;
		}

		if (other.m_EndMip <= m_BeginMip)
		{
			return true;
		}

		if (m_EndPlane <= other.m_BeginPlane)
		{
			return true;
		}

		if (other.m_EndPlane <= m_BeginPlane)
		{
			return true;
		}

		return false;
	}

protected:
	uint16 m_BeginArray; // Also used to store Tex3D slices.
	uint16 m_EndArray; // End - Begin == Array Slices
	uint8 m_BeginMip;
	uint8 m_EndMip; // End - Begin == Mip Levels
	uint8 m_BeginPlane;
	uint8 m_EndPlane;
};

template <typename TDesc>
class FD3D12View;

class CViewSubresourceSubset : public CSubresourceSubset
{
	friend class FD3D12View < D3D12_SHADER_RESOURCE_VIEW_DESC >;
	friend class FD3D12View < D3D12_RENDER_TARGET_VIEW_DESC >;
	friend class FD3D12View < D3D12_DEPTH_STENCIL_VIEW_DESC >;
	friend class FD3D12View < D3D12_UNORDERED_ACCESS_VIEW_DESC >;

public:
	CViewSubresourceSubset() {}
	inline explicit CViewSubresourceSubset(const CBufferView&)
		: CSubresourceSubset(CBufferView())
		, m_MipLevels(1)
		, m_ArraySlices(1)
		, m_MostDetailedMip(0)
		, m_ViewArraySize(1)
	{
	}

	inline CViewSubresourceSubset(uint32 Subresource, uint8 MipLevels, uint16 ArraySize, uint8 PlaneCount)
		: m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(PlaneCount)
	{
		if (Subresource < uint32(MipLevels) * uint32(ArraySize))
		{
			m_BeginArray = Subresource / MipLevels;
			m_EndArray = m_BeginArray + 1;
			m_BeginMip = Subresource % MipLevels;
			m_EndMip = m_EndArray + 1;
		}
		else
		{
			m_BeginArray = 0;
			m_BeginMip = 0;
			if (Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				m_EndArray = ArraySize;
				m_EndMip = MipLevels;
			}
			else
			{
				m_EndArray = 0;
				m_EndMip = 0;
			}
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
	}

	inline CViewSubresourceSubset(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags /*Flags*/)
		: CSubresourceSubset(Desc, ResourceFormat)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		if (Desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D)
		{
			check(m_BeginArray == 0);
			m_EndArray = 1;
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	inline CViewSubresourceSubset(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags /*Flags*/)
		: CSubresourceSubset(Desc)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		if (Desc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D)
		{
			m_BeginArray = 0;
			m_EndArray = 1;
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	inline CViewSubresourceSubset(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags Flags)
		: CSubresourceSubset(Desc, ResourceFormat, Flags)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	inline CViewSubresourceSubset(const D3D12_RENDER_TARGET_VIEW_DESC& Desc, uint8 MipLevels, uint16 ArraySize, DXGI_FORMAT ResourceFormat, ViewSubresourceSubsetFlags /*Flags*/)
		: CSubresourceSubset(Desc)
		, m_MipLevels(MipLevels)
		, m_ArraySlices(ArraySize)
		, m_PlaneCount(GetPlaneCount(ResourceFormat))
	{
		if (Desc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE3D)
		{
			m_BeginArray = 0;
			m_EndArray = 1;
		}
		m_MostDetailedMip = m_BeginMip;
		m_ViewArraySize = m_EndArray - m_BeginArray;
		Reduce();
	}

	template<typename T>
	static CViewSubresourceSubset FromView(const T* pView)
	{
		return CViewSubresourceSubset(
			pView->Desc(),
			static_cast<uint8>(pView->GetResource()->GetMipLevels()),
			static_cast<uint16>(pView->GetResource()->GetArraySize()),
			static_cast<uint8>(pView->GetResource()->GetPlaneCount())
			);
	}

public:
	class CViewSubresourceIterator;

public:
	CViewSubresourceIterator begin() const;
	CViewSubresourceIterator end() const;
	bool IsWholeResource() const;
	uint32 ArraySize() const;

	uint8 MostDetailedMip() const;
	uint16 ViewArraySize() const;

	uint32 MinSubresource() const;
	uint32 MaxSubresource() const;

private:
	// Strictly for performance, allows coalescing contiguous subresource ranges into a single range
	inline void Reduce()
	{
		if (m_BeginMip == 0
			&& m_EndMip == m_MipLevels
			&& m_BeginArray == 0
			&& m_EndArray == m_ArraySlices
			&& m_BeginPlane == 0
			&& m_EndPlane == m_PlaneCount)
		{
			uint32 startSubresource = D3D12CalcSubresource(0, 0, m_BeginPlane, m_MipLevels, m_ArraySlices);
			uint32 endSubresource = D3D12CalcSubresource(0, 0, m_EndPlane, m_MipLevels, m_ArraySlices);

			// Only coalesce if the full-resolution UINTs fit in the UINT8s used for storage here
			if (endSubresource < static_cast<uint8>(-1))
			{
				m_BeginArray = 0;
				m_EndArray = 1;
				m_BeginPlane = 0;
				m_EndPlane = 1;
				m_BeginMip = static_cast<uint8>(startSubresource);
				m_EndMip = static_cast<uint8>(endSubresource);
			}
		}
	}

protected:
	uint8 m_MipLevels;
	uint16 m_ArraySlices;
	uint8 m_PlaneCount;
	uint8 m_MostDetailedMip;
	uint16 m_ViewArraySize;
};

// This iterator iterates over contiguous ranges of subresources within a subresource subset. eg:
//
// // For each contiguous subresource range.
// for( CViewSubresourceIterator it = ViewSubset.begin(); it != ViewSubset.end(); ++it )
// {
//      // StartSubresource and EndSubresource members of the iterator describe the contiguous range.
//      for( uint32 SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++ )
//      {
//          // Action for each subresource within the current range.
//      }
//  }
//
class CViewSubresourceSubset::CViewSubresourceIterator
{
public:
	inline CViewSubresourceIterator(CViewSubresourceSubset const& SubresourceSet, uint16 ArraySlice, uint8 PlaneSlice)
		: m_Subresources(SubresourceSet)
		, m_CurrentArraySlice(ArraySlice)
		, m_CurrentPlaneSlice(PlaneSlice)
	{
	}

	inline CViewSubresourceSubset::CViewSubresourceIterator& operator++()
	{
		check(m_CurrentArraySlice < m_Subresources.m_EndArray);

		if (++m_CurrentArraySlice >= m_Subresources.m_EndArray)
		{
			check(m_CurrentPlaneSlice < m_Subresources.m_EndPlane);
			m_CurrentArraySlice = m_Subresources.m_BeginArray;
			++m_CurrentPlaneSlice;
		}

		return *this;
	}

	inline CViewSubresourceSubset::CViewSubresourceIterator& operator--()
	{
		if (m_CurrentArraySlice <= m_Subresources.m_BeginArray)
		{
			m_CurrentArraySlice = m_Subresources.m_EndArray;

			check(m_CurrentPlaneSlice > m_Subresources.m_BeginPlane);
			--m_CurrentPlaneSlice;
		}

		--m_CurrentArraySlice;

		return *this;
	}

	inline bool operator==(CViewSubresourceIterator const& other) const
	{
		return &other.m_Subresources == &m_Subresources
			&& other.m_CurrentArraySlice == m_CurrentArraySlice
			&& other.m_CurrentPlaneSlice == m_CurrentPlaneSlice;
	}

	inline bool operator!=(CViewSubresourceIterator const& other) const
	{
		return !(other == *this);
	}

	inline uint32 StartSubresource() const
	{
		return D3D12CalcSubresource(m_Subresources.m_BeginMip, m_CurrentArraySlice, m_CurrentPlaneSlice, m_Subresources.m_MipLevels, m_Subresources.m_ArraySlices);
	}

	inline uint32 EndSubresource() const
	{
		return D3D12CalcSubresource(m_Subresources.m_EndMip, m_CurrentArraySlice, m_CurrentPlaneSlice, m_Subresources.m_MipLevels, m_Subresources.m_ArraySlices);
	}

	inline TPair<uint32, uint32> operator*() const
	{
		TPair<uint32, uint32> NewPair;
		NewPair.Key = StartSubresource();
		NewPair.Value = EndSubresource();
		return NewPair;
	}

private:
	CViewSubresourceSubset const& m_Subresources;
	uint16 m_CurrentArraySlice;
	uint8 m_CurrentPlaneSlice;
};

inline CViewSubresourceSubset::CViewSubresourceIterator CViewSubresourceSubset::begin() const
{
	return CViewSubresourceIterator(*this, m_BeginArray, m_BeginPlane);
}

inline CViewSubresourceSubset::CViewSubresourceIterator CViewSubresourceSubset::end() const
{
	return CViewSubresourceIterator(*this, m_BeginArray, m_EndPlane);
}

inline bool CViewSubresourceSubset::IsWholeResource() const
{
	return m_BeginMip == 0 && m_BeginArray == 0 && m_BeginPlane == 0 && (m_EndMip * m_EndArray * m_EndPlane == m_MipLevels * m_ArraySlices * m_PlaneCount);
}

inline uint32 CViewSubresourceSubset::ArraySize() const
{
	return m_ArraySlices;
}

inline uint8 CViewSubresourceSubset::MostDetailedMip() const
{
	return m_MostDetailedMip;
}

inline uint16 CViewSubresourceSubset::ViewArraySize() const
{
	return m_ViewArraySize;
}

inline uint32 CViewSubresourceSubset::MinSubresource() const
{
	return (*begin()).Key;
}

inline uint32 CViewSubresourceSubset::MaxSubresource() const
{
	return (*(--end())).Value;
}

// The view is either based on a resource location or a resource; not both
class FD3D12ViewGeneric
{
protected:
	CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor;
	uint32 DescriptorHeapIndex;
	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation;
	TRefCountPtr<FD3D12Resource> D3DResource;
	FD3D12ResidencyHandle* ResidencyHandle;

	explicit FD3D12ViewGeneric(FD3D12ResourceLocation* InResourceLocation)
		: ResourceLocation(InResourceLocation)
		, D3DResource(nullptr)
		, ResidencyHandle((InResourceLocation) ? InResourceLocation->GetResource()->GetResidencyHandle() : nullptr)
	{
		Descriptor.ptr = 0;
		DescriptorHeapIndex = 0;
	}

	explicit FD3D12ViewGeneric(FD3D12Resource* InResource)
		: ResourceLocation(nullptr)
		, D3DResource(InResource)
		, ResidencyHandle((InResource) ? InResource->GetResidencyHandle() : nullptr)
	{
		Descriptor.ptr = 0;
		DescriptorHeapIndex = 0;
	}

	void ResetFromResourceLocation(FD3D12ResourceLocation* InResourceLocation)
	{
		ResourceLocation = InResourceLocation;
		D3DResource = nullptr;
		Descriptor.ptr = 0;
		DescriptorHeapIndex = 0;

		if (InResourceLocation)
		{
			ResidencyHandle = InResourceLocation->GetResource()->GetResidencyHandle();
		}
	}

	void ResetFromD3DResource(FD3D12Resource* InResource)
	{
		D3DResource = InResource;
		ResourceLocation = nullptr;
		Descriptor.ptr = 0;
		DescriptorHeapIndex = 0;

		if (InResource)
		{
			ResidencyHandle = InResource->GetResidencyHandle();
		}
	}

public:
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetView() const { check(Descriptor.ptr != 0); return Descriptor; }
	uint32 GetDescriptorHeapIndex() const { return DescriptorHeapIndex; }
	FD3D12ResourceLocation* GetResourceLocation() const { return ResourceLocation; }
	FD3D12Resource* GetResource() const
	{
		if (D3DResource)
		{
			return D3DResource;
		}
		else if (ResourceLocation)
		{
			return ResourceLocation->GetResource();
		}

		return nullptr;
	}

	FD3D12ResidencyHandle* GetResidencyHandle() { return ResidencyHandle; }

	friend class FD3D12VertexBuffer;
};

template <typename TDesc> struct TCreateViewMap;
template<> struct TCreateViewMap < D3D12_SHADER_RESOURCE_VIEW_DESC > { static decltype(&ID3D12Device::CreateShaderResourceView) GetCreate() { return &ID3D12Device::CreateShaderResourceView; } };
template<> struct TCreateViewMap < D3D12_RENDER_TARGET_VIEW_DESC > { static decltype(&ID3D12Device::CreateRenderTargetView) GetCreate() { return &ID3D12Device::CreateRenderTargetView; } };
template<> struct TCreateViewMap < D3D12_DEPTH_STENCIL_VIEW_DESC > { static decltype(&ID3D12Device::CreateDepthStencilView) GetCreate() { return &ID3D12Device::CreateDepthStencilView; } };
template<> struct TCreateViewMap < D3D12_UNORDERED_ACCESS_VIEW_DESC > { static decltype(&ID3D12Device::CreateUnorderedAccessView) GetCreate() { return &ID3D12Device::CreateUnorderedAccessView; } };

template <typename TDesc>
class FD3D12View : public FD3D12ViewGeneric, public FD3D12DeviceChild
{
protected:
	CViewSubresourceSubset ViewSubresourceSubset;
	TDesc Desc;

protected:
	explicit FD3D12View()
		: FD3D12ViewGeneric(nullptr)
		, FD3D12DeviceChild(nullptr)
	{
	}

	explicit FD3D12View(FD3D12Device* InParent, const TDesc* InDesc, FD3D12ResourceLocation* InResourceLocation, ViewSubresourceSubsetFlags InFlags)
		: FD3D12ViewGeneric(InResourceLocation)
		, ViewSubresourceSubset(*InDesc,
			GetResource() ? GetResource()->GetMipLevels() : 0,
			GetResource() ? GetResource()->GetArraySize() : 0,
			GetResource() ? GetResource()->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
			InFlags)
		, FD3D12DeviceChild(InParent)
	{
		Init(InDesc);
	}

	explicit FD3D12View(FD3D12Device* InParent, const TDesc* InDesc, FD3D12Resource* InResource, ViewSubresourceSubsetFlags InFlags)
		: FD3D12ViewGeneric(InResource)
		, ViewSubresourceSubset(*InDesc,
			GetResource() ? GetResource()->GetMipLevels() : 0,
			GetResource() ? GetResource()->GetArraySize() : 0,
			GetResource() ? GetResource()->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
			InFlags)
		, FD3D12DeviceChild(InParent)
	{
		Init(InDesc);
	}

	virtual ~FD3D12View()
	{
		FreeHeapSlot();
	}

	void Init(const TDesc* InDesc)
	{
		if (InDesc)
		{
			Desc = *InDesc;
		}
		else
		{
			FMemory::Memzero(&Desc, sizeof(Desc));
		}
		AllocateHeapSlot();
	}

	// Implemented in D3D12Device.h due to dependency on FD3D12Device declaration
	void AllocateHeapSlot();

	// Implemented in D3D12Device.h due to dependency on FD3D12Device declaration
	void FreeHeapSlot();

	void UpdateViewSubresourceSubset(const FD3D12Resource* const InResource)
	{
		if (InResource == GetResource())
		{
			// Nothing to update
			return;
		}

		if (InResource)
		{
			const D3D12_RESOURCE_DESC &ResDesc = InResource->GetDesc();
			ViewSubresourceSubset.m_MipLevels = ResDesc.MipLevels;
			ViewSubresourceSubset.m_ArraySlices = ResDesc.DepthOrArraySize;
			ViewSubresourceSubset.m_PlaneCount = GetPlaneCount(ResDesc.Format);
		}
		else
		{
			// Null resource
			ViewSubresourceSubset.m_MipLevels = 0;
			ViewSubresourceSubset.m_ArraySlices = 0;
			ViewSubresourceSubset.m_PlaneCount = 0;
		}
	}


public:
	// Implemented in D3D12Device.h due to dependency on FD3D12Device declaration
	void CreateView(FD3D12Resource* InResource = nullptr, FD3D12Resource* InCounterResource = nullptr);

	// Implemented in D3D12Device.h due to dependency on FD3D12Device declaration
	void CreateViewWithCounter(FD3D12Resource* InResource, FD3D12Resource* InCounterResource);

	const TDesc& GetDesc() const { return Desc; }
	const CViewSubresourceSubset& GetViewSubresourceSubset() const { return ViewSubresourceSubset; }

	template< class T >
	inline bool DoesNotOverlap(const FD3D12View< T >& Other) const
	{
		return ViewSubresourceSubset.DoesNotOverlap(Other.GetViewSubresourceSubset());
	}
};

extern LONGLONG D3D12ShaderResourceViewSequenceNumber;

/** Shader resource view class. */
class FD3D12ShaderResourceView : public FRHIShaderResourceView, public FD3D12View < D3D12_SHADER_RESOURCE_VIEW_DESC >
{
	uint64 SequenceNumber;
	const bool bIsBuffer;
	const bool bContainsDepthPlane;
	const bool bContainsStencilPlane;
	uint32 Stride;

public:
	FD3D12ShaderResourceView(FD3D12Device* InParent, D3D12_SHADER_RESOURCE_VIEW_DESC* InSRVDesc, FD3D12ResourceLocation* InResourceLocation, uint32 InStride = 1)
		: FD3D12View(InParent, InSRVDesc, InResourceLocation, ViewSubresourceSubsetFlags_None)
		, SequenceNumber(InterlockedIncrement64(&D3D12ShaderResourceViewSequenceNumber))
		, bIsBuffer(InSRVDesc->ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
		, bContainsDepthPlane(InResourceLocation ? InResourceLocation->GetResource()->IsDepthStencilResource() && InSRVDesc->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D && InSRVDesc->Texture2D.PlaneSlice == 0: false)
		, bContainsStencilPlane(InResourceLocation ? InResourceLocation->GetResource()->IsDepthStencilResource() && InSRVDesc->ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D && InSRVDesc->Texture2D.PlaneSlice == 1 : false)
		, Stride(InStride)
	{
		if (bIsBuffer && !!InResourceLocation)
		{
			check(InResourceLocation->GetOffset() / Stride == InSRVDesc->Buffer.FirstElement);
		}

		CreateView();
	}

	void Rename(FD3D12ResourceLocation* InResourceLocation, CD3DX12_CPU_DESCRIPTOR_HANDLE InDescriptor, uint32 InDescriptorHeapIndex)
	{
		ResourceLocation = InResourceLocation;
		if (bIsBuffer)
		{
			Desc.Buffer.FirstElement = InResourceLocation->GetOffset() / Stride;
		}

		D3DResource = nullptr;

		SequenceNumber = InterlockedIncrement64(&D3D12ShaderResourceViewSequenceNumber);

		// If no descriptor and heap index were provided...
		if (InDescriptor.ptr == 0)
		{
			// Create a new view
			CreateView();
		}
		else
		{
			// Otherwise use the provided descriptor and index
			Descriptor.ptr = InDescriptor.ptr;
			DescriptorHeapIndex = InDescriptorHeapIndex;
		}

		check(ResourceLocation);
		ResidencyHandle = ResourceLocation->GetResource()->GetResidencyHandle();

		ResourceLocation->UpdateDefaultStateCache();
	}

	FORCEINLINE uint64 GetSequenceNumber() const { return SequenceNumber; }
	FORCEINLINE bool IsDepthStencilResource() const { return bContainsDepthPlane || bContainsStencilPlane; }
	FORCEINLINE bool IsDepthPlaneResource() const { return bContainsDepthPlane; }
	FORCEINLINE bool IsStencilPlaneResource() const { return bContainsStencilPlane; }
	FORCEINLINE const D3D12_SHADER_RESOURCE_VIEW_DESC& GetDesc()
	{
		return Desc;
	}
};

class FD3D12UnorderedAccessView : public FRHIUnorderedAccessView, public FD3D12View < D3D12_UNORDERED_ACCESS_VIEW_DESC >
{
public:

	TRefCountPtr<FD3D12Resource> CounterResource;
	bool CounterResourceInitialized;

	FD3D12UnorderedAccessView(FD3D12Device* InParent, D3D12_UNORDERED_ACCESS_VIEW_DESC* InUAVDesc, FD3D12ResourceLocation* InResourceLocation, TRefCountPtr<FD3D12Resource> InCounterResource = nullptr)
		: FD3D12View(InParent, InUAVDesc, InResourceLocation, ViewSubresourceSubsetFlags_None)
		, CounterResource(InCounterResource)
		, CounterResourceInitialized(false)
	{
		CreateViewWithCounter(nullptr, CounterResource);
	}

};

class FD3D12RenderTargetView : public FD3D12View<D3D12_RENDER_TARGET_VIEW_DESC>, public FRHIResource
{
public:
	FD3D12RenderTargetView(FD3D12Device* InParent, D3D12_RENDER_TARGET_VIEW_DESC* InRTVDesc, FD3D12Resource* InResource)
		: FD3D12View(InParent, InRTVDesc, InResource, ViewSubresourceSubsetFlags_None)
	{
		CreateView();
	}
};

class FD3D12DepthStencilView : public FD3D12View<D3D12_DEPTH_STENCIL_VIEW_DESC>, public FRHIResource
{
	const bool bHasDepth;
	const bool bHasStencil;
	CViewSubresourceSubset DepthOnlyViewSubresourceSubset;
	CViewSubresourceSubset StencilOnlyViewSubresourceSubset;

public:
	FD3D12DepthStencilView(FD3D12Device* InParent, D3D12_DEPTH_STENCIL_VIEW_DESC* InDSVDesc, FD3D12Resource* InResource, bool InHasStencil)
		: FD3D12View(InParent, InDSVDesc, InResource, ViewSubresourceSubsetFlags_DepthAndStencilDsv)
		, bHasDepth(true)				// Assume all DSVs have depth bits in their format
		, bHasStencil(InHasStencil)		// Only some DSVs have stencil bits in their format
		, DepthOnlyViewSubresourceSubset(*InDSVDesc, 0, 0, DXGI_FORMAT_UNKNOWN, ViewSubresourceSubsetFlags_DepthOnlyDsv)
		, StencilOnlyViewSubresourceSubset(*InDSVDesc, 0, 0, DXGI_FORMAT_UNKNOWN, ViewSubresourceSubsetFlags_StencilOnlyDsv)
	{
		CreateView();

		// Create individual subresource subsets for each plane
		if (bHasDepth)
		{
			DepthOnlyViewSubresourceSubset = CViewSubresourceSubset(*InDSVDesc,
				InResource ? InResource->GetMipLevels() : 0,
				InResource ? InResource->GetArraySize() : 0,
				InResource ? InResource->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
				ViewSubresourceSubsetFlags_DepthOnlyDsv);
		}

		if (bHasStencil)
		{
			StencilOnlyViewSubresourceSubset = CViewSubresourceSubset(*InDSVDesc,
				InResource ? InResource->GetMipLevels() : 0,
				InResource ? InResource->GetArraySize() : 0,
				InResource ? InResource->GetDesc().Format : DXGI_FORMAT_UNKNOWN,
				ViewSubresourceSubsetFlags_StencilOnlyDsv);
		}
	}

	bool HasDepth() const
	{
		return bHasDepth;
	}

	bool HasStencil() const
	{
		return bHasStencil;
	}

	CViewSubresourceSubset& GetDepthOnlyViewSubresourceSubset()
	{
		check(bHasDepth);
		return DepthOnlyViewSubresourceSubset;
	}

	CViewSubresourceSubset& GetStencilOnlyViewSubresourceSubset()
	{
		check(bHasStencil);
		return StencilOnlyViewSubresourceSubset;
	}
};

/** Updates tracked stats for a buffer. */
#define D3D12_BUFFER_TYPE_CONSTANT   1
#define D3D12_BUFFER_TYPE_INDEX      2
#define D3D12_BUFFER_TYPE_VERTEX     3
#define D3D12_BUFFER_TYPE_STRUCTURED 4
extern void UpdateBufferStats(FD3D12ResourceLocation* ResourceLocation, bool bAllocating, uint32 BufferType);

/** Forward declare the constants ring buffer. */
class FD3D12ConstantsRingBuffer;

/** A ring allocation from the constants ring buffer. */
struct FRingAllocation
{
	FD3D12Resource* Buffer;
	void* DataPtr;
	uint32 Offset;
	uint32 Size;

	FRingAllocation() : Buffer(NULL) {}
	inline bool IsValid() const { return Buffer != NULL; }
};

/** Uniform buffer resource class. */
class FD3D12UniformBuffer : public FRHIUniformBuffer, public FD3D12DeviceChild
{
public:
	/** The handle to the descriptor in the offline descriptor heap */
	CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineDescriptorHandle;

	/** Unique number which identifies this constant buffer */
	uint64 SequenceNumber;

	/** The D3D12 constant buffer resource */
	TRefCountPtr<FD3D12ResourceLocation> ResourceLocation;

	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;
	
	/** Index of the descriptor in the offline heap */
	uint32 OfflineHeapIndex;

	/** Initialization constructor. */
	FD3D12UniformBuffer(class FD3D12Device* InParent, const FRHIUniformBufferLayout& InLayout, FD3D12ResourceLocation* InResourceLocation, const FRingAllocation& InRingAllocation, uint64 InSequenceNumber)
		: FRHIUniformBuffer(InLayout)
		, ResourceLocation(InResourceLocation)
		, OfflineHeapIndex(UINT_MAX)
		, SequenceNumber(InSequenceNumber)
		, FD3D12DeviceChild(InParent)
	{
		OfflineDescriptorHandle.ptr = 0;
	}

	virtual ~FD3D12UniformBuffer();

private:
	class FD3D12DynamicRHI* D3D12RHI;
};

/** Index buffer resource class that stores stride information. */
class FD3D12IndexBuffer : public FRHIIndexBuffer, public FD3D12BaseShaderResource
{
public:

	FD3D12IndexBuffer(FD3D12Device* InParent, FD3D12ResourceLocation* InResourceLocation, uint32 InStride, uint32 InSize, uint32 InUsage)
		: FRHIIndexBuffer(InStride, InSize, InUsage)
		, FD3D12BaseShaderResource(InResourceLocation, InParent)
	{}

	virtual ~FD3D12IndexBuffer();

	void Rename(FD3D12ResourceLocation* NewResource);

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
};

/** Structured buffer resource class. */
class FD3D12StructuredBuffer : public FRHIStructuredBuffer, public FD3D12BaseShaderResource
{
public:
	TRefCountPtr<FD3D12Resource> Resource;

	FD3D12StructuredBuffer(FD3D12Device* InParent, FD3D12ResourceLocation* InResourceLocation, uint32 InStride, uint32 InSize, uint32 InUsage)
		: FRHIStructuredBuffer(InStride, InSize, InUsage)
		, FD3D12BaseShaderResource(InResourceLocation, InParent)
	{
		Resource = InResourceLocation->GetResource();
	}

	FD3D12StructuredBuffer(FD3D12Device* InParent, FD3D12Resource* InResource, uint32 InStride, uint32 InSize, uint32 InUsage)
		: FRHIStructuredBuffer(InStride, InSize, InUsage)
		, FD3D12BaseShaderResource(InResource, InParent)
		, Resource(InResource)
	{}

	void Rename(FD3D12ResourceLocation* NewResource);

	virtual ~FD3D12StructuredBuffer();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
};

/** Vertex buffer resource class. */
class FD3D12VertexBuffer : public FRHIVertexBuffer, public FD3D12BaseShaderResource
{
public:
	// Current SRV
	FD3D12ShaderResourceView* DynamicSRV;

	FD3D12VertexBuffer(FD3D12Device* InParent, FD3D12ResourceLocation* InResourceLocation, uint32 InSize, uint32 InUsage)
		: FRHIVertexBuffer(InSize, InUsage)
		, FD3D12BaseShaderResource(InResourceLocation, InParent)
		, DynamicSRV(nullptr)
	{
	}

	virtual ~FD3D12VertexBuffer();

	void Rename(FD3D12ResourceLocation* NewResource);

	void SetDynamicSRV(FD3D12ShaderResourceView* InSRV)
	{
		DynamicSRV = InSRV;
	}

	FD3D12ShaderResourceView* GetDynamicSRV() const
	{
		return DynamicSRV;
	}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
};

namespace D3D12RHI
{
	void ReturnPooledTexture2D(int32 MipCount, EPixelFormat PixelFormat, FD3D12Resource* InResource);
	void ReleasePooledTextures();
}

class FD3D12ResourceBarrierBatcher : public FNoncopyable
{
public:
	explicit FD3D12ResourceBarrierBatcher()
	{};

	// Add a UAV barrier to the batch. Ignoring the actual resource for now.
	void AddUAV()
	{
		Barriers.AddUninitialized();
		D3D12_RESOURCE_BARRIER& Barrier = Barriers.Last();
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.UAV.pResource = nullptr;	// Ignore the resource ptr for now. HW doesn't do anything with it.
	}

	// Add a transition resource barrier to the batch.
	void AddTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
	{
		check(Before != After);
		Barriers.AddUninitialized();
		D3D12_RESOURCE_BARRIER& Barrier = Barriers.Last();
		Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Barrier.Transition.StateBefore = Before;
		Barrier.Transition.StateAfter = After;
		Barrier.Transition.Subresource = Subresource;
		Barrier.Transition.pResource = pResource;
	}

	// Flush the batch to the specified command list then reset.
	void Flush(ID3D12GraphicsCommandList* pCommandList)
	{
		if (Barriers.Num())
		{
			check(pCommandList);
			pCommandList->ResourceBarrier(Barriers.Num(), Barriers.GetData());
			Reset();
		}
	}

	// Clears the batch.
	void Reset()
	{
		Barriers.SetNumUnsafeInternal(0);	// Reset the array without shrinking (Does not destruct items, does not de-allocate memory).
		check(Barriers.Num() == 0);
	}

	const TArray<D3D12_RESOURCE_BARRIER>& GetBarriers() const
	{
		return Barriers;
	}

private:
	TArray<D3D12_RESOURCE_BARRIER> Barriers;
};

class FD3D12ResourceHelper : public FD3D12DeviceChild
{
public:
	HRESULT CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource);
	HRESULT CreatePlacedResource(const D3D12_RESOURCE_DESC& Desc, FD3D12Heap* BackingHeap, uint64 HeapOffset, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppResource);
	HRESULT CreateDefaultResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource, D3D12_RESOURCE_STATES InitialState);
	HRESULT CreateBuffer(D3D12_HEAP_TYPE heapType, uint64 heapSize, FD3D12Resource** ppOutResource, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, const D3D12_HEAP_PROPERTIES *pCustomHeapProperties = nullptr);

	FD3D12ResourceHelper(FD3D12Device* InParent);
};

template<class T>
struct TD3D12ResourceTraits
{
};
template<>
struct TD3D12ResourceTraits<FRHIVertexDeclaration>
{
	typedef FD3D12VertexDeclaration TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIVertexShader>
{
	typedef FD3D12VertexShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIGeometryShader>
{
	typedef FD3D12GeometryShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIHullShader>
{
	typedef FD3D12HullShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIDomainShader>
{
	typedef FD3D12DomainShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIPixelShader>
{
	typedef FD3D12PixelShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIComputeShader>
{
	typedef FD3D12ComputeShader TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIBoundShaderState>
{
	typedef FD3D12BoundShaderState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIUniformBuffer>
{
	typedef FD3D12UniformBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIIndexBuffer>
{
	typedef FD3D12IndexBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIStructuredBuffer>
{
	typedef FD3D12StructuredBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIVertexBuffer>
{
	typedef FD3D12VertexBuffer TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIShaderResourceView>
{
	typedef FD3D12ShaderResourceView TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIUnorderedAccessView>
{
	typedef FD3D12UnorderedAccessView TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHISamplerState>
{
	typedef FD3D12SamplerState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIRasterizerState>
{
	typedef FD3D12RasterizerState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIDepthStencilState>
{
	typedef FD3D12DepthStencilState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIBlendState>
{
	typedef FD3D12BlendState TConcreteType;
};
template<>
struct TD3D12ResourceTraits<FRHIComputeFence>
{
	typedef FD3D12Fence TConcreteType;
};