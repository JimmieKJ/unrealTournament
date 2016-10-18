// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderTargetPool.h: Scene render target pool manager.
=============================================================================*/

#pragma once

#include "RendererInterface.h"
#include "VisualizeTexture.h"

/** The reference to a pooled render target, use like this: TRefCountPtr<IPooledRenderTarget> */
struct FPooledRenderTarget : public IPooledRenderTarget
{
	FPooledRenderTarget(const FPooledRenderTargetDesc& InDesc) 
		: NumRefs(0)
		, UnusedForNFrames(0)
		, Desc(InDesc)
		, bSnapshot(false)
	{
	}
	/* Constructor that makes a snapshot */
	FPooledRenderTarget(const FPooledRenderTarget& SnaphotSource)
		: NumRefs(1)
		, UnusedForNFrames(0)
		, Desc(SnaphotSource.Desc)
		, bSnapshot(true)
	{
		check(IsInRenderingThread());
		RenderTargetItem = SnaphotSource.RenderTargetItem;
	}

	virtual ~FPooledRenderTarget()
	{
		check(!NumRefs || (bSnapshot && NumRefs == 1));
		RenderTargetItem.SafeRelease();
	}

	bool IsSnapshot() const 
	{ 
		return bSnapshot;
	}

	uint32 GetUnusedForNFrames() const 
	{ 
		check(!bSnapshot);
		return UnusedForNFrames; 
	}

	// interface IPooledRenderTarget --------------

	virtual uint32 AddRef() const override final;
	virtual uint32 Release() const override final;
	virtual uint32 GetRefCount() const override final;
	virtual bool IsFree() const override final;
	virtual void SetDebugName(const TCHAR *InName);
	virtual const FPooledRenderTargetDesc& GetDesc() const;
	virtual uint32 ComputeMemorySize() const;

// todo private:
	FVRamAllocation VRamAllocation;

private:

	/** For pool management (only if NumRef == 0 the element can be reused) */
	mutable int32 NumRefs;
	/** Allows to defer the release to save performance on some hardware (DirectX) */
	uint32 UnusedForNFrames;
	/** All necessary data to create the render target */
	FPooledRenderTargetDesc Desc;
	/** Snapshots are sortof fake pooled render targets, they don't own anything and can outlive the things that created them. These are for threaded rendering. */
	bool bSnapshot;

	/** @return true:release this one, false otherwise */
	bool OnFrameStart();

	friend class FRenderTargetPool;
};

enum ERenderTargetPoolEventType
{
	ERTPE_Alloc,
	ERTPE_Dealloc,
	ERTPE_Phase
};

struct FRenderTargetPoolEvent
{
	// constructor for ERTPE_Alloc
	FRenderTargetPoolEvent(uint32 InPoolEntryId, uint32 InTimeStep, FPooledRenderTarget* InPointer)
		: PoolEntryId(InPoolEntryId)
		, TimeStep(InTimeStep)
		, Pointer(InPointer)
		, EventType(ERTPE_Alloc)
		, ColumnIndex(-1)
		, ColumnX(0)
		, ColumnSize(0)
	{
		Desc = Pointer->GetDesc();
		SizeInBytes = Pointer->ComputeMemorySize();
		VRamAllocation = Pointer->VRamAllocation;
	}

	// constructor for ERTPE_Alloc
	FRenderTargetPoolEvent(uint32 InPoolEntryId, uint32 InTimeStep)
		: PoolEntryId(InPoolEntryId)
		, TimeStep(InTimeStep)
		, Pointer(0)
		, SizeInBytes(0)
		, EventType(ERTPE_Dealloc)
		, ColumnIndex(-1)
		, ColumnX(0)
		, ColumnSize(0)
	{
	}

	// constructor for ERTPE_Alloc
	// @param PhaseName must not be 0
	FRenderTargetPoolEvent(const FString& InPhaseName, uint32 InTimeStep)
		: PoolEntryId(-1)
		, TimeStep(InTimeStep)
		, Pointer(0)
		, PhaseName(InPhaseName)
		, SizeInBytes(0)
		, EventType(ERTPE_Phase)
		, ColumnIndex(-1)
		, ColumnX(0)
		, ColumnSize(0)
	{
	}

	// @return Pointer if the object is still in the pool
	IPooledRenderTarget* GetValidatedPointer() const;

