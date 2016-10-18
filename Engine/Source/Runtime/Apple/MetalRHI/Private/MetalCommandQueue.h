// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

class FMetalCommandList;

/**
 * FMetalCommandQueue:
 */
class FMetalCommandQueue
{
public:
#pragma mark - Public C++ Boilerplate -

	/**
	 * Constructor
	 * @param Device The Metal device to create on.
	 * @param MaxNumCommandBuffers The maximum number of incomplete command-buffers, defaults to 0 which implies the system default.
	 */
	FMetalCommandQueue(id<MTLDevice> Device, uint32 const MaxNumCommandBuffers = 0);
	
	/** Destructor */
	~FMetalCommandQueue(void);
	
#pragma mark - Public Command Buffer Mutators -

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 * @param CommandBuffer The new command buffer to begin encoding to.
	 */
	id<MTLCommandBuffer> CreateRetainedCommandBuffer(void);

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 * @param CommandBuffer The new command buffer to begin encoding to.
	 */
	id<MTLCommandBuffer> CreateUnretainedCommandBuffer(void);
	
	/**
	 * Commit the supplied command buffer immediately.
	 * @param CommandBuffer The command buffer to commit, must be non-nil.
 	 */
	void CommitCommandBuffer(id<MTLCommandBuffer> const CommandBuffer);
	
	/**
	 * Deferred contexts submit their internal lists of command-buffers out of order, the command-queue takes ownership and handles reordering them & lazily commits them once all command-buffer lists are submitted.
	 * @param BufferList The list of buffers to enqueue into the command-queue at the given index.
	 * @param Index The 0-based index to commit BufferList's contents into relative to other active deferred contexts.
	 * @param Count The total number of deferred contexts that will submit - only once all are submitted can any command-buffer be committed.
	 */
	void SubmitCommandBuffers(NSArray<id<MTLCommandBuffer>>* BufferList, uint32 Index, uint32 Count);

#pragma mark - Public Command Queue Accessors -
	
	/** @returns The command queue's native device. */
	id<MTLDevice> GetDevice(void) const;

#pragma mark - Public Debug Support -

	/** Inserts a boundary that marks the end of a frame for the debug capture tool. */
	void InsertDebugCaptureBoundary(void);
	
#if !UE_BUILD_SHIPPING
	/** Enable or disable runtime debugging features. */
	void SetRuntimeDebuggingLevel(int32 const Level);
	
	/** @returns The level of runtime debugging features enabled. */
	int32 GetRuntimeDebuggingLevel(void) const;
#endif

#if METAL_STATISTICS
#pragma mark - Public Statistics Extensions -

	/** @returns An object that provides Metal statistics information or nullptr. */
	class IMetalStatistics* GetStatistics(void);
#endif
	
private:
#pragma mark - Private Member Variables -
	id<MTLCommandQueue> CommandQueue;
#if METAL_STATISTICS
	class IMetalStatistics* Statistics;
#endif
	TArray<NSArray<id<MTLCommandBuffer>>*> CommandBuffers;
#if !UE_BUILD_SHIPPING
	int32 RuntimeDebuggingLevel;
#endif
};