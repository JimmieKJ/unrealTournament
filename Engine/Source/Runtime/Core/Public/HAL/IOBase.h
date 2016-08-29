// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/*-----------------------------------------------------------------------------
	FIOSystem.
-----------------------------------------------------------------------------*/

/**
 * Virtual base class of IO systems.
 */
struct CORE_API FIOSystem
{
	/** System singleton **/
	static FIOSystem& Get();

	/** Shutdown the async IO system **/
	static void Shutdown();

	/** Checks if the async IO system has already been shut down. **/
	static bool HasShutdown();

	virtual ~FIOSystem()
	{
	}
	
	/**
	 * Requests data to be loaded async. Returns immediately.
	 *
	 * @param	Filename	Filename to load
	 * @param	Offset		Offset into file
	 * @param	Size		Size of load request
	 * @param	Dest		Pointer to load data into
	 * @param	Counter		Thread safe counter to decrement when loading has finished, can be nullptr
	 * @param	Priority	Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual uint64 LoadData( 
		const FString& Filename, 
		int64 Offset, 
		int64 Size, 
		void* Dest, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority ) = 0;

	/**
	 * Requests compressed data to be loaded async. Returns immediately.
	 *
	 * @param	Filename			Filename to load
	 * @param	Offset				Offset into file
	 * @param	Size				Size of load request
	 * @param	UncompressedSize	Size of uncompressed data
	 * @param	Dest				Pointer to load data into
	 * @param	CompressionFlags	Flags controlling data decompression
	 * @param	Counter				Thread safe counter to decrement when loading has finished, can be nullptr
	 * @param	Priority			Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual uint64 LoadCompressedData( 
		const FString& Filename, 
		int64 Offset, 
		int64 Size, 
		int64 UncompressedSize, 
		void* Dest, 
		ECompressionFlags CompressionFlags, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority ) = 0;

	/**
	 * Removes N outstanding requests from the queue and returns how many were canceled. We can't cancel
	 * requests currently fulfilled and ones that have already been fulfilled.
	 *
	 * @param	RequestIndices	Indices of requests to cancel.
	 * @return	The number of requests that were canceled
	 */
	virtual int32 CancelRequests( uint64* RequestIndices, int32 NumIndices ) = 0;

	/**
	 * Removes all outstanding requests from the queue
	 */
	virtual void CancelAllOutstandingRequests() = 0;

	/**
	 * Blocks till all requests are finished and also flushes potentially open handles.
	 */
	virtual void BlockTillAllRequestsFinishedAndFlushHandles() = 0;

	/**
	 * Suspend any IO operations (can be called from another thread)
	 */
	virtual void Suspend() = 0;

	/**
	 * Resume IO operations (can be called from another thread)
	 */
	virtual void Resume() = 0;

	/**
	 * Force update async loading when multithreading is not supported.
	 */
	virtual void TickSingleThreaded() = 0;

	/**
	 * Sets the min priority of requests to fulfill. Lower priority requests will still be queued and
	 * start to be fulfilled once the min priority is lowered sufficiently. This is only a hint and
	 * implementations are free to ignore it.
	 *
	 * @param	MinPriority		Min priority of requests to fulfill
	 */
	virtual void SetMinPriority( EAsyncIOPriority MinPriority ) = 0;


	/**
	 * Give the IO system a hint that it is done with the file for now
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void HintDoneWithFile(const FString& Filename) = 0;

	/**
	 * The minimum read size...used to be DVD_ECC_BLOCK_SIZE
	 *
	 * @return Minimum read size
	 */
	virtual int64 MinimumReadSize() = 0;

	/**
	 * Flush the pending logs if any.
	 */
	virtual void FlushLog() = 0;
};

