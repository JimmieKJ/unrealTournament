// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of Device Context State Caching to improve draw
//	thread performance by removing redundant device context calls.

#pragma once
#include "Queue.h"
#include "D3D12DirectCommandListManager.h"

//-----------------------------------------------------------------------------
//	Configuration
//-----------------------------------------------------------------------------

// If set, includes a runtime toggle console command for debugging D3D11 state caching.
// ("TOGGLESTATECACHE")
#define D3D12_STATE_CACHE_RUNTIME_TOGGLE 0

// If set, includes a cache state verification check.
// After each state set call, the cached state is compared against the actual state of the ID3D11DeviceContext.
// This is *very slow* and should only be enabled to debug the state caching system.
#ifndef D3D11_STATE_CACHE_DEBUG
#define D3D11_STATE_CACHE_DEBUG 0
#endif

#define MAX_SRVS 22
#define MAX_CBS 8

// Uncomment only for debugging of the descriptor heap management; this is very noisy
//#define VERBOSE_DESCRIPTOR_HEAP_DEBUG 1

// The number of view descriptors available per (online) descriptor heap, depending on hardware tier
#define NUM_VIEW_DESCRIPTORS_PER_CONTEXT_TIER_1 250000
#define NUM_VIEW_DESCRIPTORS_PER_CONTEXT_TIER_2 150000
#define NUM_SAMPLER_DESCRIPTORS D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE
#define DESCRIPTOR_HEAP_BLOCK_SIZE 10000

#define NUM_VIEW_DESCRIPTORS_TIER_1 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1
#define NUM_VIEW_DESCRIPTORS_TIER_2 D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2
// Tier 3 Hardware is essentially bounded by available memory
#define NUM_VIEW_DESCRIPTORS_TIER_3 1500000

// Heap for updating UAV counter values.
#define COUNTER_HEAP_SIZE 1024 * 64

// Keep set state functions inline to reduce call overhead
#define D3D12_STATE_CACHE_INLINE FORCEINLINE

#if D3D12_STATE_CACHE_RUNTIME_TOGGLE
extern bool GD3D12SkipStateCaching;
#else
static const bool GD3D12SkipStateCaching = false;
#endif
template <typename T>
inline void hash_combine(SIZE_T & seed, const T & v)
{
	seed ^= GetTypeHash(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

#define PSO_IF_NOT_EQUAL_RETURN_FALSE( value ) if(lhs.##value != rhs.##value){ return false; }

#define PSO_IF_MEMCMP_FAILS_RETURN_FALSE( value ) if(FMemory::Memcmp(&lhs.##value, &rhs.##value, sizeof(rhs.##value)) != 0){ return false; }

#define PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE( value ) \
	const char* const lhString = lhs.##value##; \
	const char* const rhString = rhs.##value##; \
	if (lhString != rhString) \
	{ \
		if (strcmp(lhString, rhString) != 0) \
		{ \
			return false; \
		} \
	}

template <typename TDesc> struct equality_pipeline_state_desc;
template <> struct equality_pipeline_state_desc<FD3D12HighLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12HighLevelGraphicsPipelineStateDesc& lhs, const FD3D12HighLevelGraphicsPipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(BoundShaderState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(BlendState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DepthStencilState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(RasterizerState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleMask);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PrimitiveTopologyType);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(NumRenderTargets);
		for (SIZE_T i = 0; i < lhs.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(RTVFormats[i]);
		}
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSVFormat);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleDesc.Count);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleDesc.Quality);
		return true;
	}
};

template <> struct equality_pipeline_state_desc<FD3D12LowLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12LowLevelGraphicsPipelineStateDesc& lhs, const FD3D12LowLevelGraphicsPipelineStateDesc& rhs)
	{
		// Order from most likely to change to least
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.VS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.GS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.HS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.NumElements)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NumRenderTargets)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DSVFormat)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PrimitiveTopologyType)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.IBStripCutValue)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.RasterizedStream)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumEntries)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumStrides)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Count)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Quality)

		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.BlendState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.RasterizerState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.DepthStencilState)
		for (SIZE_T i = 0; i < lhs.Desc.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTVFormats[i]);
		}

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(VSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(GSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(HSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSHash)

#if UE_BUILD_DEBUG
		const D3D12_SHADER_BYTECODE* lhByteCode = &lhs.Desc.VS;
		const D3D12_SHADER_BYTECODE* rhByteCode = &rhs.Desc.VS;
		for (SIZE_T i = 0; i < 5; i++)
		{
			if (lhByteCode[i].pShaderBytecode != rhByteCode[i].pShaderBytecode &&
				lhByteCode[i].BytecodeLength)
			{
				if (FMemory::Memcmp(lhByteCode[i].pShaderBytecode, rhByteCode[i].pShaderBytecode, rhByteCode[i].BytecodeLength) != 0)
				{
					UE_LOG(LogD3D12RHI, Error, TEXT("Error! there is a collision with the SHA1. This should never happen but checking for completeness."));
					return false;
				}
			}
		}
#endif

		if (lhs.Desc.StreamOutput.pSODeclaration != rhs.Desc.StreamOutput.pSODeclaration &&
			lhs.Desc.StreamOutput.NumEntries)
		{
			for (SIZE_T i = 0; i < lhs.Desc.StreamOutput.NumEntries; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].Stream)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticIndex )
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].StartComponent )
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].ComponentCount )
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].OutputSlot )
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticName )
			}
		}

		if (lhs.Desc.StreamOutput.pBufferStrides != rhs.Desc.StreamOutput.pBufferStrides &&
			lhs.Desc.StreamOutput.NumStrides)
		{
			for (SIZE_T i = 0; i < lhs.Desc.StreamOutput.NumStrides; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pBufferStrides[i])
			}
		}

		if (lhs.Desc.InputLayout.pInputElementDescs != rhs.Desc.InputLayout.pInputElementDescs &&
			lhs.Desc.InputLayout.NumElements)
		{
			for (SIZE_T i = 0; i < lhs.Desc.InputLayout.NumElements; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].Format)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlot)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].AlignedByteOffset)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlotClass)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InstanceDataStepRate)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticName)
			}
		}
		return true;
	}
};
template <> struct equality_pipeline_state_desc<FD3D12ComputePipelineStateDesc>
{
	bool operator()(const FD3D12ComputePipelineStateDesc& lhs, const FD3D12ComputePipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.CS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)
		
		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(CSHash)

#if UE_BUILD_DEBUG
		if (lhs.Desc.CS.pShaderBytecode != rhs.Desc.CS.pShaderBytecode &&
			lhs.Desc.CS.BytecodeLength)
		{
			if (FMemory::Memcmp(lhs.Desc.CS.pShaderBytecode, rhs.Desc.CS.pShaderBytecode, lhs.Desc.CS.BytecodeLength) != 0)
			{
				return false;
			}
		}
#endif

		return true;
	}
};

// Like a TMap<KeyType, ValueType>
// Faster lookup performance, but possibly has false negatives
template<typename KeyType, typename ValueType>
class FD3D12ConservativeMap
{
public:
	FD3D12ConservativeMap(uint32 Size)
	{
		Table.AddUninitialized(Size);

		Reset();
	}

	void Add(const KeyType& Key, const ValueType& Value)
	{
		uint32 Index = GetIndex(Key);

		Entry& Pair = Table[Index];

		Pair.Valid = true;
		Pair.Key = Key;
		Pair.Value = Value;
	}

	ValueType* Find(const KeyType& Key)
	{
		uint32 Index = GetIndex(Key);

		Entry& Pair = Table[Index];

		if (Pair.Valid &&
			(Pair.Key == Key))
		{
			return &Pair.Value;
		}
		else
		{
			return nullptr;
		}
	}

	void Reset()
	{
		for (int32 i = 0; i < Table.Num(); i++)
		{
			Table[i].Valid = false;
		}
	}

private:
	uint32 GetIndex(const KeyType& Key)
	{
		uint32 Hash = GetTypeHash(Key);

		return Hash % static_cast<uint32>(Table.Num());
	}

	struct Entry
	{
		bool Valid;
		KeyType Key;
		ValueType Value;
	};

	TArray<Entry> Table;
};

