// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchGeneration.h: Declares classes involved with producing build data.
=============================================================================*/

#ifndef __BuildPatchGeneration_h__
#define __BuildPatchGeneration_h__

#pragma once

#if WITH_BUILDPATCHGENERATION

/**
 * A threaded class that holds Build stream data as a FIFO data buffer of given size. Will read until full then wait
 * for data to be read out before continuing.
 */
class FBuildStream
{
private:
	// A private runnable class that reads in data
	class FBuildStreamReader : public FRunnable
	{
	public:
		// The directory that we want to read from
		FString DepotDirectory;

		// The path to a text file containing \r\n separated depot relative files to ignore
		FString IgnoreListFile;

		// Pointer to FBuildStream
		FBuildStream* BuildStream;

		// Default Constructor
		FBuildStreamReader();

		// Destructor
		~FBuildStreamReader();

		// Start FRunnable interface
		virtual bool Init() override;
		virtual uint32 Run() override;
		// End FRunnable interface

		void StartThread();

		FRunnableThread * Thread;

	} BuildStreamReader;

	// A private struct for storing the data span of a file in the build stream
	struct FFileSpan
	{
		// The file's filename
		FString Filename;
		// The file data start index
		uint64 StartIdx;
		// The size of the file
		uint64 Size;
		
		/**
		 * Default constructor
		 */
		FFileSpan()
			: StartIdx( 0 )
			, Size( 0 )
		{
		}
	};

public:
	/**
	 * Constructor
	 */
	FBuildStream( const FString& RootDirectory, const FString& IgnoreListFile );

	/**
	 * Default destructor
	 */
	~FBuildStream();

	/**
	 * Retrieves the file details for a specific start index.
	 * @param	IN	StartingIdx		The data index into the build image.
	 * @param	OUT Filename		The filename for the file starting at StartingIdx. NB: The variable remains UNCHANGED if no file starts here.
	 * @param	OUT FileSize		The size of the file starting at StartingIdx. NB: The variable remains UNCHANGED if no file starts here.
	 * @return	true if the data byte at StartingIdx is the start of a file
	 */
	bool GetFileSpan( const uint64& StartingIdx, FString& Filename, uint64& FileSize );

	/**
	 * Fetches some data from the buffer, also removing it.
	 * @param	IN	Buffer			Pointer to buffer to receive the data.
	 * @param	IN	ReqSize			The amount of data to attempt to retrieve.
	 * @param	IN	WaitForData		Optional: Default true. Whether to wait until there is enough data in the buffer.
	 * @return	The amount of data retrieved
	 */
	uint32 DequeueData( uint8* Buffer, const uint32& ReqSize, const bool WaitForData = true );

	/**
	 * Gets a list of empty files that the build contains.
	 * @return	Array of empty files in the build
	 */
	const TArray< FString > GetEmptyFiles();

	/**
	 * Whether the read thread has finished reading the build image.
	 * @return	true if there is no more data coming into the buffer
	 */
	bool IsEndOfBuild();

	/**
	 * Whether there is any more data available to dequeue from the buffer
	 * @return	true if there is no more data coming in, and the internal buffer is also empty.
	 */
	bool IsEndOfData();

private:
	/**
	 * Hide the default constructor so that the params must be given
	 */
	FBuildStream(){};

	//////////////////////////////////////////////////////////////////////////
	// Private functions for manipulating the data store
private:
	/**
	 * Wipes out all data
	 */
	void Clear();

	/**
	 * How much more data the internal buffer can hold before being full
	 * @return	the number of bytes free in the buffer
	 */
	uint32 SpaceLeft();

	/**
	 * How much data the internal buffer is currently holding
	 * @return	the number of bytes held in the buffer
	 */
	uint32 DataAvailable();

	/**
	 * Mark the beginning of a new file
	 * @param	IN	Filename	The local build filename to the file
	 * @param	IN	FileSize	The size of the file
	 */
	void BeginNewFile( const FString& Filename, const uint64& FileSize );

	/**
	 * We must special case empty files because we don't want them effecting chunk data
	 * @param	IN	Filename	The local build filename to the file
	 */
	void AddEmptyFile( const FString& Filename );

