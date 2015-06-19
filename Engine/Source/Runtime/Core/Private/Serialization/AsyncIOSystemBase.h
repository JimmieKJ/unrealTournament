// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncIOSystemBase.h: Base implementation of the async IO system
=============================================================================*/

#pragma once
#include "HAL/IOBase.h"

/**
 * Base implementation of an async IO system allowing most of the code to be shared across platforms.
 */
struct CORE_API FAsyncIOSystemBase : public FIOSystem, FRunnable, FSingleThreadRunnable
{
	/** 
	 * Constructor
	 * @param InLowLevel	Low level file system to use to satisfy requests.
	**/
	FAsyncIOSystemBase(IPlatformFile& InLowLevel)
		: LowLevel(InLowLevel)
	{
	}

	/**
	 * Requests data to be loaded async. Returns immediately.
	 *
	 * @param	FileName	File name to load
	 * @param	Offset		Offset into file
	 * @param	Size		Size of load request
	 * @param	Dest		Pointer to load data into
	 * @param	Counter		Thread safe counter to decrement when loading has finished
	 * @param	Priority	Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual uint64 LoadData( 
		const FString& FileName, 
		int64 Offset, 
		int64 Size, 
		void* Dest, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority ) override;

	/**
	 * Requests compressed data to be loaded async. Returns immediately.
	 *
	 * @param	FileName			File name to load
	 * @param	Offset				Offset into file
	 * @param	Size				Size of load request
	 * @param	UncompressedSize	Size of uncompressed data
	 * @param	Dest				Pointer to load data into
	 * @param	CompressionFlags	Flags controlling data decompression
	 * @param	Counter				Thread safe counter to decrement when loading has finished, can be NULL
	 * @param	Priority			Priority of request
	 *
	 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
	 */
	virtual uint64 LoadCompressedData( 
		const FString& FileName, 
		int64 Offset, 
		int64 Size, 
		int64 UncompressedSize, 
		void* Dest, 
		ECompressionFlags CompressionFlags, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority ) override;

	/**
	 * Removes N outstanding requests from the queue and returns how many were canceled. We can't cancel
	 * requests currently fulfilled and ones that have already been fulfilled.
	 *
	 * @param	RequestIndices	Indices of requests to cancel.
	 * @return	The number of requests that were canceled
	 */
	virtual int32 CancelRequests( uint64* RequestIndices, int32 NumIndices ) override;
	
	/**
	 * Removes all outstanding requests from the queue
	 */
	virtual void CancelAllOutstandingRequests() override;
	
	/**
	 * Blocks till all requests are finished and also flushes potentially open handles.
	 */
	virtual void BlockTillAllRequestsFinishedAndFlushHandles() override;

	// FRunnable interface.

	/**
	 * Initializes critical section, event and other used variables. 
	 *
	 * This is called in the context of the thread object that aggregates 
	 * this, not the thread that passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init() override;

	/**
	 * Called in the context of the aggregating thread to perform cleanup.
	 */
	virtual void Exit() override;

	/**
	 * This is where all the actual loading is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return always 0
	 */
	virtual uint32 Run() override;
	
	/**
	 * This is called if a thread is requested to terminate early.
	 */
	virtual void Stop() override;
	
	/**
	 * This is called if a thread is requested to suspend its' IO activity
	 */
	virtual void Suspend() override;

	/**
	 * This is called if a thread is requested to resume its' IO activity
	 */
	virtual void Resume() override;

	/**
	 * Returns a pointer to the single threaded interface when mulithreading is disabled.
	 */
	virtual FSingleThreadRunnable* GetSingleThreadInterface() override
	{
		return this;
	}

	// FSingleThreadRunnable interface

	virtual void Tick() override;

	// FIOSystem Interface

	virtual void TickSingleThreaded() override;


	/**
	 * Sets the min priority of requests to fulfill. Lower priority requests will still be queued and
	 * start to be fulfilled once the min priority is lowered sufficiently. This is only a hint and
	 * implementations are free to ignore it.
	 *
	 * @param	MinPriority		Min priority of requests to fulfill
	 */
	virtual void SetMinPriority( EAsyncIOPriority MinPriority ) override;

	/**
	 * Give the IO system a hint that it is done with the file for now
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void HintDoneWithFile(const FString& Filename) override;

	/**
	 * The minimum read size...used to be DVD_ECC_BLOCK_SIZE
	 *
	 * @return Minimum read size
	 */
	virtual int64 MinimumReadSize() override;

protected:

	/**
	 * Helper structure encapsulating all required cached data for an async IO request.
	 */
	struct FAsyncIORequest
	{
		/** Index of request.																		*/
		uint64				RequestIndex;
		/** File sort key on media, INDEX_NONE if not supported or unknown.							*/
		int32					FileSortKey;

		/** Name of file.																			*/
		FString				FileName;
		/** Hash of the name of the file. This avoids the need to do a string comparison.			*/
		uint32				FileNameHash;
		/** Offset into file.																		*/
		int64				Offset;
		/** Size in bytes of data to read.															*/
		int64				Size;
		/** Uncompressed size in bytes of original data, 0 if data is not compressed on disc		*/
		int64				UncompressedSize;														
		/** Pointer to memory region used to read data into.										*/
		void*				Dest;
		/** Flags for controlling decompression														*/
		ECompressionFlags	CompressionFlags;
		/** Thread safe counter that is decremented once work is done.								*/
		FThreadSafeCounter* Counter;
		/** Priority of request.																	*/
		EAsyncIOPriority	Priority;
		/** Is this a request to destroy the handle?												*/
		uint32			bIsDestroyHandleRequest : 1;
		/** Whether we already requested the handle to be cached.									*/
		uint32			bHasAlreadyRequestedHandleToBeCached : 1;

		/** Constructor, initializing all member variables. */
		FAsyncIORequest()
		:	RequestIndex(0)
		,	FileSortKey(-1)
		,	Offset(-1)
		,	Size(-1)
		,	UncompressedSize(-1)
		,	Dest(NULL)
		,	CompressionFlags(COMPRESS_None)
		,	Counter(NULL)
		,	Priority(AIOP_MIN)
		,	bIsDestroyHandleRequest(false)
		, bHasAlreadyRequestedHandleToBeCached(false)
		{}

		/**
		 * @returns human readable string with information about request
		 */
		FString ToString() const
		{
			return FString::Printf(TEXT("%11.1f, %10d, %12lld, %12lld, %12lld, 0x%p, 0x%08x, 0x%08x, %d, %s"),
				(double)RequestIndex, FileSortKey, Offset, Size, UncompressedSize, Dest, (uint32)CompressionFlags,
				(uint32)Priority, bIsDestroyHandleRequest ? 1 : 0, *FileName);
		}
	};

	/** 
	 * Implements shared stats handling and passes read to PlatformReadDoNotCallDirectly
	 *
	 * @param	FileHandle	Platform specific file handle
	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 */	
	void InternalRead( IFileHandle* FileHandle, int64 Offset, int64 Size, void* Dest );


	/** 
	 * Pure virtual of platform specific read functionality that needs to be implemented by
	 * derived classes. Should only be called from InternalRead.
	 *
	 * @param	FileHandle	Platform specific file handle
	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 * @return	true if read was successful, false otherwise
	 */
	virtual bool PlatformReadDoNotCallDirectly( IFileHandle* FileHandle, int64 Offset, int64 Size, void* Dest );

	/** 
	 * Pure virtual of platform specific file handle creation functionality that needs to be 
	 * implemented by derived classes.
	 *
	 * @param	FileName	Pathname to file
	 *
	 * @return	Platform specific value/ handle to file that is later on used; use 
	 *			IsHandleValid to check for errors.
	 */
	virtual IFileHandle* PlatformCreateHandle( const TCHAR* FileName );

	/**
	 * This is made platform specific to allow ordering of read requests based on layout of files
	 * on the physical media. The base implementation is FIFO while taking priority into account
	 *
	 * This function is being called while there is a scope lock on the critical section so it
	 * needs to be fast in order to not block QueueIORequest and the likes.
	 *
	 * @return	index of next to be fulfilled request or INDEX_NONE if there is none
	 */
	virtual int32 PlatformGetNextRequestIndex();

	/**
	 * Let the platform handle being done with the file
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void PlatformHandleHintDoneWithFile(const FString& Filename);

	/**
	 * The minimum read size...used to be DVD_ECC_BLOCK_SIZE
	 *
	 * @return Minimum read size
	 */
	virtual int64 PlatformMinimumReadSize();

	/**
	 * Fulfills a compressed read request in a blocking fashion by falling back to using
	 * PlatformSeek and various PlatformReads in combination with FAsyncUncompress to allow
	 * decompression while we are waiting for file I/O.
	 *
	 * @param	IORequest	IO request to fulfill
	 * @param	FileHandle	File handle to use
	 */
	void FulfillCompressedRead( const FAsyncIORequest& IORequest, IFileHandle* FileHandle );

