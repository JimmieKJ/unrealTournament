// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchChunk.h: Declares classes involved with chunks for the build system.
=============================================================================*/

#pragma once

#include "CoreUObject.h"

#include "Generation/StatsCollector.h"

#include "BuildPatchChunk.generated.h"

using namespace BuildPatchServices;
/**
 * A UStruct wrapping SHA1 hash data for serialization
 */
USTRUCT()
struct FSHAHashData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint8 Hash[FSHA1::DigestSize];

	FSHAHashData();

	bool operator==(const FSHAHashData& Other) const;
	bool operator!=(const FSHAHashData& Other) const;
	FString ToString() const;
	bool isZero() const;
};

static_assert(FSHA1::DigestSize == 20, "If this changes a lot of stuff here will break!");

/**
 * Constant values and typedefs
 */
namespace FBuildPatchData
{
	enum Value
	{
		// Sizes
		ChunkDataSize		= 1024*1024,	// We are using 1MB chunks for patching
		ChunkQueueSize		= 50,			// The number of chunks to allow to be queued up when saving out
	};

	enum Type
	{
		// Data Types
		ChunkData			= 0,
		FileData			= 1,
	};
}

/**
 * Declares a struct to store the info for a chunk header
 */
struct FChunkHeader
{
	enum 
	{
		// Storage flags, can be raw or a combination of others.
		STORED_RAW			=	0x0, // Zero means raw data
		STORED_COMPRESSED	=	0x1, // Flag for compressed
		STORED_ENCRYPTED	=	0x2, // Flag for encrypted

		// Hash types
		HASH_ROLLING		=	0x1, // Uses FRollingHash class
		HASH_SHA1			=	0x2, // Uses FSHA class
	};
	// Magic value used as a check for correct file
	uint32 Magic;
	// Version of this chunk file/header combo
	uint32 Version;
	// The size of this header
	uint32 HeaderSize;
	// The size of this data
	uint32 DataSize;
	// The GUID for this data
	FGuid Guid;
	// What type of hash we are using
	uint8 HashType;
	// The ChunkHash for this chunk
	uint64 RollingHash;
	// The FileHash for this chunk
	FSHAHashData SHAHash;
	// How the chunk data is stored
	uint8 StoredAs;

	/**
	 * Default Constructor sets the magic and version ready for writing out
	 */
	FChunkHeader();

	/**
	 * Checks if the Header Magic was set to the correct value
	 * @return	true if the header magic value is correct
	 */
	const bool IsValidMagic() const;

	/**
	 * Serialization operator.
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator<< (FArchive& Ar, FChunkHeader& Header);
};

/**
 * Declares a struct to hold a chunk header with data
 */
class FChunkFile
{
	friend class FChunkWriter;
private:
	// The header for the chunk
	FChunkHeader ChunkHeader;

	// The data for the chunk
	uint8 ChunkData[ FBuildPatchData::ChunkDataSize ];

	// The last time the chunk was accessed - might not need
	volatile double LastAccessTime;

	// Whether the file came from disk.
	// If it came from memory, it will need to be saved to disk if it is being forced out
	bool bIsFromDisk;

	// The number of times tis chunk is still referenced by the installing build
	volatile uint32 ReferenceCount;

	// The thread safety data lock
	FCriticalSection ThreadLock;

public:
	// Must be allocated with ref count to start with
	FChunkFile( const uint32& InReferenceCount, const bool& bInIsFromDisk );

	/**
	 * Gets the thread lock on the data, must call ReleaseDataLock when finished with data
	 * @param	OutChunkData	Receives the pointer to chunk data
	 * @param	OutChunkHeader	Receives the pointer to header
	 */
	void GetDataLock( uint8** OutChunkData, FChunkHeader** OutChunkHeader );

	/**
	 * Releases access to the data to allow other threads to use
	 */
	void ReleaseDataLock();

	/**
	 * Gets the current reference count for this chunk
	 * @return	The ref count.
	 */
	const uint32 GetRefCount() const;

	/**
	 * Decrements the reference count for this chunk
	 * @return	The new ref count.
	 */
	uint32 Dereference();

	/**
	 * Gets the last access time for this chunk
	 * @return	The last access time that was set from FPlatformTime::Seconds().
	 */
	const double GetLastAccessTime() const;

private:
	// Hide default constructors as they must not be used
	FChunkFile(){}
	FChunkFile(const FChunkFile&){}
};

/**
 * Declares a delegate for the chunk writer that gets called whenever a chunk has completed saving out
 * @param Param1	The GUID of the completed chunk
 * @param Param2	The full path to the chunk file
 */
DECLARE_MULTICAST_DELEGATE_TwoParams( FOnChunkComplete, const FGuid&, const FString& );

/**
 * Declares threaded chunk writer class for queuing up chunk file saving
 */
class FChunkWriter
{
private:
	// A private runnable class that writes out data
	class FQueuedChunkWriter : public FRunnable
	{
	public:
		// The directory that we want to save chunks to
		FString ChunkDirectory;