	ERenderTargetPoolEventType GetEventType() const { return EventType; }

	const int32 GetPoolEntryId() const { check(EventType == ERTPE_Alloc || EventType == ERTPE_Dealloc); return PoolEntryId; }
	const FString& GetPhaseName() const { check(EventType == ERTPE_Phase); return PhaseName; }
	const FPooledRenderTargetDesc& GetDesc() const { check(EventType == ERTPE_Alloc || EventType == ERTPE_Dealloc); return Desc; }
	const uint32 GetTimeStep() const { return TimeStep; }
	const uint64 GetSizeInBytes() const { check(EventType == ERTPE_Alloc); return SizeInBytes; }
	const void SetPoolEntryId(uint32 InPoolEntryId) { PoolEntryId = InPoolEntryId; }
	const void SetColumn(uint32 InColumnIndex, uint32 InColumnX, uint32 InColumnSize) { check(EventType == ERTPE_Alloc || EventType == ERTPE_Dealloc); ColumnIndex = InColumnIndex; ColumnX = InColumnX; ColumnSize = InColumnSize; }
	const uint32 GetColumnX() const { check(EventType == ERTPE_Alloc || EventType == ERTPE_Dealloc); return ColumnX; }
	const uint32 GetColumnSize() const { check(EventType == ERTPE_Alloc || EventType == ERTPE_Dealloc); return ColumnSize; }
	bool IsVisible() const { return EventType == ERTPE_Phase || ColumnSize > 0; }

	void SetDesc(const FPooledRenderTargetDesc &InDesc) { Desc = InDesc; }

	bool NeedsDeallocEvent();

private:

	// valid if EventType==ERTPE_Alloc || EventType==ERTPE_Dealloc, -1 if not set, was index into PooledRenderTargets[]
	uint32 PoolEntryId;
	//
	uint32 TimeStep;
	// valid EventType==ERTPE_Alloc, 0 if not set
	FPooledRenderTarget* Pointer;
	//
	FVRamAllocation VRamAllocation;
	// valid if EventType==ERTPE_Phase TEXT("") if not set
	FString PhaseName;
	// valid if EventType==ERTPE_Alloc || EventType==ERTPE_Dealloc
	FPooledRenderTargetDesc Desc;
	// valid if EventType==ERTPE_Alloc 0 if unknown
	uint64 SizeInBytes;
	// e.g. ERTPE_Alloc
	ERenderTargetPoolEventType EventType;

	// for display, computed by ComputeView()

	// valid if EventType==ERTPE_Alloc || EventType==ERTPE_Dealloc, -1 if not defined yet
	uint32 ColumnIndex;
	//
	uint32 ColumnX;
	//
	uint32 ColumnSize;
};




/**
 * Encapsulates the render targets pools that allows easy sharing (mostly used on the render thread side)
 */
class FRenderTargetPool : public FRenderResource
{
public:
	FRenderTargetPool();

	/** Transitions all targets in the pool to writable. */
	void TransitionTargetsWritable(FRHICommandListImmediate& RHICmdList);

	/**
	 * @param DebugName must not be 0, we only store the pointer
	 * @param Out is not the return argument to avoid double allocation because of wrong reference counting
	 * call from RenderThread only
	 * @return true if the old element was still valid, false if a new one was assigned
	 */
	bool FindFreeElement(FRHICommandList& RHICmdList, const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget>& Out, const TCHAR* InDebugName, bool bDoWritableBarrier = true);

	void CreateUntrackedElement(const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget>& Out, const FSceneRenderTargetItem& Item);

	/** Destruct all snapshots, this must be done after all outstanding async tasks are done. It is important because they hold ref counted texture pointers etc **/
	IPooledRenderTarget* MakeSnapshot(const TRefCountPtr<IPooledRenderTarget>& In);

	/** Destruct all snapshots, this must be done after all outstanding async tasks are done. It is important because they hold ref counted texture pointers etc **/
	void DestructSnapshots();



	/** Only to get statistics on usage and free elements. Normally only called in renderthread or if FlushRenderingCommands was called() */
	void GetStats(uint32& OutWholeCount, uint32& OutWholePoolInKB, uint32& OutUsedInKB) const;
	/**
	 * Can release RT, should be called once per frame.
	 * call from RenderThread only
	 */
	void TickPoolElements();
	/** Free renderer resources */
	void ReleaseDynamicRHI();

