// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

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
	 * Commit the supplied command buffer.
	 * @param CommandBuffer The command buffer to commit, must be non-nil.
 	 */
	void CommitCommandBuffer(id<MTLCommandBuffer> const CommandBuffer);

#pragma mark - Public Command Queue Accessors -
	
	/** @returns The native command queue. */
	id<MTLCommandQueue> GetCommandQueue(void) const;

#pragma mark - Public Debug Support -

	/** Inserts a boundary that marks the end of a frame for the debug capture tool. */
	void InsertDebugCaptureBoundary(void);

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
};