		// The stats collector for stat output
		FStatsCollectorPtr StatsCollector;

		// Default Constructor
		FQueuedChunkWriter();

		// Default Destructor
		~FQueuedChunkWriter();

		// Start FRunnable interface
		virtual bool Init() override;
		virtual uint32 Run() override;
		// End FRunnable interface

		/**
		 * Add a complete chunk to the queue.
		 * BLOCKS RETURN until space in the queue
		 * @param	ChunkData		The pointer to the chunk data. Should be of FBuildPatchData::ChunkDataSize length
		 * @param	ChunkGuid		The GUID of this chunk
		 * @param	ChunkHash		The hash value of this chunk
		 */
		void QueueChunk( const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash );

		/**
		 * Call to flag as no more chunks to come (the thread will finish up and then exit).
		 */
		void SetNoMoreChunks();

		/**
		 * Retrieves the list of chunk files sizes
		 * @param	OutChunkFileSizes		The Map is emptied and then filled with the list
		 */
		void GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes);

	private:
		// The queue of chunks to save
		TArray< FChunkFile* > ChunkFileQueue;

		// A critical section for accessing ChunkFileQueue.
		FCriticalSection ChunkFileQueueCS;

		// Store whether more chunks are coming
		bool bMoreChunks;

		// A critical section for accessing ChunkFileQueue.
		FCriticalSection MoreChunksCS;

		// Store chunk file sizes.
		TMap<FGuid, int64> ChunkFileSizes;

		// A critical section for accessing ChunkFileSizes.
		FCriticalSection ChunkFileSizesCS;

		// Atmoic statistics
		volatile int64* StatFileCreateTime;
		volatile int64* StatCheckExistsTime;
		volatile int64* StatSerlialiseTime;
		volatile int64* StatChunksSaved;
		volatile int64* StatCompressTime;
		volatile int64* StatDataWritten;
		volatile int64* StatDataWriteSpeed;
		volatile int64* StatCompressionRatio;

		/**
		 * Called from within run to save out a chunk file.
		 * @param	ChunkFilename	The chunk filename
		 * @param	ChunkFile		File data to be written out. Containing header will be edited.
		 * @param	ChunkGuid		The GUID of this chunk.
		 * @return	true if no file error
		 */
		const bool WriteChunkData(const FString& ChunkFilename, FChunkFile* ChunkFile, const FGuid& ChunkGuid);

		/**
		 * Thread safe. Checks to see if there are any chunks in the queue, or if there are more chunks expected.
		 * @return	true if we should keep looping
		 */
		const bool ShouldBeRunning();

		/**
		 * Thread safe. Checks to see if there are any chunks in the queue
		 * @return	true if ChunkFileQueue is not empty
		 */
		const bool HasQueuedChunk();

		/**
		 * Thread safe. Checks to see if there is space to queue a new chunk
		 * @return	true if a chunk can be queued
		 */
		const bool CanQueueChunk();

		/**
		 * Thread safe. Gets the next chunk from the chunk queue
		 * @return	The next chunk file
		 */
		FChunkFile* GetNextChunk();

	} QueuedChunkWriter;

public:
	/**
	 * Constructor
	 */
	FChunkWriter(const FString& ChunkDirectory, FStatsCollectorRef StatsCollector);

	/**
	 * Default destructor
	 */
	~FChunkWriter();

	/**
	 * Add a complete chunk to the queue. 
	 * BLOCKS RETURN until space in the queue
	 * @param	ChunkData		The pointer to the chunk data. Should be of FBuildPatchData::ChunkDataSize length
	 * @param	ChunkGuid		The GUID of this chunk
	 * @param	ChunkHash		The hash value of this chunk
	 */
	void QueueChunk( const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash );

	/**
	 * Call when there are no more chunks. The thread will be stopped and data cleaned up.
	 */
	void NoMoreChunks();

	/**
	 * Will only return when the writer thread has finished.
	 */
	void WaitForThread();

	/**
	 * Retrieves the list of chunk files sizes from the chunk writer
	 * @param	OutChunkFileSizes		The Map is emptied and then filled with the list
	 */
	void GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes);

private:
	/**
	 * Hide the default constructor so that the params must be given
	 */
	FChunkWriter(){};

	// Holds a pointer to our thread for the runnable
	FRunnableThread* WriterThread;
};