struct FD3D12SamplerArrayDesc
{
	uint32 Count;
	uint16 SamplerID[16];
	inline bool operator==(const FD3D12SamplerArrayDesc& rhs) const
	{
		check(Count <= _countof(SamplerID));
		check(rhs.Count <= _countof(rhs.SamplerID));

		if (Count != rhs.Count)
		{
			return false;
		}
		else
		{
			// It is safe to compare pointers, because samplers are kept alive for the lifetime of the RHI
			return 0 == FMemory::Memcmp(SamplerID, rhs.SamplerID, sizeof(SamplerID[0]) * Count);
		}
	}
};
uint32 GetTypeHash(const FD3D12SamplerArrayDesc& Key);
typedef FD3D12ConservativeMap<FD3D12SamplerArrayDesc, D3D12_GPU_DESCRIPTOR_HANDLE> FD3D12SamplerMap;

struct FD3D12SRVArrayDesc
{
	uint32 Count;
	uint64 SRVSequenceNumber[MAX_SRVS];

	inline bool operator==(const FD3D12SRVArrayDesc& rhs) const
	{
		check(Count <= _countof(SRVSequenceNumber));
		check(rhs.Count <= _countof(rhs.SRVSequenceNumber));

		if (Count != rhs.Count)
		{
			return false;
		}
		else
		{
			return 0 == FMemory::Memcmp(SRVSequenceNumber, rhs.SRVSequenceNumber, sizeof(SRVSequenceNumber[0]) * Count);
		}
	}
};
uint32 GetTypeHash(const FD3D12SRVArrayDesc& Key);
typedef FD3D12ConservativeMap<FD3D12SRVArrayDesc, D3D12_GPU_DESCRIPTOR_HANDLE> FD3D12SRVMap;


class FD3D12ResourceHelper : public FD3D12DeviceChild
{
public:
	HRESULT CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource);
	HRESULT CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource);
	HRESULT CreateDefaultResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppOutResource);
	HRESULT CreateBuffer(D3D12_HEAP_TYPE heapType, uint64 heapSize, FD3D12Resource** ppOutResource, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, const D3D12_HEAP_PROPERTIES *pCustomHeapProperties = nullptr);
	HRESULT CreatePlacedBuffer(ID3D12Heap* BackingHeap, uint64 HeapOffset, D3D12_HEAP_TYPE HeapType, uint64 BufferSize, FD3D12Resource** ppOutResource, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	FD3D12ResourceHelper(FD3D12Device* InParent);
};

// Vertex Buffer State
struct FD3D12VertexBufferState
{
	TRefCountPtr<FD3D12ResourceLocation> VertexBufferLocation;
	uint32 Stride;
	uint32 Offset;
};
struct FD3D12ConstantBufferState
{
	ID3D12Resource *Resource;
	uint32 FirstConstant;
	uint32 NumConstants;
	FD3D12UniformBuffer* UniformBuffer;
};

class FD3D12DynamicRHI;

template< uint32 CPUTableSize>
struct FD3D12UniqueDescriptorTable
{
	FD3D12UniqueDescriptorTable() : GPUHandle({}){};
	FD3D12UniqueDescriptorTable(FD3D12SamplerArrayDesc KeyIn, CD3DX12_CPU_DESCRIPTOR_HANDLE* Table) : GPUHandle({})
	{
		FMemory::Memcpy(&Key, &KeyIn, sizeof(Key));//Memcpy to avoid alignement issues
		FMemory::Memcpy(CPUTable, Table, Key.Count * sizeof(CD3DX12_CPU_DESCRIPTOR_HANDLE));
	}

	FORCEINLINE uint32 GetTypeHash(const FD3D12UniqueDescriptorTable& Table)
	{
		return uint32(FD3D12PipelineStateCache::HashData((void*)Table.Key.SamplerID, Table.Key.Count * sizeof(Table.Key.SamplerID[0])));
	}

	FD3D12SamplerArrayDesc Key;
	CD3DX12_CPU_DESCRIPTOR_HANDLE CPUTable[D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT];

	// This will point to the table start in the global heap
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
};

template<typename FD3D12UniqueDescriptorTable, bool bInAllowDuplicateKeys = false>
struct FD3D12UniqueDescriptorTableKeyFuncs : BaseKeyFuncs<FD3D12UniqueDescriptorTable, FD3D12UniqueDescriptorTable, bInAllowDuplicateKeys>
{
	typedef typename TCallTraits<FD3D12UniqueDescriptorTable>::ParamType KeyInitType;
	typedef typename TCallTraits<FD3D12UniqueDescriptorTable>::ParamType ElementInitType;

	/**
	* @return The key used to index the given element.
	*/
	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element;
	}

	/**
	* @return True if the keys match.
	*/
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.Key == B.Key;
	}

	/** Calculates a hash index for a key. */
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key.Key);
	}
};

typedef FD3D12UniqueDescriptorTable<D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT> FD3D12UniqueSamplerTable;

typedef TSet<FD3D12UniqueSamplerTable, FD3D12UniqueDescriptorTableKeyFuncs<FD3D12UniqueSamplerTable>> FD3D12SamplerSet;

class FD3D12DescriptorCache;

class FD3D12OnlineHeap : public FD3D12DeviceChild
{
public:
	FD3D12OnlineHeap(FD3D12Device* Device, bool CanLoopAround, FD3D12DescriptorCache* _Parent = nullptr) : 
		DescriptorSize(0)
		, Desc({})
		, NextSlotIndex(0)
		, FirstUsedSlot(0)
		, Parent(_Parent)
		, bCanLoopAround(CanLoopAround)
		, FD3D12DeviceChild(Device){};

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSlotHandle(uint32 Slot) const { return{ CPUBase.ptr + Slot * DescriptorSize }; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSlotHandle(uint32 Slot) const { return{ GPUBase.ptr + Slot * DescriptorSize }; }

	uint32 GetDescriptorSize() const { return DescriptorSize; }

	const D3D12_DESCRIPTOR_HEAP_DESC& GetDesc() const { return Desc; }

	// Call this to reserve descriptor heap slots for use by the command list you are currently recording. This will wait if
	// necessary until slots are free (if they are currently in use by another command list.) If the reservation can be
	// fulfilled, the index of the first reserved slot is returned (all reserved slots are consecutive.) If not, it will 
	// throw an exception.
	bool CanReserveSlots(uint32 NumSlots);

	uint32 ReserveSlots(uint32 NumSlotsRequested);

	void SetNextSlot(uint32 NextSlot);

	ID3D12DescriptorHeap* GetHeap() { return Heap.GetReference(); }

	void SetParent(FD3D12DescriptorCache* InParent) { Parent = InParent; }

	// Roll over behavior depends on the heap type
	virtual void RollOver() = 0;
	virtual void NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle);
	virtual uint32 GetTotalSize();

	static const uint32 HeapExhaustedValue = uint32(-1);

protected:

	FD3D12DescriptorCache* Parent;

	FD3D12CommandListHandle CurrentCommandList;

	// Handles for manipulation of the heap
	uint32 DescriptorSize;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUBase;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUBase;

	// This index indicate where the next set of descriptors should be placed *if* there's room
	uint32 NextSlotIndex;

	// Indicates the last free slot marked by the command list being finished
	uint32 FirstUsedSlot;

	// Keeping this ptr around is basically just for lifetime management
	TRefCountPtr<ID3D12DescriptorHeap> Heap;

	// Desc contains the number of slots and allows for easy recreation
	D3D12_DESCRIPTOR_HEAP_DESC Desc;

	const bool bCanLoopAround;
};

class FD3D12GlobalOnlineHeap : public FD3D12OnlineHeap
{
public:
	FD3D12GlobalOnlineHeap(FD3D12Device* Device)
		: bUniqueDescriptorTablesAreDirty(false)
		, FD3D12OnlineHeap(Device, false)
	{ }

	void Init(uint32 TotalSize, D3D12_DESCRIPTOR_HEAP_TYPE Type);

	void ToggleDescriptorTablesDirtyFlag(bool Value) { bUniqueDescriptorTablesAreDirty = Value; }
	bool DescriptorTablesDirty() { return bUniqueDescriptorTablesAreDirty; }
	FD3D12SamplerSet& GetUniqueDescriptorTables() { return UniqueDescriptorTables; }
	FCriticalSection& GetCriticalSection() { return CriticalSection; }

	void RollOver();
private:

	FD3D12SamplerSet UniqueDescriptorTables;
	bool bUniqueDescriptorTablesAreDirty;

	FCriticalSection CriticalSection;
};

