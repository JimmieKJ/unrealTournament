// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"

struct FCompressedChunk;

#if !USE_NEW_ASYNC_IO
/*----------------------------------------------------------------------------
	FArchiveAsync.
----------------------------------------------------------------------------*/

/**
 * Rough and basic version of async archive. The code relies on Serialize only ever to be called on the last
 * precached region.
 */
class COREUOBJECT_API FArchiveAsync final: public FArchive
{
public:
	/**
	 * Constructor, initializing member variables.
	 */
	FArchiveAsync( const TCHAR* InFileName );

	/**
	 * Virtual destructor cleaning up internal file reader.
	 */
	virtual ~FArchiveAsync();

	/**
	 * Close archive and return whether there has been an error.
	 *
	 * @return	true if there were NO errors, false otherwise
	 */
	virtual bool Close();

	/**
	 * Sets mapping from offsets/ sizes that are going to be used for seeking and serialization to what
	 * is actually stored on disk. If the archive supports dealing with compression in this way it is 
	 * going to return true.
	 *
	 * @param	CompressedChunks	Pointer to array containing information about [un]compressed chunks
	 * @param	CompressionFlags	Flags determining compression format associated with mapping
	 *
	 * @return true if archive supports translating offsets & uncompressing on read, false otherwise
	 */
	virtual bool SetCompressionMap( TArray<FCompressedChunk>* CompressedChunks, ECompressionFlags CompressionFlags );

	/**
	 * Hint the archive that the region starting at passed in offset and spanning the passed in size
	 * is going to be read soon and should be precached.
	 *
	 * The function returns whether the precache operation has completed or not which is an important
	 * hint for code knowing that it deals with potential async I/O. The archive is free to either not 
	 * implement this function or only partially precache so it is required that given sufficient time
	 * the function will return true. Archives not based on async I/O should always return true.
	 *
	 * This function will not change the current archive position.
	 *
	 * @param	PrecacheOffset	Offset at which to begin precaching.
	 * @param	PrecacheSize	Number of bytes to precache
	 * @return	false if precache operation is still pending, true otherwise
	 */
	virtual bool Precache( int64 PrecacheOffset, int64 PrecacheSize );

	/**
	 * Serializes data from archive.
	 *
	 * @param	Data	Pointer to serialize to
	 * @param	Num		Number of bytes to read
	 */
	virtual void Serialize( void* Data, int64 Num );

	/**
	 * Returns the current position in the archive as offset in bytes from the beginning.
	 *
	 * @return	Current position in the archive (offset in bytes from the beginning)
	 */
	virtual int64 Tell();
	/**
	 * Returns the total size of the archive in bytes.
	 *
	 * @return total size of the archive in bytes
	 */
	virtual int64 TotalSize();

	/**
	 * Sets the current position.
	 *
	 * @param InPos	New position (as offset from beginning in bytes)
	 */
	virtual void Seek( int64 InPos );

	/**
	 * Flushes cache and frees internal data.
	 */
	virtual void FlushCache();
private:

	/**
	 * Swaps current and next buffer. Relies on calling code to ensure that there are no outstanding
	 * async read operations into the buffers.
	 */
	void BufferSwitcheroo();

	/**
	 * Whether the current precache buffer contains the passed in request.
	 *
	 * @param	RequestOffset	Offset in bytes from start of file
	 * @param	RequestSize		Size in bytes requested
	 *
	 * @return true if buffer contains request, false othwerise
	 */
	FORCEINLINE bool PrecacheBufferContainsRequest( int64 RequestOffset, int64 RequestSize );

	/**
	 * Finds and returns the compressed chunk index associated with the passed in offset.
	 *
	 * @param	RequestOffset	Offset in file to find associated chunk index for
	 *
	 * @return Index into CompressedChunks array matching this offset
	 */
	int32 FindCompressedChunkIndex( int64 RequestOffset );

	/**
	 * Precaches compressed chunk of passed in index using buffer at passed in index.
	 *
	 * @param	ChunkIndex	Index of compressed chunk
	 * @param	BufferIndex	Index of buffer to precache into	
	 */
	void PrecacheCompressedChunk( int64 ChunkIndex, int64 BufferIndex );

	/** Anon enum used to index precache data. */
	enum
	{
		CURRENT = 0,
		NEXT	= 1,
	};

	/** Cached filename for debugging.												*/
	FString							FileName;
	/** Cached file size															*/
	int64							FileSize;
	/** Cached uncompressed file size (!= FileSize for compressed packages.			*/
	int64							UncompressedFileSize;
	/** BulkData area size ( size of the bulkdata are at the end of the file. That is
		not part of the CompressedChunks, as BulkData is compressed separately		*/
	int64							BulkDataAreaSize;
	/** Current position of archive.												*/
	int64							CurrentPos;

	/** Start position of current precache request.									*/
	int64							PrecacheStartPos[2];
	/** End position (exclusive) of current precache request.						*/
	int64							PrecacheEndPos[2];
	/** Buffer containing precached data.											*/
	uint8*							PrecacheBuffer[2];
	/** Precahce buffer size */
	SIZE_T PrecacheBufferSize[2];
	/** True if the precache buffer is protected */
	bool PrecacheBufferProtected[2];

	/** Status of pending read, a value of 0 means no outstanding reads.			*/
	FThreadSafeCounter				PrecacheReadStatus[2];
	
	/** Mapping of compressed <-> uncompresses sizes and offsets, NULL if not used.	*/
	TArray<FCompressedChunk>*		CompressedChunks;
	/** Current index into compressed chunks array.									*/
	int64							CurrentChunkIndex;
	/** Compression flags determining compression of CompressedChunks.				*/
	ECompressionFlags				CompressionFlags;
	/** Caches the return value of FPlatformMisc::SupportsMultithreading (comes up in profiles often) */
	bool PlatformIsSinglethreaded;
};

#else

COREUOBJECT_API FArchive* NewFArchiveAsync2(const TCHAR* InFileName);

COREUOBJECT_API void HintFutureReadDone(const TCHAR * FileName);

COREUOBJECT_API void HintFutureRead(const TCHAR * FileName);

#endif // USE_NEW_ASYNC_IO

