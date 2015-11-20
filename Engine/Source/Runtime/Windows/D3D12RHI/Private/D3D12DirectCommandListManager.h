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
	bool IsFenceFinished(uint64 FenceValue);
	void WaitForFence(uint64 FenceValue);

	uint64 GetCurrentFence() const { return CurrentFence; }
	uint64 GetLastCompletedFence();

private:
	TRefCountPtr<ID3D12Fence> Fence;
	uint64 CurrentFence;
	uint64 LastCompletedFence;
	HANDLE hFenceCompleteEvent;
};

class FD3D12CommandListManager
{
public:
	FD3D12CommandListManager();
	~FD3D12CommandListManager();

	void Create(ID3D12Device* Direct3DDevice, D3D12_COMMAND_LIST_TYPE CommandListType, uint32 NumCommandLists = 16);
	void Destroy();

	inline bool IsReady()
	{
		return D3DCommandQueue.GetReference() != nullptr;
	}

	FD3D12CommandListHandle BeginCommandList(ID3D12PipelineState* InitialPSO = nullptr);
	void ExecuteCommandList(FD3D12CommandListHandle& hList, bool WaitForCompletion = false);
	void ExecuteCommandLists(TArray<FD3D12CommandListHandle>& Lists, bool WaitForCompletion = false);

	uint32 GetResourceBarrierCommandList(FD3D12CommandListHandle& hList, FD3D12CommandListHandle& hResourceBarrierList);

	void SignalFrameComplete(bool WaitForCompletion = false);

	CommandListState GetCommandListState(const FD3D12CLSyncPoint& hSyncPoint);
	bool IsFinished(const FD3D12CLSyncPoint& hSyncPoint, uint64 FenceOffset = 0);
	void WaitForCompletion(const FD3D12CLSyncPoint& hSyncPoint);

	bool IsFinished(const FD3D12FrameSyncPoint& hSyncPoint);
	void WaitForCompletion(const FD3D12FrameSyncPoint& hSyncPoint);

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
	FD3D12CommandListHandle CreateCommandListHandle();

	TRefCountPtr<ID3D12CommandQueue>		D3DCommandQueue;

	FThreadsafeQueue<FD3D12CommandListHandle> ReadyLists;

	FD3D12Fence Fences[FT_NumTypes];
	
#if WINVER >= 0x0600 // Interlock...64 functions are only available from Vista onwards
	int64									NumCommandListsAllocated;
#else
	int32									NumCommandListsAllocated;
#endif
	D3D12_COMMAND_LIST_TYPE					CommandListType;
	ID3D12Device*							Direct3DDevice;
	FCriticalSection						PeekReadyListCS;
	FCriticalSection						ResourceStateCS;
	FCriticalSection						FenceCS;
};