struct FD3D12OnlineHeapBlock
{
public:
	FD3D12OnlineHeapBlock(uint32 _BaseSlot, uint32 _Size) :
		BaseSlot(_BaseSlot), Size(_Size), SizeUsed(0), bFresh(true){};
	FD3D12OnlineHeapBlock() : BaseSlot(0), Size(0), SizeUsed(0), bFresh(true){}

	FD3D12CLSyncPoint SyncPoint;
	uint32 BaseSlot;
	uint32 Size;
	uint32 SizeUsed;
	// Indicates that this has never been used in a Command List before
	bool bFresh;
};

class FD3D12SubAllocatedOnlineHeap : public FD3D12OnlineHeap
{
public:
	struct SubAllocationDesc 
	{
		SubAllocationDesc() :ParentHeap(nullptr), BaseSlot(0), Size(0){};
		SubAllocationDesc(FD3D12GlobalOnlineHeap* _ParentHeap, uint32 _BaseSlot, uint32 _Size) :
			ParentHeap(_ParentHeap), BaseSlot(_BaseSlot), Size(_Size){};

		FD3D12GlobalOnlineHeap* ParentHeap;
		uint32 BaseSlot;
		uint32 Size;
	};

	FD3D12SubAllocatedOnlineHeap(FD3D12Device* Device, FD3D12DescriptorCache* Parent) :
		FD3D12OnlineHeap(Device, false, Parent){};

	void Init(SubAllocationDesc _Desc);

	// Specializations
	void RollOver();
	void NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle);
	uint32 GetTotalSize();

private:

	TQueue<FD3D12OnlineHeapBlock> DescriptorBlockPool;
	SubAllocationDesc SubDesc;

	FD3D12OnlineHeapBlock CurrentSubAllocation;
};

class FD3D12ThreadLocalOnlineHeap : public FD3D12OnlineHeap
{
public:
	FD3D12ThreadLocalOnlineHeap(FD3D12Device* Device, FD3D12DescriptorCache* _Parent)
		: FD3D12OnlineHeap(Device, true, _Parent)
	{ }

	void RollOver();

	void NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle);

	void Init(uint32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type);

private:
	struct SyncPointEntry
	{
		FD3D12CLSyncPoint SyncPoint;
		uint32 LastSlotInUse;

		SyncPointEntry() : LastSlotInUse(0)
		{}

		SyncPointEntry(const SyncPointEntry& InSyncPoint) : SyncPoint(InSyncPoint.SyncPoint), LastSlotInUse(InSyncPoint.LastSlotInUse)
		{}

		SyncPointEntry& operator = (const SyncPointEntry& InSyncPoint)
		{
			SyncPoint = InSyncPoint.SyncPoint;
			LastSlotInUse = InSyncPoint.LastSlotInUse;

			return *this;
		}
	};
	TQueue<SyncPointEntry> SyncPoints;

	struct PoolEntry
	{
		TRefCountPtr<ID3D12DescriptorHeap> Heap;
		FD3D12CLSyncPoint SyncPoint;

		PoolEntry()
		{}

		PoolEntry(const PoolEntry& InPoolEntry) : Heap(InPoolEntry.Heap), SyncPoint(InPoolEntry.SyncPoint)
		{}

		PoolEntry& operator = (const PoolEntry& InPoolEntry)
		{
			Heap = InPoolEntry.Heap;
			SyncPoint = InPoolEntry.SyncPoint;

			return *this;
		}
	};
	PoolEntry Entry;
	TQueue<PoolEntry> ReclaimPool;
};

//-----------------------------------------------------------------------------
//	FD3D12DescriptorCache Class Definition
//-----------------------------------------------------------------------------
class FD3D12DescriptorCache : public FD3D12DeviceChild
{
protected:
	FD3D12CommandContext* CmdContext;

public:
	FD3D12OnlineHeap* GetCurrentViewHeap() { return CurrentViewHeap; }
	FD3D12OnlineHeap* GetCurrentSamplerHeap() { return CurrentSamplerHeap; }

	FD3D12DescriptorCache()
		: LocalViewHeap(nullptr)
		, SubAllocatedViewHeap(nullptr, this)
		, LocalSamplerHeap(nullptr, this)
		, CBVAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1)
		, ViewHeapSequenceNumber(1) // starts at 1, because 0 means "is not in any heap"
		, SamplerMap(271) // Prime numbers for better hashing
		, SRVMap(271)
		, bUsingGlobalSamplerHeap(false)
		, CurrentViewHeap(nullptr)
		, CurrentSamplerHeap(nullptr)
		, NumLocalViewDescriptors(0)
		, FD3D12DeviceChild(nullptr)
	{
		CmdContext = nullptr;
	}

	~FD3D12DescriptorCache()
	{
		if (LocalViewHeap) { delete(LocalViewHeap); }
	}

	inline ID3D12DescriptorHeap *GetViewDescriptorHeap()
	{
		return CurrentViewHeap->GetHeap();
	}

	inline ID3D12DescriptorHeap *GetSamplerDescriptorHeap()
	{
		return CurrentSamplerHeap->GetHeap();
	}

	// Notify the descriptor cache of the current fence value every time you start recording a command list; this allows
	// us to avoid querying DX12 for that value thousands of times per frame, which can be costly.
	void NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle);

	// ------------------------------------------------------
	// end Descriptor Slot Reservation stuff

	// null views
	FDescriptorHeapManager CBVAllocator;	// CBV allocator of null CBV
	CD3DX12_CPU_DESCRIPTOR_HANDLE pNullCBV;
	TRefCountPtr<FD3D12ShaderResourceView> pNullSRV;
	TRefCountPtr<FD3D12UnorderedAccessView> pNullUAV;
	TRefCountPtr<FD3D12RenderTargetView> pNullRTV;
	TRefCountPtr<FD3D12SamplerState> pDefaultSampler;

	void SetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset);
	void SetVertexBuffers(FD3D12VertexBufferState* VertexBuffers, uint32 Count);
	void SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, TRefCountPtr<FD3D12UnorderedAccessView>* UnorderedAccessViewArray, uint32 Count, uint32 &HeapSlot);
	void SetRenderTargets(FD3D12RenderTargetView **RenderTargetViewArray, uint32 Count, FD3D12DepthStencilView* DepthStencilTarget, bool bDepthIsBoundAsSRV);
	void SetSamplers(EShaderFrequency ShaderStage, FD3D12SamplerState** Samplers, uint32 Count, uint32 &HeapSlot);
	void SetSRVs(EShaderFrequency ShaderStage, TRefCountPtr<FD3D12ShaderResourceView> * SRVs, uint32 Count, bool* CurrentShaderResourceViewsIntersectWithDepthRT, uint32 &HeapSlot);
	void SetConstantBuffers(EShaderFrequency ShaderStage, uint32 Count, uint32 &HeapSlot);
	void SetStreamOutTargets(FD3D12Resource **Buffers, uint32 Count, const uint32* Offsets);

	void SetConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex, FD3D12UniformBuffer* UniformBuffer);
	void SetConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex, ID3D12Resource* Resource, uint32 OffsetInBytes, uint32 SizeInBytes);
	void ClearConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex);

	void HeapRolledOver(D3D12_DESCRIPTOR_HEAP_TYPE Type);
	void HeapLoopedAround(D3D12_DESCRIPTOR_HEAP_TYPE Type);
	void Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, uint32 InNumViewDescriptors, uint32 InNumSamplerDescriptors, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc);
	void Clear();
	void BeginFrame();
	void EndFrame();
	void GatherUniqueSamplerTables();

	void SwitchToContextLocalViewHeap();
	void SwitchToContextLocalSamplerHeap();
	void SwitchToGlobalSamplerHeap();

	struct
	{
		TRefCountPtr<ID3D12DescriptorHeap>  Heap;

		TArray<uint64>				CurrentSRVSequenceNumber;
		CD3DX12_CPU_DESCRIPTOR_HANDLE SRVBaseHandle;

		TArray<uint64>				CurrentUniformBufferSequenceNumber;
		CD3DX12_CPU_DESCRIPTOR_HANDLE CBVBaseHandle;
	} OfflineHeap[SF_NumFrequencies];

	TArray<FD3D12UniqueSamplerTable>& GetUniqueTables() { return UniqueTables; }

	bool UsingGlobalSamplerHeap() { return bUsingGlobalSamplerHeap; }
	void DisableGlobalSamplerHeap() { bUsingGlobalSamplerHeap = false; }
	FD3D12SamplerSet& GetLocalSamplerSet() { return LocalSamplerSet; }