	/**
	 * Enqueue a buffer of data into the internal buffer.
	 * @param	IN	Buffer		Pointer to buffer of data.
	 * @param	IN	Len			The length of the data buffer.
	 */
	void EnqueueData( const uint8* Buffer, const uint32& Len );

	/**
	 * Mark the end of the build image
	 * @param	IN	bIsEnd		Optional: Default true. Whether to set the build end as true or false
	 */
	void EndOfBuild( bool bIsEnd = true );

	//////////////////////////////////////////////////////////////////////////
	// Private variables
private:
	// Holds the filenames referred to index positions in the data stream
	TMap< uint64, FFileSpan > FilesParsed;

	// Holds a list of empty files
	TArray< FString > EmptyFiles;

	// A critical section for accessing the FilesParsed and EmptyFiles data.
	FCriticalSection FilesListsCS;

	// Holds the ring buffer for loaded data code words
	TRingBuffer< uint8, StreamBufferSize > BuildDataStream;

	// A critical section for accessing the data stream, BuildDataStream
	FCriticalSection BuildDataStreamCS;

	// Whether the build has been completely read from disk
	bool bNoMoreData;

	// A critical section for accessing the no more data flag, NoMoreData.
	FCriticalSection NoMoreDataCS;
};

/**
 * A class that controls processing of build data to produce the manifest with chunks
 */
class FBuildDataChunkProcessor
{
public:

	/**
	 * Constructor
	 * @param	InBuildManifest 	a build manifest that receives the manifest data.
	 * @param	InBuildRoot			The root directory of the build
	 */
	FBuildDataChunkProcessor( FBuildPatchAppManifestRef InBuildManifest, const FString& InBuildRoot );
	
	/**
	 * Default destructor
	 */
	~FBuildDataChunkProcessor();

	/**
	 * Start a new chunk
	 * @param	bZeroBuffer 	Optional: Default true. Whether to clear the chunk buffer with zero
	 */
	void BeginNewChunk( const bool& bZeroBuffer = true );

	/**
	 * Start a new chunk part
	 */
	void BeginNewChunkPart();
	
	/**
	 * End the current chunk part
	 */
	void EndNewChunkPart();
	
	/**
	 * End the current chunk
	 * @param	ChunkHash		The chunk's hash value
	 * @param	ChunkData		Pointer to the chunk data, must be of FBuildPatchData::ChunkDataSize length
	 * @param	ChunkGuid		The chunk GUID of this chunk. If it matches CurrentChunkGuid then this is a new chunk, otherwise it is the guid of the recognized chunk
	 */
	void EndNewChunk( const uint64& ChunkHash, const uint8* ChunkData, const FGuid& ChunkGuid );
	
	/**
	 * Begins insertion of recognized chunk
	 */
	void PushChunk();

	/**
	 * Ends the insertion of recognized chunk
	 * @param	ChunkHash		The hash value for the recognised chunk
	 * @param	ChunkData		The data for the recognised chunk
	 * @param	ChunkGuid		The GUID for the recognised chunk
	 */
	void PopChunk( const uint64& ChunkHash, const uint8* ChunkData, const FGuid& ChunkGuid );

	/**
	 * When the build image has been fully covered, call to end any current chunk we have (which will be zero padded)
	 */
	void FinalChunk();

	/**
	 * Mark the beginning of a new file
	 * @param	FileName	The file's local filename
	 */
	void BeginFile( const FString& FileName );

	/**
	 * Mark the end of the current file
	 */
	void EndFile();

	/**
	 * Skips over the next byte from a recognized chunk
	 * @param	NextByte		The next byte
	 * @param	bStartOfFile	Whether this byte is the beginning of a file
	 * @param	bEndOfFile		Whether this byte is the end of a file
	 * @param	Filename		The filename of the file, only required if start of new file
	 */
	void SkipKnownByte( const uint8& NextByte, const bool& bStartOfFile, const bool& bEndOfFile, const FString& Filename );

	/**
	 * Process a new byte into the current new chunk
	 * @param	NewByte			The new byte
	 * @param	bStartOfFile	Whether this byte is the beginning of a file
	 * @param	bEndOfFile		Whether this byte is the end of a file
	 * @param	Filename		The filename of the file, only required if start of new file
	 */
	void ProcessNewByte( const uint8& NewByte, const bool& bStartOfFile, const bool& bEndOfFile, const FString& Filename );

