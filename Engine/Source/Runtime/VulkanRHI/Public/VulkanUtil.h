// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUtil.h: Vulkan Utility definitions.
=============================================================================*/

#pragma once

#include "GPUProfiler.h"

class FVulkanGPUTiming : public FGPUTiming
{
public:
	FVulkanGPUTiming() :
		StartTimestamp(0),
		EndTimestamp(0),
		bIsTiming(false),
		bEndTimestampIssued(false)
	{}

	/**
	 * Start a GPU timing measurement.
	 */
	void StartTiming();

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void EndTiming();

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64 GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	 * Initializes all Vulkan resources.
	 */
	void Initialize();

	/**
	 * Releases all Vulkan resources.
	 */
	void Release();

	bool IsComplete() const
	{
		check(bEndTimestampIssued);
		//return *((uint64*)EndTimestamp.GetPointer()) != 0;
		return true;
	}

private:
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** Timestamps for all StartTimings. */
	int32 StartTimestamp;
	/** Timestamps for all EndTimings. */
	int32 EndTimestamp;

	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool bIsTiming;
	bool bEndTimestampIssued;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FVulkanEventNode : public FGPUProfilerEventNode
{
public:
	FVulkanEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) :
		FGPUProfilerEventNode(InName, InParent)
	{
		// Initialize Buffered timestamp queries 
		Timing.Initialize();
	}

	virtual ~FVulkanEventNode()
	{
		Timing.Release(); 
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override final;


	virtual void StartTiming() override final
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override final
	{
		Timing.EndTiming();
	}

	FVulkanGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FVulkanEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:

	FVulkanEventNodeFrame()
	{
		RootEventTiming.Initialize();
	}

	~FVulkanEventNodeFrame()
	{
		RootEventTiming.Release();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override final;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override final;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override final;

	virtual bool PlatformDisablesVSync() const { return true; }

	/** Timer tracking inclusive time spent in the root nodes. */
	FVulkanGPUTiming RootEventTiming;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FVulkanGPUProfiler : public FGPUProfiler
{
	/** GPU hitch profile histories */
	TIndirectArray<FVulkanEventNodeFrame> GPUHitchEventNodeFrames;

	FVulkanGPUProfiler() :
		FGPUProfiler(), 
		bCommandlistSubmitted(false)
	{
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override final
	{
		FVulkanEventNode* EventNode = new FVulkanEventNode(InName, InParent);
		return EventNode;
	}

	virtual void PushEvent(const TCHAR* Name, FColor Color) override final;
	virtual void PopEvent() override final;

	void BeginFrame(class FVulkanCommandListContext* InCmdList, class FVulkanTimestampQueryPool* InTimestampQueryPool);

	void EndFrameBeforeSubmit();
	void EndFrame();

	bool bCommandlistSubmitted;
};

namespace VulkanRHI
{
	/**
	 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	 * @param	Result - The result code to check
	 * @param	Code - The code which yielded the result.
	 * @param	VkFunction - Tested function name.
	 * @param	Filename - The filename of the source file containing Code.
	 * @param	Line - The line number of Code within Filename.
	 */
	void VerifyVulkanResult(VkResult Result, const ANSICHAR* VkFuntion, const ANSICHAR* Filename, uint32 Line);

	VkBuffer CreateBuffer(FVulkanDevice* InDevice, VkDeviceSize Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryRequirements& OutMemoryRequirements);
}

#define VERIFYVULKANRESULT(VkFunction)				{ const VkResult ScopedResult = VkFunction; if (ScopedResult != VK_SUCCESS) { VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); }}
#define VERIFYVULKANRESULT_EXPANDED(VkFunction)		{ const VkResult ScopedResult = VkFunction; if (ScopedResult < VK_SUCCESS) { VulkanRHI::VerifyVulkanResult(ScopedResult, #VkFunction, __FILE__, __LINE__); }}