	/**
	 * Retrieves cached file handle or caches it if it hasn't been already
	 *
	 * @param	FileName	file name to retrieve cached handle for
	 * @return	cached file handle
	 */
	IFileHandle* GetCachedFileHandle( const FString& FileName );

	/**
	 * Returns cached file handle if found, or NULL if not. This function does
	 * NOT create any file handles and therefore is not blocking.
	 *
	 * @param	FileName	file name to retrieve cached handle for
	 * @return	cached file handle, NULL if not cached
	 */
	IFileHandle* FindCachedFileHandle( const FString& FileName );

	/**
	 * Returns cached file handle if found, or NULL if not. This function does
	 * NOT create any file handles and therefore is not blocking.
	 *
	 * @param	FileNameHash	hash of the file name to retrieve cached handle for
	 * @return	cached file handle, NULL if not cached
	 */
	IFileHandle* FindCachedFileHandle(const uint32 FileNameHash);

	/**
	 * Flushes all file handles.
	 */
	void FlushHandles();

	/**
	 * Blocks till all requests are finished.
	 *
	 * @todo streaming: this needs to adjusted to signal the thread to not accept any new requests from other 
	 * @todo streaming: threads while we are blocking till all requests are finished.
	 */
	void BlockTillAllRequestsFinished();

	/**
	 * Constrains bandwidth to value set in .ini
	 *
	 * @param BytesRead		Number of bytes read
	 * @param ReadTime		Time it took to read
	 */
	void ConstrainBandwidth( int64 BytesRead, float ReadTime );

	/**
	 * Packs IO request into a FAsyncIORequest and queues it in OutstandingRequests array
	 *
	 * @param	FileName			File name associated with request
	 * @param	Offset				Offset in bytes from beginning of file to start reading from
	 * @param	Size				Number of bytes to read
	 * @param	UncompressedSize	Uncompressed size in bytes of original data, 0 if data is not compressed on disc	
	 * @param	Dest				Pointer holding to be read data
	 * @param	CompressionFlags	Flags controlling data decompression
	 * @param	Counter				Thread safe counter associated with this request; will be decremented when fulfilled
	 * @param	Priority			Priority of request
	 * 
	 * @return	unique ID for request
	*/
	uint64 QueueIORequest( 
		const FString& FileName, 
		int64 Offset, 
		int64 Size, 
		int64 UncompressedSize, 
		void* Dest, 
		ECompressionFlags CompressionFlags, 
		FThreadSafeCounter* Counter,
		EAsyncIOPriority Priority );

	/**
	 * Adds a destroy handle request top the OutstandingRequests array
	 * 
	 * @param	FileName			File name associated with request
	 *
	 * @return	unique ID for request
	 */
	uint64 QueueDestroyHandleRequest(const FString& Filename);

	/** 
	 *	Logs out the given file IO information w/ the given message.
	 *	
	 *	@param	Message		The message to prepend
	 *	@param	IORequest	The IORequest to log
	 */
	void LogIORequest(const FString& Message, const FAsyncIORequest& IORequest);

	/** Critical section used to synchronize access to outstanding requests map						*/
	FCriticalSection*				CriticalSection;
	/** TMap of file name string hash to file handles												*/
	TMap<uint32, IFileHandle*>		NameHashToHandleMap;
	/** Array of outstanding requests, processed in FIFO order										*/
	TArray<FAsyncIORequest>			OutstandingRequests;
	/** Event that is signaled if there are outstanding requests									*/
	FEvent*							OutstandingRequestsEvent;
	/** Thread safe counter that is 1 if the thread is currently busy with request, 0 otherwise		*/
	FThreadSafeCounter				BusyWithRequest;
	/** Thread safe counter that is 1 if the thread is available to process requests, 0 otherwise	*/
	FThreadSafeCounter				IsRunning;
	/** Current request index. We don't really worry about wrapping around with a uint64				*/
	uint64							RequestIndex;
	/** Counter to indicate that the application requested that IO should be suspended				*/
	FThreadSafeCounter				SuspendCount;
	/** Critical section to sequence IO when needed (in addition to SuspendCount).					*/
	FCriticalSection*				ExclusiveReadCriticalSection;
	/* Min priority of requests to fulfill.															*/
	EAsyncIOPriority				MinPriority;
	/** Low level file system that we use for our requests.											*/
	IPlatformFile&					LowLevel;
};