private:
	FD3D12OnlineHeap* CurrentViewHeap;
	FD3D12OnlineHeap* CurrentSamplerHeap;

	FD3D12ThreadLocalOnlineHeap* LocalViewHeap;
	FD3D12ThreadLocalOnlineHeap LocalSamplerHeap;
	FD3D12SubAllocatedOnlineHeap SubAllocatedViewHeap;

	FD3D12SamplerMap SamplerMap;
	FD3D12SRVMap SRVMap;
	uint64 ViewHeapSequenceNumber;

	TArray<FD3D12UniqueSamplerTable> UniqueTables;

	FD3D12SamplerSet LocalSamplerSet;
	bool bUsingGlobalSamplerHeap;

	uint32 NumLocalViewDescriptors;
};

class FDiskCacheInterface
{
	// Increment this if changes are made to the
	// disk caches so stale caches get updated correctly
	static const uint32 mCurrentHeaderVersion = 3;
	struct FDiskCacheHeader
	{
		uint32 mHeaderVersion;
		uint32 mNumPsos;
		bool   mUsesAPILibraries;
	};

private:
	FString mFileName;
	byte*   mFileStart;
	HANDLE  hFile;
	HANDLE  hMemoryMap;
	HANDLE  hMapAddress;
	SIZE_T  mCurrentFileMapSize;
	SIZE_T  mCurrentOffset;
	bool    mCacheExists;
	bool    mInErrorState;
	FDiskCacheHeader mHeader;

	// There is the potential for the file mapping to grow
	// in that case all of the pointers will be invalid. Back
	// some of the pointers we might read again (i.e. shade byte
	// code for PSO mapping) in persitent system memory.
	TArray<void*> mBackedMemory;

	static const SIZE_T mFileGrowSize = (1024 * 1024); // 1 megabyte;

	void GrowMapping(SIZE_T size, bool firstrun);

public:
	bool AppendData(void* pData, size_t size);
	bool SetPointerAndAdvanceFilePosition(void** pDest, size_t size, bool backWithSystemMemory = false);
	void Reset();
	void Init(FString &filename);
	void Close(uint32 numberOfPSOs);
	void Flush(uint32 numberOfPSOs);
	void ClearDiskCache();
	uint32 GetNumPSOs() const
	{
		return mHeader.mNumPsos;
	}
	bool IsInErrorState() const;

	~FDiskCacheInterface()
	{
		for (void* memory : mBackedMemory)
		{
			if (memory)
			{
				FMemory::Free(memory);
			}
		}
	}
};

static bool GCPUSupportsSSE4;

struct FD3D12PipelineStateWorker : public FD3D12DeviceChild, public FNonAbandonableTask
{
	FD3D12PipelineStateWorker(FD3D12Device* Device, D3D12_COMPUTE_PIPELINE_STATE_DESC* _Desc)
		: bIsGraphics(false), FD3D12DeviceChild(Device) { Desc.ComputeDesc = *_Desc; };

	FD3D12PipelineStateWorker(FD3D12Device* Device, D3D12_GRAPHICS_PIPELINE_STATE_DESC* _Desc)
		: bIsGraphics(true), FD3D12DeviceChild(Device) { Desc.GraphicsDesc = *_Desc; };

	void DoWork();

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12PipelineStateWorker, STATGROUP_ThreadPoolAsyncTasks); }

	union
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDesc;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDesc;
	} Desc;

	bool bIsGraphics;
	TRefCountPtr<ID3D12PipelineState> PSO;
};

struct FD3D12PipelineState : public FD3D12DeviceChild
{
public:
	FD3D12PipelineState() :Worker(nullptr), FD3D12DeviceChild(nullptr){};
	FD3D12PipelineState(FD3D12Device* Parent) : Worker(nullptr), FD3D12DeviceChild(Parent){};

	~FD3D12PipelineState();

	void Create(FD3D12ComputePipelineStateDesc* Desc);
	void CreateAsync(FD3D12ComputePipelineStateDesc* Desc);

	void Create(FD3D12LowLevelGraphicsPipelineStateDesc* Desc);
	void CreateAsync(FD3D12LowLevelGraphicsPipelineStateDesc* Desc);

	ID3D12PipelineState* GetPipelineState();

private:
	TRefCountPtr<ID3D12PipelineState> PipelineState;

	FAsyncTask<FD3D12PipelineStateWorker>*	Worker;
};

class FD3D12PipelineStateCache : public FD3D12DeviceChild
{
private:
	enum PSO_CACHE_TYPE {
		PSO_CACHE_GRAPHICS,
		PSO_CACHE_COMPUTE,
		NUM_PSO_CACHE_TYPES
	};

	template <typename TDesc, typename TValue>
	struct TStateCacheKeyFuncs : BaseKeyFuncs<TPair<TDesc, TValue>, TDesc, false>
	{
		typedef typename TTypeTraits<TDesc>::ConstPointerType KeyInitType;
		typedef const typename TPairInitializer<typename TTypeTraits<TDesc>::ConstInitType, typename TTypeTraits<TValue>::ConstInitType>& ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			equality_pipeline_state_desc<TDesc> equal;
			return equal(A, B);
		}
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return Key.CombinedHash;
		}
	};

	template <typename TDesc, typename TValue = FD3D12PipelineState>
	using TPipelineCache = TMap<TDesc, TValue, FDefaultSetAllocator, TStateCacheKeyFuncs<TDesc, TValue>>;

	TPipelineCache<FD3D12HighLevelGraphicsPipelineStateDesc, TPair<ID3D12PipelineState*, uint64>> HighLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12LowLevelGraphicsPipelineStateDesc> LowLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12ComputePipelineStateDesc> ComputePipelineStateCache;

	FCriticalSection CS;
	FDiskCacheInterface DiskCaches[NUM_PSO_CACHE_TYPES];

	ID3D12PipelineState* Add(FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc);
	ID3D12PipelineState* Add(FD3D12ComputePipelineStateDesc &computePSODesc);

	ID3D12PipelineState* FindGraphicsLowLevel(FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc);

#if UE_BUILD_DEBUG
	uint64 GraphicsCacheRequestCount = 0;
	uint64 HighLevelCacheFulfillCount = 0;
	uint64 HighLevelCacheStaleCount = 0;
	uint64 HighLevelCacheMissCount = 0;
#endif

public:
	void RebuildFromDiskCache();

	ID3D12PipelineState* FindGraphics(FD3D12HighLevelGraphicsPipelineStateDesc &graphicsPSODesc);
	ID3D12PipelineState* FindCompute(FD3D12ComputePipelineStateDesc &computePSODesc);

	void Close();

	void Init(FString &GraphicsCacheFilename, FString &ComputeCacheFilename);

	static SIZE_T HashPSODesc(const FD3D12HighLevelGraphicsPipelineStateDesc &psoDesc);
	static SIZE_T HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc &psoDesc);
	static SIZE_T HashPSODesc(const FD3D12ComputePipelineStateDesc &psoDesc);

	static inline SIZE_T HashData(void* data, SIZE_T numBytes);

	FD3D12PipelineStateCache(FD3D12Device* InParent);
	~FD3D12PipelineStateCache();

	FD3D12PipelineStateCache& operator=(const FD3D12PipelineStateCache&);

	static const bool bUseAPILibaries = false;
};

class FD3D12BitArray
{
public:

	FD3D12BitArray() : Array(0) {}
	FD3D12BitArray(uint32 InArray) : Array(InArray) {}

	void Clear()
	{
		Array = 0;
	}

	void SetIndex(uint32 Index)
	{
		Array |= (1 << Index);
	}

	void ClearIndex(uint32 Index)
	{
		Array &= ~(1 << Index);
	}

	void Set(uint32 Bits)
	{
		for (uint32 i = 0; i < Bits; ++i)
		{
			SetIndex(i);
		}
	}

	bool AnySet() const
	{
		return (Array != 0);
	}

	uint32 LastSet() const
	{
		return 32 - FMath::CountLeadingZeros(Array);
	}

	uint32 GetValue() const
	{
		return Array;
	}

private:

	uint32 Array;
};

class FD3D12StateArray
{
public:

	void SetIndex(uint32 Index)
	{
		SetIndices.SetIndex(Index);
		ChangedIndices.SetIndex(Index);
	}

	void ClearIndex(uint32 Index)
	{
		SetIndices.ClearIndex(Index);
		ChangedIndices.SetIndex(Index);
	}

	uint32 LastSet() const
	{
		FD3D12BitArray Merged(SetIndices.GetValue() | ChangedIndices.GetValue());

		return Merged.LastSet();
	}

	void Clear()
	{
		SetIndices.Clear();
		ChangedIndices.Clear();
	}

	void ClearChanges()
	{
		ChangedIndices.Clear();
	}

private:

	FD3D12BitArray SetIndices;
	FD3D12BitArray ChangedIndices;
};

//-----------------------------------------------------------------------------
//	FD3D12StateCache Class Definition
//-----------------------------------------------------------------------------
class FD3D12StateCacheBase : public FD3D12DeviceChild
{
	friend class FD3D12DynamicRHI;

public:
	enum ESRV_Type
	{
		SRV_Unknown,
		SRV_Dynamic,
		SRV_Static,
	};
protected:
	FD3D12CommandContext* CmdContext;

	bool bNeedSetVB;
	bool bNeedSetIB;
	bool bNeedSetUAVsPerShaderStage[SF_NumFrequencies];
	bool bNeedSetRTs;
	bool bNeedSetSOs;
	bool bNeedSetSamplersPerShaderStage[SF_NumFrequencies];
	bool bNeedSetSamplers;
	bool bNeedSetSRVsPerShaderStage[SF_NumFrequencies];
	bool bSRVSCleared;
	bool bNeedSetSRVs;
	bool bNeedSetConstantBuffersPerShaderStage[SF_NumFrequencies];
	bool bNeedSetConstantBuffers;
	bool bNeedSetViewports;
	bool bNeedSetScissorRects;
	bool bNeedSetPrimitiveTopology;
	bool bNeedSetBlendFactor;
	bool bNeedSetStencilRef;
	bool bAutoFlushComputeShaderCache;
	D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;

	struct
	{
		struct
		{
			// Cache
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedRebuildPSO;

			// Note: Current root signature is part of the bound shader state
			bool bNeedSetRootSignature;

			// Full high level PSO desc
			FD3D12HighLevelGraphicsPipelineStateDesc HighLevelDesc;

			// Depth Stencil State Cache
			uint32 CurrentReferenceStencil;

			// Blend State Cache
			float CurrentBlendFactor[4];