	/** Allows to remove a resource so it cannot be shared and gets released immediately instead a/some frame[s] later. */
	void FreeUnusedResource(TRefCountPtr<IPooledRenderTarget>& In);

	/** Good to call between levels or before memory intense operations. */
	void FreeUnusedResources();

	// for debugging purpose, assumes you call FlushRenderingCommands() be
	// @return can be 0, that doesn't mean iteration is done
	FPooledRenderTarget* GetElementById(uint32 Id) const;

	uint32 GetElementCount() const { return PooledRenderTargets.Num(); }

	// @return -1 if not found
	int32 FindIndex(IPooledRenderTarget* In) const;

	// Render visualization
	void RenderVisualizeTexture(class FDeferredShadingSceneRenderer& Scene);

	void SetObserveTarget(const FString& InObservedDebugName, uint32 InObservedDebugNameReusedGoal = 0xffffffff);

	// Logs out usage information.
	void DumpMemoryUsage(FOutputDevice& OutputDevice);

	// to not have event recording for some time during rendering (e.g. thumbnail rendering)
	void SetEventRecordingActive(bool bValue) { bEventRecordingActive = bValue; }

	//
	void DisableEventDisplay() { RenderTargetPoolEvents.Empty(); bEventRecordingStarted = false; }
	//
	bool IsEventRecordingEnabled() const;

	void AddPhaseEvent(const TCHAR* InPhaseName);

	/** renders the VisualizeTextureContent to the current render target */
	void PresentContent(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);

	FVisualizeTexture VisualizeTexture;

private:

	friend void RenderTargetPoolEvents(const TArray<FString>& Args);

	/** Elements can be 0, we compact the buffer later. */
	TArray< TRefCountPtr<FPooledRenderTarget> > PooledRenderTargets;
	TArray< TRefCountPtr<FPooledRenderTarget> > DeferredDeleteArray;
	TArray< FTextureRHIParamRef > TransitionTargets;	

	/** These are snapshots, have odd life times, live in the scene allocator, and don't contibute to any accounting or other management. */
	TArray<FPooledRenderTarget*> PooledRenderTargetSnapshots;

	// redundant, can always be computed with GetStats(), to debug "out of memory" situations and used for r.RenderTargetPoolMin
	uint32 AllocationLevelInKB;

	FGraphEventRef TransitionFence;

	// to avoid log spam
	bool bCurrentlyOverBudget;

	// for debugging purpose
	void VerifyAllocationLevel() const;

	// could be done on the fly but that makes the RenderTargetPoolEvents harder to read
	void CompactPool();

	void WaitForTransitionFence();

	// the following is used for Event recording --------------------------------

	struct SMemoryStats
	{
		SMemoryStats()
			: DisplayedUsageInBytes(0)
			, TotalUsageInBytes(0)
			, TotalColumnSize(0)
		{
		}
		// for statistics
		uint64 DisplayedUsageInBytes;
		// for statistics
		uint64 TotalUsageInBytes;
		// for display purpose, to normalize the view width
		uint64 TotalColumnSize;
	};

	// if next frame we want to run with bEventRecording=true
	bool bStartEventRecordingNextTick;
	// in KB, e.g. 1MB = 1024, 0 to display all
	uint32 EventRecordingSizeThreshold;
	// true if active, to not have the event recording for some time during rendering (e.g. thumbnail rendering)
	bool bEventRecordingActive;
	// true meaning someone used r.RenderTargetPool.Events to start it
	bool bEventRecordingStarted;
	// only used if bEventRecording
	TArray<FRenderTargetPoolEvent> RenderTargetPoolEvents;
	//
	uint32 CurrentEventRecordingTime;

	//
	void AddDeallocEvents();

	// @param In must no be 0
	void AddAllocEvent(uint32 InPoolEntryId, FPooledRenderTarget* In);

	//
	void AddAllocEventsFromCurrentState();

	uint32 ComputeEventDisplayHeight();

	// @return 0 if none was found
	const FString* GetLastEventPhaseName();

	// sorted by size
	SMemoryStats ComputeView();

	// can be optimized to not be generated each time
	void GenerateVRamAllocationUsage(TArray<FVRamAllocation>& Out);

	friend struct FPooledRenderTarget;
};

/** The global render targets for easy shading. */
extern TGlobalResource<FRenderTargetPool> GRenderTargetPool;
