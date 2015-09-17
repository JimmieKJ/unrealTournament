// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchGeneration.h: Declares classes involved with producing build data.
=============================================================================*/

#pragma once

#if WITH_BUILDPATCHGENERATION

/**
 * A class that controls processing of build data to produce the manifest with files
 */
class FBuildDataFileProcessor
{
private:

	/**
	 * Make the default constructor private so that we must have the required params passed in.
	 */
	FBuildDataFileProcessor() { check(false); }

public:

	/**
	 * Constructor
	 * @param	InBuildManifest			The build manifest that receives the manifest data.
	 * @param	InBuildRoot				The root directory of the build
	 * @param	InDataThresholdTime		The cutoff time used for file reuse. Files older than this will not be reused
	 */
	FBuildDataFileProcessor( FBuildPatchAppManifestRef InBuildManifest, const FString& InBuildRoot, const FDateTime& InDataThresholdTime );

	/**
	 * Mark the beginning of a new file
	 * @param	InFileName		The file's local filename
	 */
	void BeginFile( const FString& InFileName );

	/**
	 * Process a set of data for a file
	 * @param	Data		The data memory
	 * @param	DataLen		The length of the data
	 */
	void ProcessFileData( const uint8* Data, const uint32& DataLen );

	/**
	 * Mark the end of the current file
	 */
	void EndFile();

	/**
	 * Get the new/know file stats
	 * @param	OutNewFiles		Receives the number of new files found
	 * @param	OutKnownFiles	Receives the number of recognised files found
	 */
	void GetFileStats( uint32& OutNewFiles, uint32& OutKnownFiles );

private:

	// Keep track of the number of new files
	uint32 NumNewFiles;

	// Keep track of the number of recognised files
	uint32 NumKnownFiles;

	// Holds the build manifest that is receiving the information
	FBuildPatchAppManifestPtr BuildManifest;

	// Holds the build manifest that is receiving the information
	FString BuildRoot;

	// Holds the current file being processed. Receives the chunk parts that we pass over.
	FFileManifestData* CurrentFile;

	// Holds an SHA hash calculator
	FSHA1 FileHash;

	// Counts the size of the file
	uint64 FileSize;

	// Are we currently processing a file
	bool IsProcessingFile;

	// The threshold time for old data. Used to determine whether to reuse chunks.
	FDateTime DataThresholdTime;
};

/**
 * A class that can hold recently used chunks in memory so that when hash collisions for particular hashes are common
 * we are not having to load existing chunks from disk/network every time. This is especially notable for when source data
 * can have repeated cycles of bytes for long periods.
 */
class FBuildGenerationChunkCache
{
public:
	/**
	 * A child class that is return to provide an interface to chunk data, wraps up reading from RAM or file
	 */
	class FChunkReader
	{
	private:
		// The path to the file for this chunk
		const FString ChunkFilePath;

		// The archive for this chunk if loading from file
		FArchive* ChunkFileReader;

		// Shared ref to the FChunkFile that holds chunk data
		TSharedRef< FChunkFile > ChunkFile;

		// Pointer to an unsigned int that stores how many bytes were read from disk so far
		uint32* FileBytesRead;

		// Pointer to the header data from our FChunkFile
		FChunkHeader* ChunkHeader;

		// Pointer to the chunk data from our FChunkFile
		uint8* ChunkData;

		// How many bytes we have read out of this chunk, used to calculate when we need more data from file
		uint32 MemoryBytesRead;

	public:
		/**
		 * Constructs the reader from required data
		 * @param	InChunkFilePath		The file path of the source file for the chunk
		 * @param	InChunkFile			The data structure for this chunk
		 * @param	InBytesRead			Pointer to an unsigned int to keep track of how much data is read from file
		 */
		FChunkReader(const FString& InChunkFilePath, TSharedRef< FChunkFile > InChunkFile, uint32* InBytesRead);

		/**
		 * Destructor. Cleans up and releases data locks.
		 */
		~FChunkReader();

		/**
		 * Whether the chunk is valid according to header information.
		 * @return		Returns true if the chunk file checked out ok to use
		 */
		const bool IsValidChunk();