			// Viewport
			uint32	CurrentNumberOfViewports;
			D3D12_VIEWPORT CurrentViewport[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

			// Vertex Buffer State
			FD3D12VertexBufferState CurrentVertexBuffers[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			int32 MaxBoundVertexBufferIndex;

			// SO
			uint32			CurrentNumberOfStreamOutTargets;
			FD3D12Resource* CurrentStreamOutTargets[D3D12_SO_STREAM_COUNT];
			uint32			CurrentSOOffsets[D3D12_SO_STREAM_COUNT];

			// Index Buffer State
			TRefCountPtr<FD3D12ResourceLocation> CurrentIndexBufferLocation;
			DXGI_FORMAT CurrentIndexFormat;
			uint32 CurrentIndexOffset;

			// Primitive Topology State
			D3D_PRIMITIVE_TOPOLOGY CurrentPrimitiveTopology;

			// Input Layout State
			D3D12_RECT CurrentScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			D3D12_RECT CurrentViewportScissorRects[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			uint32 CurrentNumberOfScissorRects;

			FD3D12RenderTargetView* RenderTargetArray[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

			FD3D12DepthStencilView* CurrentDepthStencilTarget;
		} Graphics;

		struct
		{
			// Cache
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedRebuildPSO;

			// Note: Current root signature is part of the bound compute shader
			bool bNeedSetRootSignature;

			// Compute
			FD3D12ComputeShader* CurrentComputeShader;
		} Compute;

		struct
		{
			// UAVs
			TRefCountPtr<FD3D12UnorderedAccessView> UnorderedAccessViewArray[SF_NumFrequencies][D3D12_PS_CS_UAV_REGISTER_COUNT];
			uint32	CurrentUAVStartSlot[SF_NumFrequencies];

			// Shader Resource Views Cache
			TRefCountPtr<FD3D12ShaderResourceView> CurrentShaderResourceViews[SF_NumFrequencies][D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			bool CurrentShaderResourceViewsIntersectWithDepthRT[SF_NumFrequencies][MAX_SRVS];
			uint32 ShaderResourceViewsIntersectWithDepthCount;
			int32 MaxBoundShaderResourcesIndex[SF_NumFrequencies];

			// Sampler State
			FD3D12SamplerState* CurrentSamplerStates[SF_NumFrequencies][D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT];

			// PSO
			ID3D12PipelineState* CurrentPipelineStateObject;
			bool bNeedSetPSO;

			uint32 CurrentShaderSamplerCounts[SF_NumFrequencies];
			uint32 CurrentShaderSRVCounts[SF_NumFrequencies];
			uint32 CurrentShaderCBCounts[SF_NumFrequencies];
			uint32 CurrentShaderUAVCounts[SF_NumFrequencies];
		} Common;
	} PipelineState;

	FD3D12DescriptorCache DescriptorCache;
	bool bAlwaysSetIndexBuffers;

	void InternalSetIndexBuffer(FD3D12ResourceLocation *IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset);

	typedef void(*TSetSRVAlternate)(FD3D12StateCacheBase* StateCache, FD3D12ShaderResourceView* SRV, uint32 ResourceIndex, ESRV_Type SrvType);
	template <EShaderFrequency ShaderFrequency>
	void InternalSetShaderResourceView(FD3D12ShaderResourceView*& SRV, uint32 ResourceIndex, ESRV_Type SrvType, TSetSRVAlternate AlternatePathFunction)
	{
		check(ResourceIndex < ARRAYSIZE(PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency]));
		auto& CurrentShaderResourceViews = PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency];
		if ((CurrentShaderResourceViews[ResourceIndex] != SRV) || GD3D12SkipStateCaching)
		{
			// Keep track of the highest bound resource
			int32& MaxResourceIndex = PipelineState.Common.MaxBoundShaderResourcesIndex[ShaderFrequency];
			if (SRV != nullptr)
			{
				// Mark the SRVs as not cleared
				bSRVSCleared = false;

				check(ResourceIndex < MAX_SRVS);
				// Update the max resource index to the highest bound resource index.
				MaxResourceIndex = FMath::Max(MaxResourceIndex, static_cast<int32>(ResourceIndex));
			}
			else
			{
				// If this was the highest bound resource...
				if (MaxResourceIndex == ResourceIndex)
				{
					// Adjust the max resource index downwards until we
					// hit the next non-null slot, or we've run out of slots.
					do
					{
						MaxResourceIndex--;
					} while (MaxResourceIndex >= 0 && CurrentShaderResourceViews[MaxResourceIndex] == nullptr);
				}
			}

			CurrentShaderResourceViews[ResourceIndex] = SRV;
			bNeedSetSRVsPerShaderStage[ShaderFrequency] = true;
			bNeedSetSRVs = true;

			if (FD3D12DynamicRHI::ResourceViewsIntersect(PipelineState.Graphics.CurrentDepthStencilTarget, SRV))
			{
				const D3D12_DEPTH_STENCIL_VIEW_DESC &dsvDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetDesc();
				const bool bReadOnlyDepth = (dsvDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH) != 0;
				if (bReadOnlyDepth)
				{
					// If the DSV has the read only depth then we can leave the depth stencil bound. This should be safe,
					// as only the stencil bits should have artifacts and the shader reading the DS shouldn't care about it.
					if (!PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
					{
						PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = true;
						PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount++;
					}
				}
				else
				{
					// Unbind the DSV because it's being used for depth write
					check(!bReadOnlyDepth);
					PipelineState.Graphics.CurrentDepthStencilTarget = nullptr;
					if (PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
					{
						PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = false;
						PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount--;
					}
				}
			}
			else
			{
				if (PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
				{
					PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = false;
					PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount--;
				}
			}

			if (AlternatePathFunction != nullptr)
			{
				(*AlternatePathFunction)(this, SRV, ResourceIndex, SrvType);
			}
		}
	}

	typedef void(*TSetStreamSourceAlternate)(FD3D12StateCacheBase* StateCache, FD3D12Resource* VertexBuffer, uint32 StreamIndex, uint32 Stride, uint32 Offset);
	void InternalSetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset, TSetStreamSourceAlternate AlternatePathFunction);

	typedef void(*TSetSamplerStateAlternate)(FD3D12StateCacheBase* StateCache, FD3D12SamplerState* SamplerState, uint32 SamplerIndex);
	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void InternalSetSamplerState(FD3D12SamplerState* SamplerState, uint32 SamplerIndex, TSetSamplerStateAlternate AlternatePathFunction)
	{
		check(SamplerIndex < ARRAYSIZE(PipelineState.Common.CurrentSamplerStates[ShaderFrequency]));;
		if ((PipelineState.Common.CurrentSamplerStates[ShaderFrequency][SamplerIndex] != SamplerState) || GD3D12SkipStateCaching)
		{
			PipelineState.Common.CurrentSamplerStates[ShaderFrequency][SamplerIndex] = SamplerState;
			bNeedSetSamplersPerShaderStage[ShaderFrequency] = true;
			bNeedSetSamplers = true;
			if (AlternatePathFunction != nullptr)
			{
				(*AlternatePathFunction)(this, SamplerState, SamplerIndex);
			}
		}
	}

	// Shorthand for typing/reading convenience
	D3D12_STATE_CACHE_INLINE FD3D12BoundShaderState* BSS()
	{
		return PipelineState.Graphics.HighLevelDesc.BoundShaderState;
	}

	template <typename TShader> struct StateCacheShaderTraits;
#define DECLARE_SHADER_TRAITS(Name) \
	template <> struct StateCacheShaderTraits<FD3D12##Name##Shader> \
	{ \
		static const EShaderFrequency Frequency = SF_##Name; \
		static FD3D12##Name##Shader* GetShader(FD3D12BoundShaderState* BSS) { return BSS ? BSS->Get##Name##Shader() : nullptr; } \
	}
	DECLARE_SHADER_TRAITS(Vertex);
	DECLARE_SHADER_TRAITS(Pixel);
	DECLARE_SHADER_TRAITS(Domain);
	DECLARE_SHADER_TRAITS(Hull);
	DECLARE_SHADER_TRAITS(Geometry);
#undef DECLARE_SHADER_TRAITS

	template <typename TShader> D3D12_STATE_CACHE_INLINE void SetShader(TShader* Shader)
	{
		typedef StateCacheShaderTraits<TShader> Traits;
		TShader* OldShader = Traits::GetShader(BSS());
		if (OldShader != Shader)
		{
			PipelineState.Common.CurrentShaderSamplerCounts[Traits::Frequency] = (Shader) ? Shader->ResourceCounts.NumSamplers : 0;
			PipelineState.Common.CurrentShaderSRVCounts[Traits::Frequency]     = (Shader) ? Shader->ResourceCounts.NumSRVs     : 0;
			PipelineState.Common.CurrentShaderCBCounts[Traits::Frequency]      = (Shader) ? Shader->ResourceCounts.NumCBs      : 0;
			PipelineState.Common.CurrentShaderUAVCounts[Traits::Frequency]     = (Shader) ? Shader->ResourceCounts.NumUAVs     : 0;
		}
	}

	template <typename TShader> D3D12_STATE_CACHE_INLINE void GetShader(TShader** Shader)
	{
		*Shader = StateCacheShaderTraits<TShader>::GetShader(BSS());
	}

public:

	void InheritState(const FD3D12StateCacheBase& AncestralCache)
	{
		FMemory::Memcpy(&PipelineState, &AncestralCache.PipelineState, sizeof(PipelineState));
		RestoreState();
	}

	FD3D12DescriptorCache* GetDescriptorCache()
	{
		return &DescriptorCache;
	}

	ID3D12PipelineState* GetPipelineStateObject()
	{
		return PipelineState.Common.CurrentPipelineStateObject;
	}

	FD3D12RootSignature* GetGraphicsRootSignature()
	{
		return PipelineState.Graphics.HighLevelDesc.BoundShaderState ?
			PipelineState.Graphics.HighLevelDesc.BoundShaderState->pRootSignature : nullptr;
	}

	FD3D12RootSignature* GetComputeRootSignature()
	{
		return PipelineState.Compute.CurrentComputeShader ?
			PipelineState.Compute.CurrentComputeShader->pRootSignature : nullptr;
	}

	void ClearSamplers();
	void ClearSRVs();

	template <EShaderFrequency ShaderFrequency>
	void ClearShaderResourceViews(FD3D12ResourceLocation*& ResourceLocation)
	{
		int32& MaxResourceIndex = PipelineState.Common.MaxBoundShaderResourcesIndex[ShaderFrequency];
		auto& CurrentShaderResourceViews = PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency];

		const int32 OriginalMaxResourceIndex = MaxResourceIndex;
		for (int32 i = 0; i <= OriginalMaxResourceIndex; ++i)
		{
			if (CurrentShaderResourceViews[i].GetReference() && CurrentShaderResourceViews[i]->GetResourceLocation() == ResourceLocation)
			{
				SetShaderResourceView<ShaderFrequency>(nullptr, i);
			}
		}

		// Restore the max index to make sure we set null descriptors in the slots of any SRVs we unbind due to them overlapping with other
		// resources like RTVs and DSVs.
		MaxResourceIndex = OriginalMaxResourceIndex;
	}

	void FD3D12StateCacheBase::FlushComputeShaderCache(bool bForce = false);

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void SetShaderResourceView(FD3D12ShaderResourceView* SRV, uint32 ResourceIndex, ESRV_Type SrvType = SRV_Unknown)
	{
		InternalSetShaderResourceView<ShaderFrequency>(SRV, ResourceIndex, SrvType, nullptr);
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void GetShaderResourceViews(uint32 StartResourceIndex, uint32& NumResources, FD3D12ShaderResourceView** SRV)
	{
		{
			uint32 NumLoops = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartResourceIndex;
			NumResources = 0;
			for (uint32 ResourceLoop = 0; ResourceLoop < NumLoops; ResourceLoop++)
			{
				SRV[ResourceLoop] = PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency][ResourceLoop + StartResourceIndex];
				if (SRV[ResourceLoop])
				{
					SRV[ResourceLoop]->AddRef();
					NumResources = ResourceLoop;
				}
			}
		}
	}

	void UpdateViewportScissorRects();
	void SetScissorRects(uint32 Count, D3D12_RECT* ScissorRects);
	void SetScissorRect(D3D12_RECT ScissorRect);

	D3D12_STATE_CACHE_INLINE void GetScissorRect(D3D12_RECT *ScissorRect) const
	{
		check(ScissorRect);
		FMemory::Memcpy(ScissorRect, &PipelineState.Graphics.CurrentScissorRects, sizeof(D3D12_RECT));
	}


	void SetViewport(D3D12_VIEWPORT Viewport);
	void SetViewports(uint32 Count, D3D12_VIEWPORT* Viewports);

	D3D12_STATE_CACHE_INLINE void GetViewport(D3D12_VIEWPORT *Viewport) const
	{
		check(Viewport);
		FMemory::Memcpy(Viewport, &PipelineState.Graphics.CurrentViewport, sizeof(D3D12_VIEWPORT));
	}

	D3D12_STATE_CACHE_INLINE void GetViewports(uint32* Count, D3D12_VIEWPORT *Viewports) const
	{
		check(*Count);
		if (Viewports) //NULL is legal if you just want count
		{
			//as per d3d spec
			int32 StorageSizeCount = (int32)(*Count);
			int32 CopyCount = FMath::Min(FMath::Min(StorageSizeCount, (int32)PipelineState.Graphics.CurrentNumberOfViewports), D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
			if (CopyCount > 0)
			{
				FMemory::Memcpy(Viewports, &PipelineState.Graphics.CurrentViewport[0], sizeof(D3D12_VIEWPORT) * CopyCount);
			}
			//remaining viewports in supplied array must be set to zero
			if (StorageSizeCount > CopyCount)
			{
				FMemory::Memset(&Viewports[CopyCount], 0, sizeof(D3D12_VIEWPORT) * (StorageSizeCount - CopyCount));
			}
		}
		*Count = PipelineState.Graphics.CurrentNumberOfViewports;
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void SetSamplerState(FD3D12SamplerState* SamplerState, uint32 SamplerIndex)
	{
		InternalSetSamplerState<ShaderFrequency>(SamplerState, SamplerIndex, nullptr);
	}

	template <EShaderFrequency ShaderFrequency>
	D3D12_STATE_CACHE_INLINE void GetSamplerState(uint32 StartSamplerIndex, uint32 NumSamplerIndexes, FD3D12SamplerState** SamplerStates) const
	{
		check(StartSamplerIndex + NumSamplerIndexes <= D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
		for (uint32 StateLoop = 0; StateLoop < NumSamplerIndexes; StateLoop++)
		{
			SamplerStates[StateLoop] = CurrentShaderResourceViews[ShaderFrequency][StateLoop + StartSamplerIndex];
			if (SamplerStates[StateLoop])
			{
				SamplerStates[StateLoop]->AddRef();
			}
		}
	}

	template <EShaderFrequency ShaderFrequency>
	void D3D12_STATE_CACHE_INLINE SetConstantBuffer(uint32 SlotIndex, FD3D12ResourceLocation* ResourceLocation, FD3D12UniformBuffer* UniformBuffer)
	{
		// Update the high water mark
		check(SlotIndex < MAX_CBS);

		if (UniformBuffer && (UniformBuffer->OfflineDescriptorHandle.ptr != 0))
		{
			check(!ResourceLocation);
			DescriptorCache.SetConstantBuffer(ShaderFrequency, SlotIndex, UniformBuffer);
		}
		else if (ResourceLocation)
		{
			check(!UniformBuffer);
			const uint32 AlignedSize = Align(ResourceLocation->GetEffectiveBufferSize(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			DescriptorCache.SetConstantBuffer(ShaderFrequency, SlotIndex, ResourceLocation->GetResource()->GetResource(), ResourceLocation->GetOffset(), AlignedSize);
		}
		else
		{
			DescriptorCache.ClearConstantBuffer(ShaderFrequency, SlotIndex);
		}

		bNeedSetConstantBuffersPerShaderStage[ShaderFrequency] = true;
		bNeedSetConstantBuffers = true;
	}

	D3D12_STATE_CACHE_INLINE void SetRasterizerState(D3D12_RASTERIZER_DESC* State)
	{
		if (PipelineState.Graphics.HighLevelDesc.RasterizerState != State || GD3D12SkipStateCaching)
		{
			PipelineState.Graphics.HighLevelDesc.RasterizerState = State;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetRasterizerState(D3D12_RASTERIZER_DESC** RasterizerState) const
	{
		*RasterizerState = PipelineState.Graphics.HighLevelDesc.RasterizerState;
	}

	void SetBlendState(D3D12_BLEND_DESC* State, const float BlendFactor[4], uint32 SampleMask);

	D3D12_STATE_CACHE_INLINE void GetBlendState(D3D12_BLEND_DESC** BlendState, float BlendFactor[4], uint32* SampleMask) const
	{
		*BlendState = PipelineState.Graphics.HighLevelDesc.BlendState;
		*SampleMask = PipelineState.Graphics.HighLevelDesc.SampleMask;
		FMemory::Memcpy(BlendFactor, PipelineState.Graphics.CurrentBlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor));
	}

	void SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC* State, uint32 RefStencil);

	D3D12_STATE_CACHE_INLINE void GetDepthStencilState(D3D12_DEPTH_STENCIL_DESC** DepthStencilState, uint32* StencilRef) const
	{
		*DepthStencilState = PipelineState.Graphics.HighLevelDesc.DepthStencilState;
		*StencilRef = PipelineState.Graphics.CurrentReferenceStencil;
	}

	D3D12_STATE_CACHE_INLINE void GetVertexShader(FD3D12VertexShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetHullShader(FD3D12HullShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetDomainShader(FD3D12DomainShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetGeometryShader(FD3D12GeometryShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void GetPixelShader(FD3D12PixelShader** Shader)
	{
		GetShader(Shader);
	}

	D3D12_STATE_CACHE_INLINE void SetBoundShaderState(FD3D12BoundShaderState* BoundShaderState)
	{
		if (BoundShaderState)
		{
			SetShader(BoundShaderState->GetVertexShader());
			SetShader(BoundShaderState->GetPixelShader());
			SetShader(BoundShaderState->GetDomainShader());
			SetShader(BoundShaderState->GetHullShader());
			SetShader(BoundShaderState->GetGeometryShader());
		}
		else
		{
			SetShader<FD3D12VertexShader>(nullptr);
			SetShader<FD3D12PixelShader>(nullptr);
			SetShader<FD3D12HullShader>(nullptr);
			SetShader<FD3D12DomainShader>(nullptr);
			SetShader<FD3D12GeometryShader>(nullptr);
		}

		FD3D12BoundShaderState*& CurrentBSS = PipelineState.Graphics.HighLevelDesc.BoundShaderState;
		if (CurrentBSS != BoundShaderState)
		{
			const FD3D12RootSignature* const pCurrentRootSignature = CurrentBSS ? CurrentBSS->pRootSignature : nullptr;
			const FD3D12RootSignature* const pNewRootSignature = BoundShaderState ? BoundShaderState->pRootSignature : nullptr;
			if (pCurrentRootSignature != pNewRootSignature)
			{
				PipelineState.Graphics.bNeedSetRootSignature = true;
			}

			CurrentBSS = BoundShaderState;
			PipelineState.Graphics.bNeedRebuildPSO = true;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetBoundShaderState(FD3D12BoundShaderState** BoundShaderState) const
	{
		*BoundShaderState = PipelineState.Graphics.HighLevelDesc.BoundShaderState;
	}

	D3D12_STATE_CACHE_INLINE void SetComputeShader(FD3D12ComputeShader* Shader)
	{
		if (PipelineState.Compute.CurrentComputeShader != Shader)
		{
			// See if we need to change the root signature
			const FD3D12RootSignature* const pCurrentRootSignature = PipelineState.Compute.CurrentComputeShader ? PipelineState.Compute.CurrentComputeShader->pRootSignature : nullptr;
			const FD3D12RootSignature* const pNewRootSignature = Shader ? Shader->pRootSignature : nullptr;
			if (pCurrentRootSignature != pNewRootSignature)
			{
				PipelineState.Compute.bNeedSetRootSignature = true;
			}

			PipelineState.Compute.CurrentComputeShader                  = Shader;
			PipelineState.Compute.bNeedRebuildPSO = true;
			PipelineState.Common.CurrentShaderSamplerCounts[SF_Compute] = (Shader) ? Shader->ResourceCounts.NumSamplers : 0;
			PipelineState.Common.CurrentShaderSRVCounts[SF_Compute]     = (Shader) ? Shader->ResourceCounts.NumSRVs     : 0;
			PipelineState.Common.CurrentShaderCBCounts[SF_Compute]      = (Shader) ? Shader->ResourceCounts.NumCBs      : 0;
			PipelineState.Common.CurrentShaderUAVCounts[SF_Compute]     = (Shader) ? Shader->ResourceCounts.NumUAVs     : 0;
		}
	}

	D3D12_STATE_CACHE_INLINE void GetComputeShader(FD3D12ComputeShader** ComputeShader)
	{
		*ComputeShader = PipelineState.Compute.CurrentComputeShader;
	}

	D3D12_STATE_CACHE_INLINE void GetInputLayout(D3D12_INPUT_LAYOUT_DESC* InputLayout) const
	{
		*InputLayout = PipelineState.Graphics.HighLevelDesc.BoundShaderState->InputLayout;
	}

	D3D12_STATE_CACHE_INLINE void SetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset)
	{
		InternalSetStreamSource(VertexBufferLocation, StreamIndex, Stride, Offset, nullptr);
	}

	D3D12_STATE_CACHE_INLINE void GetStreamSources(uint32 StartStreamIndex, uint32 NumStreams, FD3D12ResourceLocation** VertexBuffers, uint32* Strides, uint32* Offsets)
	{
		check(StartStreamIndex + NumStreams <= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
		for (uint32 StreamLoop = 0; StreamLoop < NumStreams; StreamLoop++)
		{
			FD3D12VertexBufferState& Slot = PipelineState.Graphics.CurrentVertexBuffers[StreamLoop + StartStreamIndex];
			VertexBuffers[StreamLoop] = Slot.VertexBufferLocation;
			if (Strides)
			{
				Strides[StreamLoop] = Slot.Stride;
			}

			if (Offsets)
			{
				Offsets[StreamLoop] = Slot.Offset + Slot.VertexBufferLocation->GetOffset();
			}

			if (Slot.VertexBufferLocation)
			{
				Slot.VertexBufferLocation->AddRef();
			}
		}
	}

	D3D12_STATE_CACHE_INLINE bool IsShaderResource(const FD3D12ResourceLocation* VertexBufferLocation) const
	{
		for (int i = 0; i < SF_NumFrequencies; i++)
		{
			for (uint32 j = 0; j < D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++j)
			{
				if (PipelineState.Common.CurrentShaderResourceViews[i][j] && PipelineState.Common.CurrentShaderResourceViews[i][j]->GetResourceLocation())
				{
					if (PipelineState.Common.CurrentShaderResourceViews[i][j]->GetResourceLocation() == VertexBufferLocation)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	D3D12_STATE_CACHE_INLINE bool IsStreamSource(const FD3D12ResourceLocation* VertexBufferLocation) const
	{
		for (int32 index = 0; index <= PipelineState.Graphics.MaxBoundVertexBufferIndex; ++index)
		{
			if (PipelineState.Graphics.CurrentVertexBuffers[index].VertexBufferLocation.GetReference() == VertexBufferLocation)
			{
				return true;
			}
		}

		return false;
	}

public:

	D3D12_STATE_CACHE_INLINE void SetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset)
	{
		InternalSetIndexBuffer(IndexBufferLocation, Format, Offset);
	}

	D3D12_STATE_CACHE_INLINE const FD3D12ResourceLocation *GetIndexBufferLocation() const
	{
		return PipelineState.Graphics.CurrentIndexBufferLocation;
	}

	D3D12_STATE_CACHE_INLINE bool IsIndexBuffer(const FD3D12ResourceLocation *ResourceLocation) const
	{
		return PipelineState.Graphics.CurrentIndexBufferLocation &&
			PipelineState.Graphics.CurrentIndexBufferLocation->GetResource() == ResourceLocation->GetResource()
			&& PipelineState.Graphics.CurrentIndexBufferLocation->GetOffset() == ResourceLocation->GetOffset();
	}

	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);


	D3D12_STATE_CACHE_INLINE void GetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY* PrimitiveTopology) const
	{
		*PrimitiveTopology = PipelineState.Graphics.CurrentPrimitiveTopology;
	}

	FD3D12StateCacheBase()
	{
		// Make all the bound shader resources -1
		for (auto& Index : PipelineState.Common.MaxBoundShaderResourcesIndex)
		{
			Index = INDEX_NONE;
		}

		PipelineState.Graphics.MaxBoundVertexBufferIndex = INDEX_NONE;
	}

	void Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, const FD3D12StateCacheBase* AncestralState, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc, bool bInAlwaysSetIndexBuffers = false);

	~FD3D12StateCacheBase()
	{
	}

	void ApplyState(bool IsCompute = false);
	void ApplySamplers(const FD3D12RootSignature* const pRootSignature, uint32 StartStage, uint32 EndStage);
	void RestoreState();
	void DirtyViewDescriptorTables();
	void DirtySamplerDescriptorTables();
	bool VerifyResourceStates(const bool IsCompute);

	template <class ViewT>
	bool VerifyViewState(ID3D12DebugCommandList* pDebugCommandList, FD3D12View<ViewT> *pView, D3D12_RESOURCE_STATES State);

	void SetRenderTargets(uint32 NumSimultaneousRenderTargets, FD3D12RenderTargetView** RTArray, FD3D12DepthStencilView* DSTarget);
	D3D12_STATE_CACHE_INLINE void GetRenderTargets(FD3D12RenderTargetView **RTArray, uint32* NumSimultaneousRTs, FD3D12DepthStencilView** DepthStencilTarget)
	{
		if (RTArray) //NULL is legal
		{
			FMemory::Memcpy(RTArray, PipelineState.Graphics.RenderTargetArray, sizeof(FD3D12RenderTargetView*)* D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
			*NumSimultaneousRTs = PipelineState.Graphics.HighLevelDesc.NumRenderTargets;
		}

		if (DepthStencilTarget)
		{
			*DepthStencilTarget = PipelineState.Graphics.CurrentDepthStencilTarget;
		}
	}

	void SetStreamOutTargets(uint32 NumSimultaneousStreamOutTargets, FD3D12Resource** SOArray, const uint32* SOOffsets);

	void SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32 *UAVInitialCountArray);

	D3D12_STATE_CACHE_INLINE void AutoFlushComputeShaderCache(bool bEnable)
	{
		bAutoFlushComputeShaderCache = bEnable;
	}

	/**
	 * Clears all D3D11 State, setting all input/output resource slots, shaders, input layouts,
	 * predications, scissor rectangles, depth-stencil state, rasterizer state, blend state,
	 * sampler state, and viewports to NULL
	 */
	virtual void ClearState();

	/**
	 * Releases any object references held by the state cache
	 */
	void Clear();

	void ForceRebuildGraphicsPSO() { PipelineState.Graphics.bNeedRebuildPSO = true; }
	void ForceRebuildComputePSO() { PipelineState.Compute.bNeedRebuildPSO = true; }
	void ForceSetGraphicsRootSignature() { PipelineState.Graphics.bNeedSetRootSignature = true; }
	void ForceSetComputeRootSignature() { PipelineState.Compute.bNeedSetRootSignature = true; }
	void ForceSetVB() { bNeedSetVB = true; }
	void ForceSetIB() { bNeedSetIB = true; }
	void ForceSetRTs() { bNeedSetRTs = true; }
	void ForceSetSOs() { bNeedSetSOs = true; }
	void ForceSetSamplersPerShaderStage(uint32 Frequency) { bNeedSetSamplersPerShaderStage[Frequency] = true; }
	void ForceSetSamplers() { bNeedSetSamplers = true; }
	void ForceSetSRVsPerShaderStage(uint32 Frequency) { bNeedSetSRVsPerShaderStage[Frequency] = true; }
	void ForceSetSRVs() { bNeedSetSRVs = true; }
	void ForceSetConstantBuffers() { bNeedSetConstantBuffers = true; }
	void ForceSetViewports() { bNeedSetViewports = true; }
	void ForceSetScissorRects() { bNeedSetScissorRects = true; }
	void ForceSetPrimitiveTopology() { bNeedSetPrimitiveTopology = true; }
	void ForceSetBlendFactor() { bNeedSetBlendFactor = true; }
	void ForceSetStencilRef() { bNeedSetStencilRef = true; }

	bool GetForceRebuildGraphicsPSO() const { return PipelineState.Graphics.bNeedRebuildPSO; }
	bool GetForceRebuildComputePSO() const { return PipelineState.Compute.bNeedRebuildPSO; }
	bool GetForceSetVB() const { return bNeedSetVB; }
	bool GetForceSetIB() const { return bNeedSetIB; }
	bool GetForceSetRTs() const { return bNeedSetRTs; }
	bool GetForceSetSOs() const { return bNeedSetSOs; }
	bool GetForceSetSamplersPerShaderStage(uint32 Frequency) const { return bNeedSetSamplersPerShaderStage[Frequency]; }
	bool GetForceSetSamplers() const { return bNeedSetSamplers; }
	bool GetForceSetSRVsPerShaderStage(uint32 Frequency) const { return bNeedSetSRVsPerShaderStage[Frequency]; }
	bool GetForceSetSRVs() const { return bNeedSetSRVs; }
	bool GetForceSetConstantBuffers() const { return bNeedSetConstantBuffers; }
	bool GetForceSetViewports() const { return bNeedSetViewports; }
	bool GetForceSetScissorRects() const { return bNeedSetScissorRects; }
	bool GetForceSetPrimitiveTopology() const { return bNeedSetPrimitiveTopology; }
	bool GetForceSetBlendFactor() const { return bNeedSetBlendFactor; }
	bool GetForceSetStencilRef() const { return bNeedSetStencilRef; }


#if D3D11_STATE_CACHE_DEBUG
protected:
	// Debug helper methods to verify cached state integrity.
	template <EShaderFrequency ShaderFrequency>
	void VerifySamplerStates();

	template <EShaderFrequency ShaderFrequency>
	void VerifyConstantBuffers();

	template <EShaderFrequency ShaderFrequency>
	void VerifyShaderResourceViews();
#endif
};