	/**
	 * Get the new/know chunks stats
	 * @param	OutNewChunks	Receives the number of new chunks found
	 * @param	OutKnownChunks	Receives the number of recognised chunks found
	 */
	void GetChunkStats( uint32& OutNewChunks, uint32& OutKnownChunks );

	/**
	 * Get the chunk filesizes that have been saved out
	 * @return ref to the Map of GUID to filesize
	 */
	const TMap<FGuid, int64>& GetChunkFilesizes();

private:

	// Keep track of the number of new chunks generated
	uint32 NumNewChunks;

	// Keep track of the number of recognised chunks
	uint32 NumKnownChunks;

	// Holds the build manifest that is receiving the information
	FBuildPatchAppManifestPtr BuildManifest;

	// Holds the build manifest that is receiving the information
	FString BuildRoot;

	// The chunk writer system
	FChunkWriter ChunkWriter;

	// A GUID for the current chunk
	FGuid CurrentChunkGuid;

	// The current chunk data
	uint8* CurrentChunkBuffer;

	// The current index into the chunk data
	uint32 CurrentChunkBufferPos;

	// Holds the current file being processed. Receives the chunk parts that we pass over.
	FFileManifestData* CurrentFile;

	// Holds an SHA hash calculator
	FSHA1 FileHash;

	// Are we currently processing a chunk
	bool IsProcessingChunk;

	// Are we currently processing a chunk part
	bool IsProcessingChunkPart;

	// Are we currently processing a file
	bool IsProcessingFile;

	// Have we pushed to a recognized chunk
	bool ChunkIsPushed;

	// Backup of CurrentChunkGuid when pushing a chunk
	FGuid BackupChunkGuid;

	// Backup of CurrentChunkBufferPos when pushing a chunk
	uint32 BackupChunkBufferPos;

	// Backup of IsProcessingChunk when pushing a chunk
	bool BackupProcessingChunk;

	// Backup of IsProcessingChunkPart when pushing a chunk
	bool BackupProcessingChunkPart;

	// Cached map of chunk file sizes
	TMap<FGuid, int64> ChunkFileSizes;
};

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

		// The validity period of chunk data. Anything older than this will be considered to be invalid
		FDateTime DataAgeThreshold;

	public:
		/**
		 * Constructs the reader from required data
		 * @param	InChunkFilePath		The file path of the source file for the chunk
		 * @param	InChunkFile			The data structure for this chunk
		 * @param	InBytesRead			Pointer to an unsigned int to keep track of how much data is read from file
		 * @param	InDataAgeThreshold	The validity period of chunk data. Anything older than this will be considered to be invalid
		 */
		FChunkReader( const FString& InChunkFilePath, TSharedRef< FChunkFile > InChunkFile, uint32* InBytesRead, const FDateTime& InDataAgeThreshold );

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
	* @param	DataAgeThreshold		The cutoff point before which data chunks should be regarded as isvalid
	*/
	FBuildGenerationChunkCache(const FDateTime& DataAgeThreshold);

private:
	// The number of chunks to cache in memory
	const static int32 NumChunksToCache = 256;

	FDateTime DataAgeThreshold;

	// The map of chunks we have cached
	TMap< FString, TSharedRef< FChunkFile > > ChunkCache;

	// The store for how much file data has been read for each chunk
	TMap< FString, uint32* > BytesReadPerChunk;

	// The singleton instance for this class
	static TSharedPtr< FBuildGenerationChunkCache > SingletonInstance;

public:
	/**
	 * Get a reader class for a chunk file. The reader class is the interface to chunk data which will either come from RAM or file.
	 * @return		Shared ref to a reader class, in order to allow memory freeing, you should not keep hold of references when finished with chunk
	 */
	TSharedRef< FChunkReader > GetChunkReader( const FString& ChunkFilePath );

	/**
	 * Call to initialise static singleton so that the cache system can be used
	 * @param DataAgeThreshold		Any chunks older than this date will be classed as invalid
	 */
	static void Init(const FDateTime& DataAgeThreshold);

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
	FFileAttributes();
};

#endif //WITH_BUILDPATCHGENERATION

#endif // __BuildPatchGeneration_h__