		/**
		 * Get the GUID for this chunk
		 * @return		The chunk GUID
		 */
		const FGuid& GetChunkGuid();

		/**
		 * Reads the next block of bytes from the chunk data, setting the pointer to a memory location of the first byte requested
		 * @param OutDataBuffer		Pointer to a pointer which gets set to location of next byte
		 * @param ReadLength		How many bytes should be available to read from the pointer given. This must be <= BytesLeft
		 */
		void ReadNextBytes( uint8** OutDataBuffer, const uint32& ReadLength );

		/**
		 * Get the number of bytes left to read from this chunk data
		 * @return		The number of bytes left available to read from ReadNextBytes
		 */
		const uint32 BytesLeft();

	private:
		/**
		 * Returns the FArchive for the chunk source file. Wraps up opening the file, grabbing the header, and seeking to the
		 * next byte that hasn't been read yet, which all happens only first time we need the file per reader.
		 * @return		Archive for reading chunk from disk
		 */
		FArchive* GetArchive();
	};

private:
	/**
	* Constructor
	*/
	FBuildGenerationChunkCache();

private:
	// The number of chunks to cache in memory
	const static int32 NumChunksToCache = 1024;

	// The map of chunks we have cached
	TMap< FString, TSharedRef< FChunkFile > > ChunkCache;

	// The store for how much file data has been read for each chunk
	TMap< FString, uint32* > BytesReadPerChunk;

	// The stats collector class, and some stats variables
	FStatsCollectorPtr StatsCollector;
	volatile int64* StatChunksInDataCache;
	volatile int64* StatNumCacheLoads;
	volatile int64* StatNumCacheBoots;

	// The singleton instance for this class
	static TSharedPtr< FBuildGenerationChunkCache > SingletonInstance;

public:

	/**
	 * Sets the stats collector, which will be used to record statistics from the point of settings
	 * @param StatsCollector		Ref to the stats collector
	 */
	void SetStatsCollector(FStatsCollectorRef StatsCollector);

	/**
	 * Get a reader class for a chunk file. The reader class is the interface to chunk data which will either come from RAM or file.
	 * @return		Shared ref to a reader class, in order to allow memory freeing, you should not keep hold of references when finished with chunk
	 */
	TSharedRef< FChunkReader > GetChunkReader( const FString& ChunkFilePath );

	/**
	 * Call to initialise static singleton so that the cache system can be used
	 */
	static void Init();

	/**
	 * Get a reference to the chunk system. Only call between Init and Shutdown calls.
	 */
	static FBuildGenerationChunkCache& Get();

	/**
	 * Call to clean up all data and release memory allocations for the class.
	 */
	static void Shutdown();

private:
	/**
	 * Clear out and clean up all memory allocations
	 */
	void Cleanup();
};

/**
 * A class that controls the process of generating manifests and chunk data from a build image, and also constructing builds from manifests and chunks
 */
class FBuildDataGenerator
{
	friend class FBuildDataChunkProcessor;
	friend class FBuildDataFileProcessor;
public:

	/**
	 * Processes a Build Image to determine new chunks and produce a chunk based manifest, all saved to the cloud.
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param Settings				Specifies the settings for the operation.  See FBuildPatchSettings documentation.
	 * @return		true if no file errors occurred
	 */
	//static bool GenerateChunksManifestFromDirectory( const FString& RootDirectory, const uint32& InAppID, const FString& AppName, const FString& BuildVersion, const FString& LaunchExe, const FString& LaunchCommand, const FString& IgnoreListFile );
	static bool GenerateChunksManifestFromDirectory( const FBuildPatchSettings& Settings );

	/**
	 * Processes a Build Image to determine new raw files and produce a raw file based manifest, all saved to the cloud.
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param Settings				Specifies the settings for the operation.  See FBuildPatchSettings documentation.
	 * @return		true if no file errors occurred
	 */
	//static bool GenerateFilesManifestFromDirectory( const FString& RootDirectory, const uint32& InAppID, const FString& AppName, const FString& BuildVersion, const FString& LaunchExe, const FString& LaunchCommand, const FString& IgnoreListFile );
	static bool GenerateFilesManifestFromDirectory( const FBuildPatchSettings& Settings );

