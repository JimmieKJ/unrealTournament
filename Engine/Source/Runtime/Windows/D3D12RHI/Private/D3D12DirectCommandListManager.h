#pragma once
#include "Core.h"
#include "RHI.h"
#include "Engine.h"
#include "Windows/D3D12RHIBasePrivate.h"
#define DEFAULT_MAIN_POOL_COMMAND_LISTS 3
#define MAX_ALLOCATED_COMMAND_LISTS 256

enum class CommandListState
{
	kOpen,
	kQueued,
	kFinished
};

enum EFenceType
{
	FT_CommandList,
	FT_Frame,
	FT_NumTypes
};

class FD3D12Fence
{
public:
	FD3D12Fence();
	~FD3D12Fence();

	void CreateFence(ID3D12Device* pDirect3DDevice, uint64 InitialValue = 0);
	uint64 Signal(ID3D12CommandQueue* pCommandQueue);
	bool IsFenceComplete(uint64 FenceValue);
	void WaitForFence(uint64 FenceValue);

	uint64 GetCurrentFence() const { return CurrentFence; }
	uint64 GetLastCompletedFence();

private:
	TRefCountPtr<ID3D12Fence> Fence;
	uint64 CurrentFence;
	uint64 LastCompletedFence;
	HANDLE hFenceCompleteEvent;
};

class FD3D12CommandAllocatorManager : public FD3D12DeviceChild
{
public:
	FD3D12CommandAllocatorManager(FD3D12Device* InParent, const D3D12_COMMAND_LIST_TYPE& InType)
		: FD3D12DeviceChild(InParent)
		, Type(InType)
	{}

	~FD3D12CommandAllocatorManager()
	{
		// Go through all command allocators owned by this manager and delete them.
		for (auto Iter = CommandAllocators.CreateIterator(); Iter; ++Iter)
		{
			FD3D12CommandAllocator* pCommandAllocator = *Iter;
			delete pCommandAllocator;
		}
	}

	FD3D12CommandAllocator* ObtainCommandAllocator();
	void ReleaseCommandAllocator(FD3D12CommandAllocator* CommandAllocator);

private:
	TArray<FD3D12CommandAllocator*> CommandAllocators;		// List of all command allocators owned by this manager
	TQueue<FD3D12CommandAllocator*> CommandAllocatorQueue;	// Queue of available allocators. Note they might still be in use by the GPU.
	FCriticalSection CS;	// Must be thread-safe because multiple threads can obtain/release command allocators
	D3D12_COMMAND_LIST_TYPE Type;
};

class FD3D12CommandListManager : public FD3D12DeviceChild
{
public:
	FD3D12CommandListManager(FD3D12Device* InParent, D3D12_COMMAND_LIST_TYPE CommandListType);
	~FD3D12CommandListManager();

	void Create(uint32 NumCommandLists = 0);
	void Destroy();

	inline bool IsReady()
	{
		return D3DCommandQueue.GetReference() != nullptr;
	}

	// This use to also take an optional PSO parameter so that we could pass this directly to Create/Reset command lists,
	// however this was removed as we generally can't actually predict what PSO we'll need until draw due to frequent
	// state changes. We leave PSOs to always be resolved in ApplyState().
	FD3D12CommandListHandle ObtainCommandList(FD3D12CommandAllocator& CommandAllocator);
	void ReleaseCommandList(FD3D12CommandListHandle& hList);

	void ExecuteCommandList(FD3D12CommandListHandle& hList, bool WaitForCompletion = false);
	void ExecuteCommandLists(TArray<FD3D12CommandListHandle>& Lists, bool WaitForCompletion = false);

	uint32 GetResourceBarrierCommandList(FD3D12CommandListHandle& hList, FD3D12CommandListHandle& hResourceBarrierList);

	void SignalFrameComplete(bool WaitForCompletion = false);

	CommandListState GetCommandListState(const FD3D12CLSyncPoint& hSyncPoint);
	bool IsComplete(const FD3D12CLSyncPoint& hSyncPoint, uint64 FenceOffset = 0);
	void WaitForCompletion(const FD3D12CLSyncPoint& hSyncPoint);

	inline HRESULT GetTimestampFrequency(uint64* Frequency)
	{
		return D3DCommandQueue->GetTimestampFrequency(Frequency);
	}

	inline ID3D12CommandQueue* GetD3DCommandQueue()
	{
		return D3DCommandQueue.GetReference();
	}

	inline FD3D12Fence& GetFence(EFenceType FenceType)
	{
		check(FenceType < FT_NumTypes);
		return Fences[FenceType];
	}

	void WaitForCommandQueueFlush();

private:
	// Returns signaled Fence
	uint64 ExecuteAndIncrementFence(ID3D12CommandList* pD3DCommandLists[], uint64 NumCommandLists, FD3D12Fence &Fence);
	FD3D12CommandListHandle CreateCommandListHandle(FD3D12CommandAllocator& CommandAllocator);

	TRefCountPtr<ID3D12CommandQueue>		D3DCommandQueue;

	FThreadsafeQueue<FD3D12CommandListHandle> ReadyLists;

	// Command allocators used exclusively for resource barrier command lists.
	FD3D12CommandAllocatorManager ResourceBarrierCommandAllocatorManager;
	FD3D12CommandAllocator* ResourceBarrierCommandAllocator;

	FD3D12Fence Fences[FT_NumTypes];

#if WINVER >= 0x0600 // Interlock...64 functions are only available from Vista onwards
	int64									NumCommandListsAllocated;
#else
	int32									NumCommandListsAllocated;
#endif
	D3D12_COMMAND_LIST_TYPE					CommandListType;
	FCriticalSection						ResourceStateCS;
	FCriticalSection						FenceCS;
};

