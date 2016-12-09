// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

class FMetalCommandList;

/**
 * Enumeration of features which are present only on some OS/device combinations.
 * These have to be checked at runtime as well as compile time to ensure backward compatibility.
 */
enum EMetalFeatures
{
	/** Support for separate front & back stencil ref. values */
	EMetalFeaturesSeparateStencil = 1 << 0,
	/** Support for specifying an update to the buffer offset only */
	EMetalFeaturesSetBufferOffset = 1 << 1,
	/** Support for specifying the depth clip mode */
	EMetalFeaturesDepthClipMode = 1 << 2,
	/** Support for specifying resource usage & memory options */
	EMetalFeaturesResourceOptions = 1 << 3,
	/** Supports texture->buffer blit options for depth/stencil blitting */
	EMetalFeaturesDepthStencilBlitOptions = 1 << 4,
    /** Supports creating a native stencil texture view from a depth/stencil texture */
    EMetalFeaturesStencilView = 1 << 5,
    /** Supports a depth-16 pixel format */
    EMetalFeaturesDepth16 = 1 << 6,
	/** Supports NSUInteger counting visibility queries */
	EMetalFeaturesCountingQueries = 1 << 7,
	/** Supports base vertex/instance for draw calls */
	EMetalFeaturesBaseVertexInstance = 1 << 8,
	/** Supports indirect buffers for draw calls */
	EMetalFeaturesIndirectBuffer = 1 << 9,
	/** Supports layered rendering */
	EMetalFeaturesLayeredRendering = 1 << 10,
	/** Support for specifying small buffers as byte arrays */
	EMetalFeaturesSetBytes = 1 << 11
};

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
	
	/**
	 * @param InFeature A specific Metal feature to check for.
	 * @returns True if the requested feature is supported, else false.
	 */
	inline bool SupportsFeature(EMetalFeatures InFeature) { return ((Features & InFeature) != 0); }

#pragma mark - Public Debug Support -

	/** Inserts a boundary that marks the end of a frame for the debug capture tool. */
	void InsertDebugCaptureBoundary(void);
	
#if METAL_DEBUG_OPTIONS
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
#if METAL_DEBUG_OPTIONS
	int32 RuntimeDebuggingLevel;
#endif
	uint32 Features;
};