	/**
	 * Checks to see if a given chunk already exists in the database of known chunks
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param ChunkHash		IN		The hash for this chunk.
	 * @param ChunkData		IN		The actual chunk data.
	 * @param ChunkGuid		OUT		Will be set to the existing chunk guid if found.
	 * @return		Whether this chunk is new or existing.
	 */
	static bool FindExistingChunkData( const uint64& ChunkHash, const TRingBuffer< uint8, FBuildPatchData::ChunkDataSize >& ChunkData, FGuid& ChunkGuid );
	static bool FindExistingChunkData( const uint64& ChunkHash, const uint8* ChunkData, FGuid& ChunkGuid );

	/**
	 * Given a Guid and Hash for a chunk file, try to find it from a cloud directory, starting from the latest
	 * version of the naming convention.
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param ChunkGuid		The guid of the chunk data
	 * @param ChunkHash		The chunk's hash value
	 * @return		The full patch to the found chunk, otherwise empty string.
	 */
	static FString DiscoverChunkFilename(const FGuid& ChunkGuid, const uint64& ChunkHash);

	/**
	 * Checks to see if a given file already exists in the database of known files
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param InSourceFile			IN		The filename of the file on disk.
	 * @param InFileHash			IN		The hash for the file.
	 * @param DataThresholdTime		IN		The date/time before which files will not be reused
	 * @param OutFileGuid			OUT		Will be set to the existing file guid if found.
	 * @return		Whether this file is new or existing.
	 */
	static bool FindExistingFileData(const FString& InSourceFile, const FSHAHashData& InFileHash, const FDateTime& DataThresholdTime, FGuid& OutFileGuid);

	/**
	 * Saves out the file data for use with file based patch manifests
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param SourceFile		The filename of the source file on disk.
	 * @param FileHash			The hash for the file.
	 * @param FileGuid			The GUID for the file.
	 * @return		Whether this file is new or existing.
	 */
	static bool SaveOutFileData(const FString& SourceFile, const FSHAHashData& FileHash, const FGuid& FileGuid);

	/**
	 * Strips out files from the provided array that appear in the ignore list file
	 * @param AllFiles			The array of files to be stripped
	 * @param DepotDirectory	The directory that is the root of the ignore file list contents
	 * @param IgnoreListFile	Path to a text file containing list of ignorable files
	 */
	static void StripIgnoredFiles( TArray< FString >& AllFiles, const FString& DepotDirectory, const FString& IgnoreListFile );

private:

	/**
	 * Helper function to compare given data to an existing chunk on file
	 * @param ChunkFilePath       IN    The path to the chunk file.
	 * @param ChunkData           IN    The data to compare with
	 * @param ChunkGuid           OUT   This will receive the GUID from the chunk header.
	 * @param SourceChunkIsValid  OUT   This will be set with whether the source chunk specified is valid
	 * @return		true if no file errors occurred and the data is identical
	 */
	static bool CompareDataToChunk(const FString& ChunkFilePath, uint8* ChunkData, FGuid& ChunkGuid, bool& SourceChunkIsValid);

private:
	// For Chunk Manifest generation, the existing chunks rolling hash lookup
	static TMap< uint64, TArray< FGuid > > ExistingChunkHashInventory;

	// For Chunk Manifest generation, the existing chunks GUID lookup
	static TMap< FGuid, FString > ExistingChunkGuidInventory;

	// For Chunk Manifest generation, have we enumerated the existing chunks yet
	static bool ExistingChunksEnumerated;

	// For File Manifest generation, the existing files
	static TMap< FSHAHashData, TArray< FString > > ExistingFileInventory;

	// For File Manifest generation, have we enumerated the existing files yet
	static bool ExistingFilesEnumerated;

	// A critical section to enforce only one build at a time
	static FCriticalSection SingleConcurrentBuildCS;

};

/**
 * Simple data structure to hold values parsed from the File Attribute List
 */
struct FFileAttributes
{
	bool bReadOnly;
	bool bCompressed;
	bool bUnixExecutable;
	TSet<FString> InstallTags;
	FFileAttributes();
};

#endif //WITH_BUILDPATCHGENERATION
