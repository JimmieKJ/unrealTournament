// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#include "MetalCommandQueue.h"
#include "GPUProfiler.h"

#if METAL_STATISTICS
#include "MetalStatistics.h"
#endif

// Stats
DECLARE_CYCLE_STAT_EXTERN(TEXT("MakeDrawable time"),STAT_MetalMakeDrawableTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Draw call time"),STAT_MetalDrawCallTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PrepareDraw time"),STAT_MetalPrepareDrawTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PipelineState time"),STAT_MetalPipelineStateTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BoundShaderState time"),STAT_MetalBoundShaderStateTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("VertexDeclaration time"),STAT_MetalVertexDeclarationTime,STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Uniform buffer pool cleanup time"), STAT_MetalUniformBufferCleanupTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Buffer Page-Off time"), STAT_MetalBufferPageOffTime, STATGROUP_MetalRHI, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Texture Page-Off time"), STAT_MetalTexturePageOffTime, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Uniform buffer pool memory"), STAT_MetalTotalUniformBufferMemory, STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Free Uniform pool memory"), STAT_MetalFreeUniformBufferMemory, STATGROUP_MetalRHI, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Uniform buffer pool num free"), STAT_MetalNumFreeUniformBuffers, STATGROUP_MetalRHI, );
#if PLATFORM_MAC
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Device Utilization %"),STAT_MetalDeviceUtilisation,STATGROUP_MetalRHI, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Device Utilization % at cur p-state"),STAT_MetalDeviceUtilisationAtPState,STATGROUP_MetalRHI, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Device Unit 0 Utilization %"),STAT_MetalDevice0Utilisation,STATGROUP_MetalRHI, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Device Unit 1 Utilization %"),STAT_MetalDevice1Utilisation,STATGROUP_MetalRHI, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Device Unit 2 Utilization %"),STAT_MetalDevice2Utilisation,STATGROUP_MetalRHI, );
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Device Unit 3 Utilization %"),STAT_MetalDevice3Utilisation,STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("VRAM Used Bytes"),STAT_MetalVRAMUsedBytes,STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("VRAM Free Bytes"),STAT_MetalVRAMFreeBytes,STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("VRAM Largest Free Bytes"),STAT_MetalVRAMLargestFreeBytes,STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("In Use Vid Mem Bytes"),STAT_MetalInUseVidMemBytes,STATGROUP_MetalRHI, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("In Use Sys Mem Bytes"),STAT_MetalInUseSysMemBytes,STATGROUP_MetalRHI, );
#endif

#if METAL_STATISTICS
#define RHI_PROFILE_DRAW_CALL_STATS(StartPoint, EndPoint, NumPrims, NumVerts) FMetalDrawProfiler GPUWork(Profiler, (uint32)StartPoint, (uint32)EndPoint, NumPrims, NumVerts)
#else
#define RHI_PROFILE_DRAW_CALL_STATS(StartPoint, EndPoint, NumPrims, NumVerts) FMetalDrawProfiler GPUWork(Profiler, (uint32)0, (uint32)0, NumPrims, NumVerts)
#endif

class FMetalDrawProfiler
{
public:
	FMetalDrawProfiler(struct FMetalGPUProfiler* InProfiler, uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives = 0, uint32 NumVertices = 0);
	~FMetalDrawProfiler();
	
private:
	struct FMetalGPUProfiler* Profiler;
	uint32 StartPoint;
	uint32 EndPoint;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FMetalEventNode : public FGPUProfilerEventNode
{
public:
	
	FMetalEventNode(FMetalContext* InContext, const TCHAR* InName, FGPUProfilerEventNode* InParent, bool bIsRoot, bool bInFullProfiling)
	: FGPUProfilerEventNode(InName, InParent)
	, Context(InContext)
	, StartTime(0)
	, EndTime(0)
	, bRoot(bIsRoot)
    , bFullProfiling(bInFullProfiling)
	{
	}
	
	virtual ~FMetalEventNode();
	
	/**
	 * Returns the time in ms that the GPU spent in this draw event.
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override;
	
	virtual void StartTiming() override;
	
	virtual void StopTiming() override;
	
	void Start(id<MTLCommandBuffer> Buffer);
	void Stop(id<MTLCommandBuffer> Buffer);

	bool Wait() const { return bRoot && bFullProfiling; }
	bool IsRoot() const { return bRoot; }
	
	uint64 GetCycles() { return EndTime - StartTime; }
	
#if METAL_STATISTICS
	void StartDraw(bool bActiveStats, uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives, uint32 NumVertices);
	void StopDraw(void);
	
	void GetStats(FMetalPipelineStats& OutStats);
#endif
	
private:
	FMetalContext* Context;
#if METAL_STATISTICS
	TArray<IMetalDrawStats*> DrawStats;
#endif
	uint64 StartTime;
	uint64 EndTime;
	bool bRoot;
    bool bFullProfiling;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FMetalEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:
	FMetalEventNodeFrame(FMetalContext* InContext, bool bInFullProfiling)
	: RootNode(new FMetalEventNode(InContext, TEXT("Frame"), nullptr, true, bInFullProfiling))
    , bFullProfiling(bInFullProfiling)
	{
	}
	
	virtual ~FMetalEventNodeFrame()
	{
        if(bFullProfiling)
        {
            delete RootNode;
        }
	}
	
	/** Start this frame of per tracking */
	virtual void StartFrame() override;
	
	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override;
	
	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;
	
	virtual void LogDisjointQuery() override;
	
	FMetalEventNode* RootNode;
    bool bFullProfiling;
};

// This class has multiple inheritance but really FGPUTiming is a static class
class FMetalGPUTiming : public FGPUTiming
{
public:
	
	/**
	 * Constructor.
	 */
	FMetalGPUTiming()
	{
		StaticInitialize(nullptr, PlatformStaticInitialize);
	}
	
	
private:
	
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData)
	{
		// Are the static variables initialized?
		if ( !GAreGlobalsInitialized )
		{
			GIsSupported = true;
			GTimingFrequency = 1000 * 1000 * 1000;
			GAreGlobalsInitialized = true;
		}
	}
};

/**
 * Encapsulates GPU profiling logic and data.
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FMetalGPUProfiler : public FGPUProfiler
{
	/** GPU hitch profile histories */
	TIndirectArray<FMetalEventNodeFrame> GPUHitchEventNodeFrames;
	
	FMetalGPUProfiler(FMetalContext* InContext)
	:	FGPUProfiler()
	,	Context(InContext)
    ,   NumNestedFrames(0)
    ,	bActiveStats(false)
	{
	}
	
	virtual ~FMetalGPUProfiler() {}
	
	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override;
	
	void Cleanup();
	
	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;
	
	void BeginFrame();
	void EndFrame();
	
	void StartGPUWork(uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives = 0, uint32 NumVertices = 0);
	void FinishGPUWork(void);
	
	FMetalGPUTiming TimingSupport;
	FMetalContext* Context;
    int32 NumNestedFrames;
    bool bActiveStats;
};

