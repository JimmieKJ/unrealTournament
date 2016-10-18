// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12DescriptorCache.h: D3D12 State application functionality
=============================================================================*/
#pragma once

class FD3D12DynamicRHI;
struct FD3D12VertexBufferCache;
struct FD3D12IndexBufferCache;
struct FD3D12ShaderResourceViewCache;

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

uint32 GetTypeHash(const D3D12_SAMPLER_DESC& Desc);
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

template< uint32 CPUTableSize>
struct FD3D12UniqueDescriptorTable
{
	FD3D12UniqueDescriptorTable() : GPUHandle({}) {};
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
		, FD3D12DeviceChild(Device) {};

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSlotHandle(uint32 Slot) const { return{ CPUBase.ptr + Slot * DescriptorSize }; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSlotHandle(uint32 Slot) const { return{ GPUBase.ptr + Slot * DescriptorSize }; }

	inline const uint32 GetDescriptorSize() const { return DescriptorSize; }

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
		BaseSlot(_BaseSlot), Size(_Size), SizeUsed(0), bFresh(true) {};
	FD3D12OnlineHeapBlock() : BaseSlot(0), Size(0), SizeUsed(0), bFresh(true) {}

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
		SubAllocationDesc() :ParentHeap(nullptr), BaseSlot(0), Size(0) {};
		SubAllocationDesc(FD3D12GlobalOnlineHeap* _ParentHeap, uint32 _BaseSlot, uint32 _Size) :
			ParentHeap(_ParentHeap), BaseSlot(_BaseSlot), Size(_Size) {};

		FD3D12GlobalOnlineHeap* ParentHeap;
		uint32 BaseSlot;
		uint32 Size;
	};

	FD3D12SubAllocatedOnlineHeap(FD3D12Device* Device, FD3D12DescriptorCache* Parent) :
		FD3D12OnlineHeap(Device, false, Parent) {};

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

	void SetIndexBuffer(FD3D12IndexBufferCache& Cache);
	void SetVertexBuffers(FD3D12VertexBufferCache& Cache);
	void SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, TRefCountPtr<FD3D12UnorderedAccessView>* UnorderedAccessViewArray, uint32 Count, uint32 &HeapSlot);
	void SetRenderTargets(FD3D12RenderTargetView** RenderTargetViewArray, uint32 Count, FD3D12DepthStencilView* DepthStencilTarget);
	void SetSamplers(EShaderFrequency ShaderStage, FD3D12SamplerState** Samplers, uint32 Count, uint32 &HeapSlot);
	void SetSRVs(EShaderFrequency ShaderStage, FD3D12ShaderResourceViewCache& Cache, uint32 Count, uint32 &HeapSlot);
	void SetConstantBuffers(EShaderFrequency ShaderStage, uint32 Count, uint32 &HeapSlot);
	void SetStreamOutTargets(FD3D12Resource **Buffers, uint32 Count, const uint32* Offsets);

	void SetConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex, FD3D12UniformBuffer* UniformBuffer);
	void SetConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex, FD3D12ResourceLocation* ResourceLocation, uint32 SizeInBytes);
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
