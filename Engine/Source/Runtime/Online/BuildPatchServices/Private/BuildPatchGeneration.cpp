// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchGeneration.cpp: Implements the classes that control build
	installation, and the generation of chunks and manifests from a build image.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

#if WITH_BUILDPATCHGENERATION

FFileAttributes::FFileAttributes()
	: bReadOnly(false)
	, bCompressed(false)
	, bUnixExecutable(false)
{}

/**
 * Creates a 32bit hash value from a FSHAHashData. This is so that a TMap can be keyed
 * using a FSHAHashData
 * @param Hash	The SHA1 hash
 * @return The crc for the data
 */
FORCEINLINE uint32 GetTypeHash(const FSHAHashData& Hash)
{
	return FCrc::MemCrc32(Hash.Hash, FSHA1::DigestSize);
}

static bool IsUnixExecutable(const TCHAR* Filename)
{
#if PLATFORM_MAC
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(Filename), &FileInfo) == 0)
	{
		return (FileInfo.st_mode & S_IXUSR) != 0;
	}
#endif
	return false;
}

static FString GetSymlinkTarget(const TCHAR* Filename)
{
#if PLATFORM_MAC
	ANSICHAR SymlinkTarget[MAX_PATH] = { 0 };
	if (readlink(TCHAR_TO_UTF8(Filename), SymlinkTarget, MAX_PATH) != -1)
	{
		return UTF8_TO_TCHAR(SymlinkTarget);
	}
#endif
	return TEXT("");
}

/* FBuildStreamReader implementation
*****************************************************************************/
FBuildStream::FBuildStreamReader::FBuildStreamReader()
	: DepotDirectory(TEXT(""))
	, BuildStream(NULL)
	, Thread(NULL)
{
}

FBuildStream::FBuildStreamReader::~FBuildStreamReader()
{
	if (Thread)
	{
		Thread->Kill();
		delete Thread;
		Thread = NULL;
	}
}

bool FBuildStream::FBuildStreamReader::Init() 
{
	return BuildStream != NULL && IFileManager::Get().DirectoryExists( *DepotDirectory );
}

uint32 FBuildStream::FBuildStreamReader::Run()
{
	// Clear the build stream
	BuildStream->Clear();

	TArray< FString > AllFiles;
	IFileManager::Get().FindFilesRecursive( AllFiles, *DepotDirectory, TEXT("*.*"), true, false );
	AllFiles.Sort();

	// Remove the files that appear in an ignore list
	FBuildDataGenerator::StripIgnoredFiles( AllFiles, DepotDirectory, IgnoreListFile );

	// Allocate our file read buffer
	uint8* FileReadBuffer = new uint8[ FileBufferSize ];

	for (auto& SourceFile : AllFiles)
	{
		// Read the file
		FArchive* FileReader = IFileManager::Get().CreateFileReader( *SourceFile );
		const bool bBuildFileOpenSuccess = FileReader != NULL;
		if( bBuildFileOpenSuccess )
		{
			// Make SourceFile the format we want it in and start a new file
			FPaths::MakePathRelativeTo( SourceFile, *( DepotDirectory + TEXT( "/" ) ) );
			int64 FileSize = FileReader->TotalSize();
			// Process files that have bytes
			if( FileSize > 0 )
			{
				BuildStream->BeginNewFile( SourceFile, FileSize );
				while( !FileReader->AtEnd() )
				{
					const int64 SizeLeft = FileSize - FileReader->Tell();
					const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
					FileReader->Serialize( FileReadBuffer, ReadLen );
					// Copy into data stream
					BuildStream->EnqueueData( FileReadBuffer, ReadLen );
				}
			}
			// Special case zero byte files
			else if( FileSize == 0 )
			{
				BuildStream->AddEmptyFile( SourceFile );
			}
			FileReader->Close();
			delete FileReader;
		}
		else
		{
			// Not being able to load a required file from the build would be fatal, hard fault.
			GLog->Logf( TEXT( "FBuildStreamReader: Could not open file from build! %s" ), *SourceFile );
			GLog->PanicFlushThreadedLogs();
			// Use bool variable for easier to understand assert message
			check( bBuildFileOpenSuccess );
		}
	}

	// Mark end of build
	BuildStream->EndOfBuild();

	// Deallocate our file read buffer
	delete[] FileReadBuffer;

	return 0;
}

void FBuildStream::FBuildStreamReader::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("BuildStreamReaderThread"));
}

/* FBuildStream implementation
*****************************************************************************/
FBuildStream::FBuildStream( const FString& RootDirectory, const FString& IgnoreListFile )
{
	BuildStreamReader.DepotDirectory = RootDirectory;
	BuildStreamReader.IgnoreListFile = IgnoreListFile;
	BuildStreamReader.BuildStream = this;
	BuildStreamReader.StartThread();
}

FBuildStream::~FBuildStream()
{
}

bool FBuildStream::GetFileSpan( const uint64& StartingIdx, FString& Filename, uint64& FileSize )
{
	bool bFound = false;
	FilesListsCS.Lock();
	// Find the filename
	FFileSpan* FileSpan = FilesParsed.Find( StartingIdx );
	if( FileSpan != NULL )
	{
		Filename = FileSpan->Filename;
		FileSize = FileSpan->Size;
		bFound = true;
	}
	FilesListsCS.Unlock();
	return bFound;
}

uint32 FBuildStream::DequeueData( uint8* Buffer, const uint32& ReqSize, const bool WaitForData )
{
	// Wait for data
	if( WaitForData )
	{
		while ( DataAvailable() < ReqSize && !IsEndOfBuild() )
		{
			FPlatformProcess::Sleep( 0.01f );
		}
	}

	BuildDataStreamCS.Lock();
	uint32 ReadLen = FMath::Min< int64 >( ReqSize, DataAvailable() );
	ReadLen = BuildDataStream.Dequeue( Buffer, ReadLen );
	BuildDataStreamCS.Unlock();

	return ReadLen;
}

const TArray< FString > FBuildStream::GetEmptyFiles()
{
	FScopeLock ScopeLock( &FilesListsCS );
	return EmptyFiles;
}

bool FBuildStream::IsEndOfBuild()
{
	bool rtn;
	NoMoreDataCS.Lock();
	rtn = bNoMoreData;
	NoMoreDataCS.Unlock();
	return rtn;
}

bool FBuildStream::IsEndOfData()
{
	bool rtn;
	NoMoreDataCS.Lock();
	rtn = bNoMoreData;
	NoMoreDataCS.Unlock();
	BuildDataStreamCS.Lock();
	rtn &= BuildDataStream.RingDataUsage() == 0;
	BuildDataStreamCS.Unlock();
	return rtn;
}

void FBuildStream::Clear()
{
	EndOfBuild( false );

	BuildDataStreamCS.Lock();
	BuildDataStream.Empty();
	BuildDataStreamCS.Unlock();

	FilesListsCS.Lock();
	FilesParsed.Empty();
	FilesListsCS.Unlock();
}

uint32 FBuildStream::SpaceLeft()
{
	uint32 rtn;
	BuildDataStreamCS.Lock();
	rtn = BuildDataStream.RingDataSize() - BuildDataStream.RingDataUsage();
	BuildDataStreamCS.Unlock();
	return rtn;
}

uint32 FBuildStream::DataAvailable()
{
	uint32 rtn;
	BuildDataStreamCS.Lock();
	rtn = BuildDataStream.RingDataUsage();
	BuildDataStreamCS.Unlock();
	return rtn;
}

void FBuildStream::BeginNewFile( const FString& Filename, const uint64& FileSize )
{
	FFileSpan FileSpan;
	FileSpan.Filename = Filename;
	FileSpan.Size = FileSize;
	BuildDataStreamCS.Lock();
	FileSpan.StartIdx = BuildDataStream.TotalDataPushed();
	BuildDataStreamCS.Unlock();
	FilesListsCS.Lock();
	FilesParsed.Add( FileSpan.StartIdx, FileSpan );
	FilesListsCS.Unlock();
}

void FBuildStream::AddEmptyFile( const FString& Filename )
{
	FilesListsCS.Lock();
	EmptyFiles.Add( Filename );
	FilesListsCS.Unlock();
}

void FBuildStream::EnqueueData( const uint8* Buffer, const uint32& Len )
{
	// Wait for space
	while ( SpaceLeft() < Len )
	{
		FPlatformProcess::Sleep( 0.01f );
	}
	BuildDataStreamCS.Lock();
	BuildDataStream.Enqueue( Buffer, Len );
	BuildDataStreamCS.Unlock();
}

void FBuildStream::EndOfBuild( bool bIsEnd )
{
	NoMoreDataCS.Lock();
	bNoMoreData = bIsEnd;
	NoMoreDataCS.Unlock();
}

/* FBuildDataProcessor implementation
*****************************************************************************/
FBuildDataChunkProcessor::FBuildDataChunkProcessor( FBuildPatchAppManifestRef InBuildManifest, const FString& InBuildRoot )
	: NumNewChunks( 0 )
	, NumKnownChunks( 0 )
	, BuildRoot( InBuildRoot )
	, ChunkWriter( FBuildPatchServicesModule::GetCloudDirectory() )
	, CurrentChunkBufferPos( 0 )
	, CurrentFile(nullptr)
	, IsProcessingChunk( false )
	, IsProcessingChunkPart( false )
	, IsProcessingFile( false )
	, ChunkIsPushed( false )
	, BackupChunkBufferPos( 0 )
	, BackupProcessingChunk( false )
	, BackupProcessingChunkPart( false )
{
	BuildManifest = InBuildManifest;
	CurrentChunkGuid.Invalidate();
	BackupChunkGuid.Invalidate();
	CurrentChunkBuffer = new uint8[ FBuildPatchData::ChunkDataSize ];
}

FBuildDataChunkProcessor::~FBuildDataChunkProcessor()
{
	delete[] CurrentChunkBuffer;
}

void FBuildDataChunkProcessor::BeginNewChunk( const bool& bZeroBuffer )
{
	check( IsProcessingChunk == false );
	check( IsProcessingChunkPart == false );
	IsProcessingChunk = true;

	// NB: If you change this function, make sure you make required changes to 
	// Backup of chunk if need be at the end of a recognized chunk

	// Erase the chunk buffer data ready for new chunk parts
	if( bZeroBuffer )
	{
		FMemory::Memzero( CurrentChunkBuffer, FBuildPatchData::ChunkDataSize );
		CurrentChunkBufferPos = 0;
	}

	// Get a new GUID for this chunk for identification
	CurrentChunkGuid = FGuid::NewGuid();
}

void FBuildDataChunkProcessor::BeginNewChunkPart()
{
	check( IsProcessingChunkPart == false );
	check( IsProcessingChunk == true );
	IsProcessingChunkPart = true;

	// The current file should have a new chunk part setup
	if (CurrentFile)
	{
		CurrentFile->FileChunkParts.Add(FChunkPartData());
		FChunkPartData& NewPart = CurrentFile->FileChunkParts.Last();
		NewPart.Guid = CurrentChunkGuid;
		NewPart.Offset = CurrentChunkBufferPos;
	}
}

void FBuildDataChunkProcessor::EndNewChunkPart()
{
	check( IsProcessingChunkPart == true );
	check( IsProcessingChunk == true );
	IsProcessingChunkPart = false;

	// Current file should have it's last chunk part updated with the size value finalized.
	if (CurrentFile)
	{
		FChunkPartData* ChunkPart = &CurrentFile->FileChunkParts.Last();
		ChunkPart->Size = CurrentChunkBufferPos - ChunkPart->Offset;
		check( ChunkPart->Size != 0 );
	}
}

void FBuildDataChunkProcessor::EndNewChunk( const uint64& ChunkHash, const uint8* ChunkData, const FGuid& ChunkGuid )
{
	check( IsProcessingChunk == true );
	check( IsProcessingChunkPart == false );
	IsProcessingChunk = false;

	// A bool that will state whether we should update and log chunking progress
	bool bLogProgress = false;

	// If the new chunk was recognized, then we will have got a different Guid through, so fix up guid for files using this chunk
	if( CurrentChunkGuid != ChunkGuid )
	{
		bLogProgress = true;
		++NumKnownChunks;
		for (auto& FileManifest : BuildManifest->Data->FileManifestList)
		{
			for (auto& ChunkPart : FileManifest.FileChunkParts)
			{
				if( ChunkPart.Guid == CurrentChunkGuid )
				{
					ChunkPart.Guid = ChunkGuid;
				}
			}
		}
	}
	// If the chunk is new, count it if we were passed data
	else if( ChunkData )
	{
		bLogProgress = true;
		++NumNewChunks;
	}

	// Always queue the chunk if passed data, it will be skipped automatically if existing already, and we need to make
	// sure the latest version is saved out when recognizing an older version
	if( ChunkData )
	{
		ChunkWriter.QueueChunk( ChunkData, ChunkGuid, ChunkHash );

		// Also add the info to the data
		if (!BuildManifest->ChunkInfoLookup.Contains(ChunkGuid))
		{
			BuildManifest->Data->ChunkList.Add(FChunkInfoData());
			FChunkInfoData& NewChunk = BuildManifest->Data->ChunkList.Last();
			NewChunk.Guid = ChunkGuid;
			NewChunk.Hash = ChunkHash;
			NewChunk.FileSize = INDEX_NONE;
			NewChunk.GroupNumber = FCrc::MemCrc32(&ChunkGuid, sizeof(FGuid)) % 100;
			// Lookup is used just to ensure one copy of each chunk added, we must not use the ptr as array will realloc
			BuildManifest->ChunkInfoLookup.Add(ChunkGuid, nullptr);
		}

	}

	if( bLogProgress )
	{
		// Output to log for builder info
		GLog->Logf( TEXT( "%s %s [%d:%d]" ), *BuildManifest->GetAppName(), *BuildManifest->GetVersionString(), NumNewChunks, NumKnownChunks );
	}
}

void FBuildDataChunkProcessor::PushChunk()
{
	check( !ChunkIsPushed );
	ChunkIsPushed = true;

	// Then we can skip over this entire chunk using it for current files in processing
	BackupProcessingChunk = IsProcessingChunk;
	BackupProcessingChunkPart = IsProcessingChunkPart;
	BackupChunkGuid = CurrentChunkGuid;
	BackupChunkBufferPos = CurrentChunkBufferPos;
	if( BackupProcessingChunkPart )
	{
		EndNewChunkPart();
	}
	if( BackupProcessingChunk )
	{
		// null ChunkData will stop the incomplete chunk being saved out
		EndNewChunk( 0, NULL, CurrentChunkGuid );
	}

	// Start this chunk, must begin from 0
	CurrentChunkBufferPos = 0;
	BeginNewChunk( false );
	BeginNewChunkPart();
}

void FBuildDataChunkProcessor::PopChunk( const uint64& ChunkHash, const uint8* ChunkData, const FGuid& ChunkGuid )
{
	check( ChunkIsPushed );
	ChunkIsPushed = false;

	EndNewChunkPart();
	EndNewChunk( ChunkHash, ChunkData, ChunkGuid );

	// Backup data for previous part chunk
	IsProcessingChunk = BackupProcessingChunk;
	CurrentChunkGuid = BackupChunkGuid;
	CurrentChunkBufferPos = BackupChunkBufferPos;
}

void FBuildDataChunkProcessor::FinalChunk()
{
	// If we're processing a chunk part, then we MUST be processing a chunk too
	check(IsProcessingChunkPart ? IsProcessingChunk : true);

	if( IsProcessingChunk )
	{
		// The there is no more data and the last file is processed.
		// The final chunk should be finished
		uint64 NewChunkHash = FRollingHash< FBuildPatchData::ChunkDataSize >::GetHashForDataSet( CurrentChunkBuffer );
		FGuid ChunkGuid = CurrentChunkGuid;
		FBuildDataGenerator::FindExistingChunkData( NewChunkHash, CurrentChunkBuffer, ChunkGuid );
		if( IsProcessingChunkPart )
		{
			EndNewChunkPart();
		}
		EndNewChunk( NewChunkHash, CurrentChunkBuffer, ChunkGuid );
	}

	// Wait for the chunk writer to finish
	ChunkWriter.NoMoreChunks();
	ChunkWriter.WaitForThread();
}

void FBuildDataChunkProcessor::BeginFile( const FString& FileName )
{
	check( IsProcessingFile == false );
	IsProcessingFile = true;

	// We should start a new file, which begins with the current new chunk and current chunk offset
	BuildManifest->Data->FileManifestList.Add(FFileManifestData());
	CurrentFile = &BuildManifest->Data->FileManifestList.Last();
	CurrentFile->Filename = FileName;
	CurrentFile->bIsUnixExecutable = IsUnixExecutable(*(BuildRoot / FileName));
	CurrentFile->SymlinkTarget = GetSymlinkTarget(*(BuildRoot / FileName));

	// Setup for current chunk part
	if( IsProcessingChunkPart )
	{
		CurrentFile->FileChunkParts.Add(FChunkPartData());
		FChunkPartData& NewPart = CurrentFile->FileChunkParts.Last();
		NewPart.Guid = CurrentChunkGuid;
		NewPart.Offset = CurrentChunkBufferPos;
	}
}

void FBuildDataChunkProcessor::EndFile()
{
	check( IsProcessingFile == true );
	IsProcessingFile = false;

	if (CurrentFile)
	{
		if( IsProcessingChunkPart )
		{
			// Current file should have it's last chunk part updated with the size value finalized.
			FChunkPartData* ChunkPart = &CurrentFile->FileChunkParts.Last();
			ChunkPart->Size = CurrentChunkBufferPos - ChunkPart->Offset;
		}
		// Check file size is correct from chunks
		int64 FileSystemSize = IFileManager::Get().FileSize(*(BuildRoot / CurrentFile->Filename));
		CurrentFile->Init();
		check(CurrentFile->GetFileSize() == FileSystemSize);
	}

	CurrentFile = nullptr;
}

void FBuildDataChunkProcessor::SkipKnownByte( const uint8& NextByte, const bool& bStartOfFile, const bool& bEndOfFile, const FString& Filename )
{
	// Check for start of new file
	if( bStartOfFile )
	{
		BeginFile( Filename );
		FileHash.Reset();
	}

	// Increment position between start and end of file
	++CurrentChunkBufferPos;
	FileHash.Update( &NextByte, 1 );

	// Check for end of file
	if( bEndOfFile )
	{
		FileHash.Final();
		FileHash.GetHash( CurrentFile->FileHash.Hash );
		EndFile();
	}
}

void FBuildDataChunkProcessor::ProcessNewByte( const uint8& NewByte, const bool& bStartOfFile, const bool& bEndOfFile, const FString& Filename )
{
	// If we finished a chunk, we begin a new chunk and new chunk part
	if( !IsProcessingChunk )
	{
		BeginNewChunk();
		BeginNewChunkPart();
	}
	// Or if we finished a chunk part (by recognizing a chunk hash), we begin a new one
	else if( !IsProcessingChunkPart )
	{
		BeginNewChunkPart();
	}

	// Check for start of new file
	if( bStartOfFile )
	{
		BeginFile( Filename );
		FileHash.Reset();
	}

	// We add the old byte to our new chunk which will be used as part of the file it belonged to
	CurrentChunkBuffer[ CurrentChunkBufferPos++ ] = NewByte;
	FileHash.Update( &NewByte, 1 );

	// Check for end of file
	if( bEndOfFile )
	{
		FileHash.Final();
		FileHash.GetHash( CurrentFile->FileHash.Hash );
		EndFile();
	}

	// Do we have a full new chunk?
	check( CurrentChunkBufferPos <= FBuildPatchData::ChunkDataSize );
	if( CurrentChunkBufferPos == FBuildPatchData::ChunkDataSize )
	{
		uint64 NewChunkHash = FRollingHash< FBuildPatchData::ChunkDataSize >::GetHashForDataSet( CurrentChunkBuffer );
		FGuid ChunkGuid = CurrentChunkGuid;
		FBuildDataGenerator::FindExistingChunkData( NewChunkHash, CurrentChunkBuffer, ChunkGuid );
		EndNewChunkPart();
		EndNewChunk( NewChunkHash, CurrentChunkBuffer, ChunkGuid );
	}
}

void FBuildDataChunkProcessor::GetChunkStats( uint32& OutNewFiles, uint32& OutKnownFiles )
{
	OutNewFiles = NumNewChunks;
	OutKnownFiles = NumKnownChunks;
}

const TMap<FGuid, int64>& FBuildDataChunkProcessor::GetChunkFilesizes()
{
	ChunkWriter.GetChunkFilesizes(ChunkFileSizes);
	return ChunkFileSizes;
}

/* FBuildDataFileProcessor implementation
*****************************************************************************/
FBuildDataFileProcessor::FBuildDataFileProcessor(FBuildPatchAppManifestRef InBuildManifest, const FString& InBuildRoot, const FDateTime& InDataThresholdTime)
	: NumNewFiles( 0 )
	, NumKnownFiles( 0 )
	, BuildManifest( InBuildManifest )
	, BuildRoot( InBuildRoot )
	, CurrentFile(nullptr)
	, FileHash()
	, FileSize( 0 )
	, IsProcessingFile( false )
	, DataThresholdTime( InDataThresholdTime )
{
}

void FBuildDataFileProcessor::BeginFile( const FString& InFileName )
{
	check( !IsProcessingFile );
	IsProcessingFile = true;

	// Create the new file
	BuildManifest->Data->FileManifestList.Add(FFileManifestData());
	CurrentFile = &BuildManifest->Data->FileManifestList.Last();
	CurrentFile->Filename = InFileName;
	CurrentFile->bIsUnixExecutable = IsUnixExecutable(*(BuildRoot / InFileName));
	CurrentFile->SymlinkTarget = GetSymlinkTarget(*(BuildRoot / InFileName));

	// Add the 'chunk part' which will just be the whole file
	CurrentFile->FileChunkParts.Add(FChunkPartData());

	// Reset the hash and size counter
	FileHash.Reset();
	FileSize = 0;
}

void FBuildDataFileProcessor::ProcessFileData( const uint8* Data, const uint32& DataLen )
{
	check( IsProcessingFile );

	// Update the hash
	FileHash.Update( Data, DataLen );

	// Count size
	FileSize += DataLen;
}

void FBuildDataFileProcessor::EndFile()
{
	check( IsProcessingFile );
	check(CurrentFile);
	check( CurrentFile->FileChunkParts.Num() == 1 );
	IsProcessingFile = false;

	// Finalize the hash
	FileHash.Final();
	FileHash.GetHash( CurrentFile->FileHash.Hash );

	// Use hash and full file name to find out if we have a file match
	FString FullPath = FPaths::Combine( *BuildRoot, *CurrentFile->Filename );
	FGuid FileGuid = FGuid::NewGuid();
	bool bFoundSameFile = FBuildDataGenerator::FindExistingFileData( FullPath, CurrentFile->FileHash, DataThresholdTime, FileGuid );

	// Fill the 'chunk part' info
	FChunkPartData& FilePart = CurrentFile->FileChunkParts[0];
	FilePart.Guid = FileGuid;
	FilePart.Offset = 0;
	FilePart.Size = FileSize;

	// Init and check size
	CurrentFile->Init();
	check(CurrentFile->GetFileSize() == FileSize);

	// Always call save to ensure new versions exist when recognising old ones, the save will be skipped if required copies already exist.
	FBuildDataGenerator::SaveOutFileData( FullPath, CurrentFile->FileHash, FileGuid );

	// Also add the info to the data
	if (!BuildManifest->ChunkInfoLookup.Contains(FileGuid))
	{
		BuildManifest->Data->ChunkList.Add(FChunkInfoData());
		FChunkInfoData& DataInfo = BuildManifest->Data->ChunkList.Last();
		DataInfo.Guid = FileGuid;
		DataInfo.Hash = INDEX_NONE;
		DataInfo.FileSize = FileSize;
		DataInfo.GroupNumber = FCrc::MemCrc32(&FileGuid, sizeof(FGuid)) % 100;
		// Lookup is used just to ensure one copy of each chunk added, we must not use the ptr as array will realloc
		BuildManifest->ChunkInfoLookup.Add(FileGuid, nullptr);
	}

	// Count new/known stats
	if( bFoundSameFile == false )
	{
		++NumNewFiles;
	}
	else
	{
		++NumKnownFiles;
	}

	// Output to log for builder info
	GLog->Logf( TEXT( "%s %s [%d:%d]" ), *BuildManifest->GetAppName(), *BuildManifest->GetVersionString(), NumNewFiles, NumKnownFiles );
}

void FBuildDataFileProcessor::GetFileStats( uint32& OutNewFiles, uint32& OutKnownFiles )
{
	OutNewFiles = NumNewFiles;
	OutKnownFiles = NumKnownFiles;
}

/* FBuildSimpleChunkCache::FChunkReader implementation
*****************************************************************************/
FBuildGenerationChunkCache::FChunkReader::FChunkReader( const FString& InChunkFilePath, TSharedRef< FChunkFile > InChunkFile, uint32* InBytesRead, const FDateTime& InDataAgeThreshold )
	: ChunkFilePath( InChunkFilePath )
	, ChunkFileReader( NULL )
	, ChunkFile( InChunkFile )
	, FileBytesRead( InBytesRead )
	, MemoryBytesRead( 0 )
	, DataAgeThreshold( InDataAgeThreshold )
{
	ChunkFile->GetDataLock( &ChunkData, &ChunkHeader );
}

FBuildGenerationChunkCache::FChunkReader::~FChunkReader()
{
	ChunkFile->ReleaseDataLock();

	// Close file handle
	if( ChunkFileReader != NULL )
	{
		ChunkFileReader->Close();
		delete ChunkFileReader;
		ChunkFileReader = NULL;
	}
}

FArchive* FBuildGenerationChunkCache::FChunkReader::GetArchive()
{
	// Open file handle?
	if( ChunkFileReader == NULL )
	{
		ChunkFileReader = IFileManager::Get().CreateFileReader( *ChunkFilePath );
		if( ChunkFileReader == NULL )
		{
			// Break the magic to mark as invalid
			ChunkHeader->Magic = 0;
			GLog->Logf( TEXT( "WARNING: Skipped missing chunk file %s" ), *ChunkFilePath );
			return NULL;
		}
		// Read Header?
		if( ChunkHeader->Guid.IsValid() == false )
		{
			*ChunkFileReader << *ChunkHeader;
		}
		// Check the file is not too old to reuse
		if (IFileManager::Get().GetTimeStamp(*ChunkFilePath) < DataAgeThreshold)
		{
			// Break the magic to mark as invalid (this chunk is too old to reuse)
			ChunkHeader->Magic = 0;
			return ChunkFileReader;
		}
		// Check we can seek otherwise bad chunk
		const int64 ExpectedFileSize = ChunkHeader->DataSize + ChunkHeader->HeaderSize;
		const int64 ChunkFileSize = ChunkFileReader->TotalSize();
		const int64 NextByte = ChunkHeader->HeaderSize + *FileBytesRead;
		if( ChunkFileSize == ExpectedFileSize
		 && NextByte < ChunkFileSize )
		{
			// Seek to next byte
			ChunkFileReader->Seek( NextByte );
			// Break the magic to mark as invalid if archive errored, this chunk will get ignored
			if( ChunkFileReader->GetError() )
			{
				ChunkHeader->Magic = 0;
			}
		}
		else
		{
			// Break the magic to mark as invalid
			ChunkHeader->Magic = 0;
		}
		// If this chunk is valid and compressed, we must read the entire file and decompress to memory now if we have not already
		// as we cannot compare to compressed data
		if( *FileBytesRead == 0 && ChunkHeader->StoredAs & FChunkHeader::STORED_COMPRESSED )
		{
			// Load the compressed chunk data
			TArray< uint8 > CompressedData;
			CompressedData.Empty( ChunkHeader->DataSize );
			CompressedData.AddUninitialized( ChunkHeader->DataSize );
			ChunkFileReader->Serialize( CompressedData.GetData(), ChunkHeader->DataSize );
			// Uncompress
			bool bSuceess = FCompression::UncompressMemory(
				static_cast< ECompressionFlags >( COMPRESS_ZLIB | COMPRESS_BiasMemory ),
				ChunkData,
				FBuildPatchData::ChunkDataSize,
				CompressedData.GetData(),
				ChunkHeader->DataSize );
			// Mark that we have fully read decompressed data and update the chunkfile's data size as we are expanding it
			*FileBytesRead = FBuildPatchData::ChunkDataSize;
			ChunkHeader->DataSize = FBuildPatchData::ChunkDataSize;
			// Check uncompression was OK
			if( !bSuceess )
			{
				ChunkHeader->Magic = 0;
			}
		}
	}
	return ChunkFileReader;
}

const bool FBuildGenerationChunkCache::FChunkReader::IsValidChunk()
{
	if( ChunkHeader->Guid.IsValid() == false )
	{
		GetArchive();
	}
	// Check magic, and current support etc.
	const bool bValidHeader = ChunkHeader->IsValidMagic();
	const bool bValidChunkGuid = ChunkHeader->Guid.IsValid();
	const bool bSupportedFormat = ChunkHeader->StoredAs == FChunkHeader::STORED_RAW || ChunkHeader->StoredAs == FChunkHeader::STORED_COMPRESSED;
	return bValidHeader && bValidChunkGuid && bSupportedFormat;
}

const FGuid& FBuildGenerationChunkCache::FChunkReader::GetChunkGuid()
{
	if( ChunkHeader->Guid.IsValid() == false )
	{
		GetArchive();
	}
	return ChunkHeader->Guid;
}

void FBuildGenerationChunkCache::FChunkReader::ReadNextBytes( uint8** OutDataBuffer, const uint32& ReadLength )
{
	uint8* BufferNextByte = &ChunkData[ MemoryBytesRead ];
	// Do we need to load from disk?
	if( ( MemoryBytesRead + ReadLength ) > *FileBytesRead )
	{
		FArchive* Reader = GetArchive();
		// Do not allow incorrect usage
		check( Reader );
		check( ReadLength <= BytesLeft() );
		// Read the number of bytes extra we need
		const int32 NumFileBytesRead = *FileBytesRead;
		const int32 NextMemoryBytesRead = MemoryBytesRead + ReadLength;
		const uint32 FileReadLen = FMath::Max<int32>( 0, NextMemoryBytesRead - NumFileBytesRead );
		*FileBytesRead += FileReadLen;
		Reader->Serialize( BufferNextByte, FileReadLen );
		// Assert if read error, if theres some problem accessing chunks then continuing would cause bad patch
		// ratios, so it's better to hard fault.
		const bool bChunkReadOK = !Reader->GetError();
		if( !bChunkReadOK )
		{
			// Print something helpful
			GLog->Logf( TEXT( "FATAL ERROR: Could not read from chunk FArchive %s" ), *ChunkFilePath );
			// Check with bool variable so that output will be readable
			check( bChunkReadOK );
		}
	}
	MemoryBytesRead += ReadLength;
	(*OutDataBuffer) = BufferNextByte;
}

const uint32 FBuildGenerationChunkCache::FChunkReader::BytesLeft()
{
	if( ChunkHeader->Guid.IsValid() == false )
	{
		GetArchive();
	}
	return FBuildPatchData::ChunkDataSize - MemoryBytesRead;
}

/* FBuildGenerationChunkCache implementation
*****************************************************************************/
FBuildGenerationChunkCache::FBuildGenerationChunkCache(const FDateTime& InDataAgeThreshold)
	: DataAgeThreshold(InDataAgeThreshold)
{
}

TSharedRef< FBuildGenerationChunkCache::FChunkReader > FBuildGenerationChunkCache::GetChunkReader( const FString& ChunkFilePath )
{
	if( ChunkCache.Contains( ChunkFilePath ) == false )
	{
		// Remove oldest access from cache?
		if( ChunkCache.Num() >= NumChunksToCache )
		{
			FString OldestAccessChunk = TEXT( "" );
			double OldestAccessTime = FPlatformTime::Seconds();
			for( auto ChunkCacheIt = ChunkCache.CreateConstIterator(); ChunkCacheIt; ++ChunkCacheIt )
			{
				const FString& ChunkCacheFilePath = ChunkCacheIt.Key();
				const FChunkFile& ChunkFile = ChunkCacheIt.Value().Get();
				if( ChunkFile.GetLastAccessTime() < OldestAccessTime )
				{
					OldestAccessTime = ChunkFile.GetLastAccessTime();
					OldestAccessChunk = ChunkCacheFilePath;
				}
			}
			ChunkCache.Remove( OldestAccessChunk );
			delete BytesReadPerChunk[ OldestAccessChunk ];
			BytesReadPerChunk.Remove( OldestAccessChunk );
		}
		// Add the chunk to cache
		ChunkCache.Add( ChunkFilePath, MakeShareable( new FChunkFile( 1, true ) ) );
		BytesReadPerChunk.Add( ChunkFilePath, new uint32( 0 ) );
	}
	return MakeShareable( new FChunkReader( ChunkFilePath, ChunkCache[ ChunkFilePath ], BytesReadPerChunk[ ChunkFilePath ], DataAgeThreshold ) );
}

void FBuildGenerationChunkCache::Cleanup()
{
	ChunkCache.Empty( 0 );
	for( auto BytesReadPerChunkIt = BytesReadPerChunk.CreateConstIterator(); BytesReadPerChunkIt; ++BytesReadPerChunkIt )
	{
		delete BytesReadPerChunkIt.Value();
	}
	BytesReadPerChunk.Empty( 0 );
}

/* FBuildGenerationChunkCache system singleton setup
*****************************************************************************/
TSharedPtr< FBuildGenerationChunkCache > FBuildGenerationChunkCache::SingletonInstance = NULL;

void FBuildGenerationChunkCache::Init(const FDateTime& DataAgeThreshold)
{
	// We won't allow misuse of these functions
	check( !SingletonInstance.IsValid() );
	SingletonInstance = MakeShareable(new FBuildGenerationChunkCache(DataAgeThreshold));
}

FBuildGenerationChunkCache& FBuildGenerationChunkCache::Get()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	return *SingletonInstance.Get();
}

void FBuildGenerationChunkCache::Shutdown()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	SingletonInstance->Cleanup();
	SingletonInstance.Reset();
}

/* FBuildDataGenerator static variables
*****************************************************************************/
TMap< FGuid, FString > FBuildDataGenerator::ExistingChunkGuidInventory;
TMap< uint64, TArray< FGuid > > FBuildDataGenerator::ExistingChunkHashInventory;
bool FBuildDataGenerator::ExistingChunksEnumerated = false;
TMap< FSHAHashData, TArray< FString > > FBuildDataGenerator::ExistingFileInventory;
bool FBuildDataGenerator::ExistingFilesEnumerated = false;
FCriticalSection FBuildDataGenerator::SingleConcurrentBuildCS;

static void AddCustomFieldsToBuildManifest(const TMap<FString, FVariant>& CustomFields, IBuildManifestPtr BuildManifest)
{
	for (const auto& CustomField : CustomFields)
	{
		int32 VarType = CustomField.Value.GetType();
		if (VarType == EVariantTypes::Float || VarType == EVariantTypes::Double)
		{
			BuildManifest->SetCustomField(CustomField.Key, (double)CustomField.Value);
		}
		else if (VarType == EVariantTypes::Int8 || VarType == EVariantTypes::Int16 || VarType == EVariantTypes::Int32 || VarType == EVariantTypes::Int64 ||
			VarType == EVariantTypes::UInt8 || VarType == EVariantTypes::UInt16 || VarType == EVariantTypes::UInt32 || VarType == EVariantTypes::UInt64)
		{
			BuildManifest->SetCustomField(CustomField.Key, (int64)CustomField.Value);
		}
		else if (VarType == EVariantTypes::String)
		{
			BuildManifest->SetCustomField(CustomField.Key, CustomField.Value.GetValue<FString>());
		}
	}
}

static void FileAttributesMetaToMap(const FString& AttributesList, TMap<FString, FFileAttributes>& FileAttributesMap)
{
	GLog->Logf(TEXT("Parsing file attributes list:-"));
	checkf(AttributesList.Len() > 0, TEXT("Attributes File List was empty file"));
	const TCHAR Quote = TEXT('\"');
	const TCHAR EOFile = TEXT('\0');
	const TCHAR EOLine = TEXT('\n');

	const TCHAR* CharPtr = *AttributesList;
	while (*CharPtr != EOFile)
	{
		// Parse filename
		while (*CharPtr != Quote && *CharPtr != EOFile){ ++CharPtr; }
		if (*CharPtr == EOFile)
		{
			break;
		}
		const TCHAR* FilenameStart = ++CharPtr;
		while (*CharPtr != Quote && *CharPtr != EOFile && *CharPtr != EOLine){ ++CharPtr; }
		checkf(*CharPtr != EOFile, TEXT("Attributes File List: Unexpected end of file before next quote! Pos:%d"), CharPtr - *AttributesList);
		checkf(*CharPtr != EOLine, TEXT("Attributes File List: Unexpected end of line before next quote! Pos:%d"), CharPtr - *AttributesList);
		const TCHAR* FilenameEnd = CharPtr++;
		// Parse keywords
		while (*CharPtr != Quote && *CharPtr != EOFile && *CharPtr != EOLine){ ++CharPtr; }
		checkf(*CharPtr != Quote, TEXT("Attributes File List: Unexpected Quote before end of keywords! Pos:%d"), CharPtr - *AttributesList);
		const TCHAR* EndOfLine = CharPtr;
		// Grab info
		FString Filename = FString(FilenameEnd - FilenameStart, FilenameStart).Replace(TEXT("\\"), TEXT("/"));
		FString Keywords(EndOfLine - FilenameEnd, FilenameEnd);
		FFileAttributes FileAttributes;
		GLog->Logf(TEXT("    %s"), *Filename);
		if (Keywords.Contains(TEXT("readonly")))
		{
			FileAttributes.bReadOnly = true;
			GLog->Logf(TEXT("        readonly"), *Filename);
		}
		if (Keywords.Contains(TEXT("compressed")))
		{
			FileAttributes.bCompressed = true;
			GLog->Logf(TEXT("        compressed"), *Filename);
		}
		if (Keywords.Contains(TEXT("executable")))
		{
			FileAttributes.bUnixExecutable = true;
			GLog->Logf(TEXT("        executable"), *Filename);
		}
		if (!(FileAttributes.bReadOnly || FileAttributes.bCompressed || FileAttributes.bUnixExecutable))
		{
			GLog->Logf(TEXT("        none"), *Filename);
		}
		FileAttributesMap.Add(MoveTemp(Filename), FileAttributes);
	}

}

/* FBuildDataGenerator implementation
*****************************************************************************/
bool FBuildDataGenerator::GenerateChunksManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	// Output to log for builder info
	GLog->Logf(TEXT("Running Chunks Patch Generation for: %u:%s %s"), Settings.AppID, *Settings.AppName, *Settings.BuildVersion);

	// Take the build CS
	FScopeLock SingleConcurrentBuild( &SingleConcurrentBuildCS );

	// Create our chunk cache
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	FBuildGenerationChunkCache::Init(Cutoff);

	// Create a manifest
	FBuildPatchAppManifestRef BuildManifest = MakeShareable( new FBuildPatchAppManifest() );

	// Setup custom fields
	AddCustomFieldsToBuildManifest(Settings.CustomFields, BuildManifest);

	// Get the file attributes
	FString AttributesList;
	TMap<FString, FFileAttributes> FileAttributesMap;
	if (Settings.AttributeListFile.Len() > 0)
	{
		FFileHelper::LoadFileToString(AttributesList, *Settings.AttributeListFile);
		if (!AttributesList.IsEmpty())
		{
			FileAttributesMetaToMap(AttributesList, FileAttributesMap);
		}
		else
		{
			GLog->Logf(TEXT("WARNING: Attributes list file empty"));
		}
	}

	// Reset chunk inventory
	ExistingChunksEnumerated = false;
	ExistingChunkGuidInventory.Empty();
	ExistingChunkHashInventory.Empty();

	// Declare a build processor
	FBuildDataChunkProcessor DataProcessor(BuildManifest, Settings.RootDirectory);

	// Create a build streamer
	FBuildStream* BuildStream = new FBuildStream(Settings.RootDirectory, Settings.IgnoreListFile);

	// Set the basic details
	BuildManifest->Data->bIsFileData = false;
	BuildManifest->Data->AppID = Settings.AppID;
	BuildManifest->Data->AppName = Settings.AppName;
	BuildManifest->Data->BuildVersion = Settings.BuildVersion;
	BuildManifest->Data->LaunchExe = Settings.LaunchExe;
	BuildManifest->Data->LaunchCommand = Settings.LaunchCommand;
	BuildManifest->Data->PrereqName = Settings.PrereqName;
	BuildManifest->Data->PrereqPath = Settings.PrereqPath;
	BuildManifest->Data->PrereqArgs = Settings.PrereqArgs;

	// Create a data buffer
	const uint32 DataBufferSize = FBuildPatchData::ChunkDataSize;
	uint8* DataBuffer = new uint8[ DataBufferSize ];

	// We'll need a rolling hash for chunking
	FRollingHash< FBuildPatchData::ChunkDataSize >* RollingHash = new FRollingHash< FBuildPatchData::ChunkDataSize >();
	
	// Refers to how much data has been processed (into the FBuildDataProcessor)
	uint64 ProcessPos = 0;

	// Records the current file we are processing
	FString FileName;

	// And the current file's data left to process
	uint64 FileDataCount = 0;

	// Used to store data read lengths
	uint32 ReadLen = 0;

	// The last time we logged out data processed
	double LastProgressLog = FPlatformTime::Seconds();
	const double TimeGenStarted = LastProgressLog;

	// Loop through all data
	while ( !BuildStream->IsEndOfData() )
	{
		// Grab some data from the build stream
		ReadLen = BuildStream->DequeueData( DataBuffer, DataBufferSize );

		// A bool says if there's no more data to come from the Build Stream
		const bool bNoMoreData = BuildStream->IsEndOfData();

		// Refers to how much data from DataBuffer has been passed into the rolling hash
		uint32 DataBufferPos = 0;

		// Count how many times we pad the rolling hash with zero
		uint32 PaddedZeros = 0;

		// Process data while we have more
		while ( ( DataBufferPos < ReadLen ) || ( bNoMoreData && PaddedZeros < RollingHash->GetWindowSize() ) )
		{
			// Prime the rolling hash
			if( RollingHash->GetNumDataNeeded() > 0 )
			{
				if( DataBufferPos < ReadLen )
				{
					RollingHash->ConsumeByte( DataBuffer[ DataBufferPos++ ] );
				}
				else
				{
					RollingHash->ConsumeByte( 0 );
					++PaddedZeros;
				}
				// Keep looping until primed
				continue;
			}

			// Check if we recognized a chunk
			FGuid ChunkGuid;
			const uint64 WindowHash = RollingHash->GetWindowHash();
			const TRingBuffer< uint8, FBuildPatchData::ChunkDataSize >& WindowData = RollingHash->GetWindowData();
			bool ChunkRecognised = FindExistingChunkData( WindowHash, WindowData, ChunkGuid );
			if( ChunkRecognised )
			{
				// Process all bytes
				DataProcessor.PushChunk();
				const uint32 WindowDataSize = RollingHash->GetWindowSize() - PaddedZeros;
				for( uint32 i = 0; i < WindowDataSize; ++i )
				{
					const bool bStartOfFile = BuildStream->GetFileSpan( ProcessPos, FileName, FileDataCount );
					const bool bEndOfFile = FileDataCount <= 1;
					check( FileDataCount > 0 );// If FileDataCount is ever 0, it means this piece of data belongs to no file, so something is wrong
					DataProcessor.SkipKnownByte( WindowData[i], bStartOfFile, bEndOfFile, FileName );
					++ProcessPos;
					--FileDataCount;
				}
				uint8* SerialWindowData = new uint8[ FBuildPatchData::ChunkDataSize ];
				WindowData.Serialize( SerialWindowData );
				DataProcessor.PopChunk( WindowHash, SerialWindowData, ChunkGuid );
				delete[] SerialWindowData;

				// Clear
				RollingHash->Clear();
			}
			else
			{
				// Process one byte
				const bool bStartOfFile = BuildStream->GetFileSpan( ProcessPos, FileName, FileDataCount );
				const bool bEndOfFile = FileDataCount <= 1;
				DataProcessor.ProcessNewByte( WindowData.Bottom(), bStartOfFile, bEndOfFile, FileName );
				++ProcessPos;
				--FileDataCount;

				// Roll
				if( DataBufferPos < ReadLen )
				{
					RollingHash->RollForward( DataBuffer[ DataBufferPos++ ] );
				}
				else if( bNoMoreData )
				{
					RollingHash->RollForward( 0 );
					++PaddedZeros;
				}
			}

			// Log processed data
			if( ( FPlatformTime::Seconds() - LastProgressLog ) >= 10.0 )
			{
				LastProgressLog = FPlatformTime::Seconds();
				GLog->Logf( TEXT( "Processed %lld bytes." ), ProcessPos );
			}
		}
	}

	// The final chunk if any should be finished.
	// This also triggers the chunk writer thread to exit.
	DataProcessor.FinalChunk();

	// Handle empty files
	FSHA1 EmptyHasher;
	EmptyHasher.Final();
	const TArray< FString >& EmptyFileList = BuildStream->GetEmptyFiles();
	for (const auto& EmptyFile : EmptyFileList)
	{
		BuildManifest->Data->FileManifestList.Add(FFileManifestData());
		FFileManifestData& EmptyFileManifest = BuildManifest->Data->FileManifestList.Last();
		EmptyFileManifest.Filename = EmptyFile;
		EmptyHasher.GetHash(EmptyFileManifest.FileHash.Hash);
	}

	// Add chunk sizes
	const TMap<FGuid, int64>& ChunkFilesizes = DataProcessor.GetChunkFilesizes();
	for (FChunkInfoData& ChunkInfo : BuildManifest->Data->ChunkList)
	{
		if (ChunkFilesizes.Contains(ChunkInfo.Guid))
		{
			ChunkInfo.FileSize = ChunkFilesizes[ChunkInfo.Guid];
		}
	}

	// Fill out lookups
	BuildManifest->InitLookups();

	// Fill out the file attributes
	for (const auto& Entry : FileAttributesMap)
	{
		const FString& Filename = Entry.Key;
		const FFileAttributes& Attributes = Entry.Value;
		if (BuildManifest->FileManifestLookup.Contains(Filename))
		{
			FFileManifestData& FileManifest = *BuildManifest->FileManifestLookup[Filename];
			FileManifest.bIsReadOnly = Attributes.bReadOnly;
			FileManifest.bIsCompressed = Attributes.bCompressed;
			// Only overwrite unix exe if true
			if (Attributes.bUnixExecutable)
			{
				FileManifest.bIsUnixExecutable = Attributes.bUnixExecutable;
			}
		}
		else
		{
			GLog->Logf(TEXT("File Attributes: File not in build %s"), *Filename);
		}
	}

	// Save manifest into the cloud directory
	FString JsonFilename = FBuildPatchServicesModule::GetCloudDirectory() / FDefaultValueHelper::RemoveWhitespaces(BuildManifest->Data->AppName + BuildManifest->Data->BuildVersion) + TEXT(".manifest");
	BuildManifest->Data->ManifestFileVersion = EBuildPatchAppManifestVersion::GetLatestJsonVersion();
	BuildManifest->SaveToFile(JsonFilename, false);

	// Output to log for builder info
	GLog->Logf(TEXT("Saved manifest to %s"), *JsonFilename);

	// Clean up memory
	delete[] DataBuffer;
	delete BuildStream;
	delete RollingHash;
	FBuildGenerationChunkCache::Shutdown();

	// @TODO LSwift: Detect errors and return false on failure
	return true;
}

bool FBuildDataGenerator::GenerateFilesManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	// Output to log for builder info
	GLog->Logf(TEXT("Running Files Patch Generation for: %u:%s %s"), Settings.AppID, *Settings.AppName, *Settings.BuildVersion);

	// Take the build CS
	FScopeLock SingleConcurrentBuild( &SingleConcurrentBuildCS );

	// Create a manifest
	FBuildPatchAppManifestRef BuildManifest = MakeShareable( new FBuildPatchAppManifest() );

	// Setup custom fields
	AddCustomFieldsToBuildManifest(Settings.CustomFields, BuildManifest);

	// Get the file attributes
	FString AttributesList;
	TMap<FString, FFileAttributes> FileAttributesMap;
	if (Settings.AttributeListFile.Len() > 0)
	{
		FFileHelper::LoadFileToString(AttributesList, *Settings.AttributeListFile);
		if (!AttributesList.IsEmpty())
		{
			FileAttributesMetaToMap(AttributesList, FileAttributesMap);
		}
		else
		{
			GLog->Logf(TEXT("WARNING: Attributes list file empty"));
		}
	}

	// Reset file inventory
	ExistingFilesEnumerated = false;
	ExistingFileInventory.Empty();

	// Declare a build processor
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	FBuildDataFileProcessor DataProcessor(BuildManifest, Settings.RootDirectory, Cutoff);

	// Set the basic details
	BuildManifest->Data->bIsFileData = true;
	BuildManifest->Data->AppID = Settings.AppID;
	BuildManifest->Data->AppName = Settings.AppName;
	BuildManifest->Data->BuildVersion = Settings.BuildVersion;
	BuildManifest->Data->LaunchExe = Settings.LaunchExe;
	BuildManifest->Data->LaunchCommand = Settings.LaunchCommand;
	BuildManifest->Data->PrereqName = Settings.PrereqName;
	BuildManifest->Data->PrereqPath = Settings.PrereqPath;
	BuildManifest->Data->PrereqArgs = Settings.PrereqArgs;

	// Create a data buffer
	uint8* FileReadBuffer = new uint8[ FileBufferSize ];
	
	// Refers to how much data has been processed (into the FBuildDataFileProcessor)
	uint64 ProcessPos = 0;

	// Find all files
	TArray< FString > AllFiles;
	IFileManager::Get().FindFilesRecursive(AllFiles, *Settings.RootDirectory, TEXT("*.*"), true, false);
	AllFiles.Sort();

	// Remove the files that appear in an ignore list
	FBuildDataGenerator::StripIgnoredFiles(AllFiles, Settings.RootDirectory, Settings.IgnoreListFile);

	// Loop through all files
	for( auto FileIt = AllFiles.CreateConstIterator(); FileIt; ++FileIt )
	{
		const FString& FileName = *FileIt;
		// Read the file
		FArchive* FileReader = IFileManager::Get().CreateFileReader( *FileName );
		if( FileReader != NULL )
		{
			// Make SourceFile the format we want it in and start a new file
			FString SourceFile = FileName;
			FPaths::MakePathRelativeTo(SourceFile, *(Settings.RootDirectory + TEXT("/")));
			int64 FileSize = FileReader->TotalSize();
			if( FileSize < 0 )
			{
				// Skip potential error ( INDEX_NONE == -1 )
				continue;
			}
			DataProcessor.BeginFile( SourceFile );
			while( !FileReader->AtEnd() )
			{
				const int64 SizeLeft = FileSize - FileReader->Tell();
				const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
				ProcessPos += ReadLen;
				FileReader->Serialize( FileReadBuffer, ReadLen );
				// Copy into data stream
				DataProcessor.ProcessFileData( FileReadBuffer, ReadLen );
			}
			FileReader->Close();
			delete FileReader;
			DataProcessor.EndFile();
		}
		else
		{
			// @TODO LSwift: Handle File error?
		}
	}

	// Fill out lookups
	BuildManifest->InitLookups();

	// Fill out the file attributes
	for (const auto& Entry : FileAttributesMap)
	{
		const FString& Filename = Entry.Key;
		const FFileAttributes& Attributes = Entry.Value;
		if (BuildManifest->FileManifestLookup.Contains(Filename))
		{
			FFileManifestData& FileManifest = *BuildManifest->FileManifestLookup[Filename];
			FileManifest.bIsReadOnly = Attributes.bReadOnly;
			FileManifest.bIsCompressed = Attributes.bCompressed;
			// Only overwrite unix exe if true
			if (Attributes.bUnixExecutable)
			{
				FileManifest.bIsUnixExecutable = Attributes.bUnixExecutable;
			}
		}
		else
		{
			GLog->Logf(TEXT("File Attributes: File not in build %s"), *Filename);
		}
	}

	// Save manifest into the cloud directory
	FString JsonFilename = FBuildPatchServicesModule::GetCloudDirectory() / FDefaultValueHelper::RemoveWhitespaces(BuildManifest->Data->AppName + BuildManifest->Data->BuildVersion) + TEXT(".manifest");
	BuildManifest->Data->ManifestFileVersion = EBuildPatchAppManifestVersion::GetLatestJsonVersion();
	BuildManifest->SaveToFile(JsonFilename, false);

	// Output to log for builder info
	GLog->Logf(TEXT("Saved manifest to %s"), *JsonFilename);

	// Clean up memory
	delete[] FileReadBuffer;

	// @TODO LSwift: Detect errors and return false on failure
	return true;
}

bool FBuildDataGenerator::FindExistingChunkData( const uint64& ChunkHash, const uint8* ChunkData, FGuid& ChunkGuid )
{
	// Quick code hack
	TRingBuffer< uint8, FBuildPatchData::ChunkDataSize > ChunkDataRing;
	const uint32 ChunkDataLen = FBuildPatchData::ChunkDataSize;
	ChunkDataRing.Enqueue( ChunkData, ChunkDataLen );
	return FindExistingChunkData( ChunkHash, ChunkDataRing, ChunkGuid );
}

bool FBuildDataGenerator::FindExistingChunkData( const uint64& ChunkHash, const TRingBuffer< uint8, FBuildPatchData::ChunkDataSize >& ChunkData, FGuid& ChunkGuid )
{
	bool bFoundMatchingChunk = false;

	// Perform an inventory on Cloud chunks if not already done
	if( ExistingChunksEnumerated == false )
	{
		IFileManager& FileManager = IFileManager::Get();
		FString JSONOutput;
		TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > DebugWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create( &JSONOutput );
		DebugWriter->WriteObjectStart();

		// Find all manifest files
		const FString CloudDir = FBuildPatchServicesModule::GetCloudDirectory();
		if (FileManager.DirectoryExists(*CloudDir))
		{
			const double StartEnumerate = FPlatformTime::Seconds();
			TArray<FString> AllManifests;
			GLog->Logf(TEXT("BuildDataGenerator: Enumerating Manifests from %s"), *CloudDir);
			FileManager.FindFiles(AllManifests, *(CloudDir / TEXT("*.manifest")), true, false);
			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			GLog->Logf(TEXT("BuildDataGenerator: Found %d manifests in %.1f seconds"), AllManifests.Num(), EnumerateTime);

			// Load all manifest files
			uint64 NumChunksFound = 0;
			const double StartLoadAllManifest = FPlatformTime::Seconds();
			for (const auto& ManifestFile : AllManifests)
			{
				// Determine chunks from manifest file
				const FString ManifestFilename = CloudDir / ManifestFile;
				FBuildPatchAppManifestRef BuildManifest = MakeShareable(new FBuildPatchAppManifest());
				const double StartLoadManifest = FPlatformTime::Seconds();
				if (BuildManifest->LoadFromFile(ManifestFilename))
				{
					const double LoadManifestTime = FPlatformTime::Seconds() - StartLoadManifest;
					GLog->Logf(TEXT("BuildDataGenerator: Loaded %s in %.1f seconds"), *ManifestFile, LoadManifestTime);
					if(!BuildManifest->IsFileDataManifest())
					{
						TArray<FGuid> ChunksReferenced;
						BuildManifest->GetDataList(ChunksReferenced);
						for (const auto& ReferencedChunkGuid : ChunksReferenced)
						{
							uint64 ReferencedChunkHash;
							if (BuildManifest->GetChunkHash(ReferencedChunkGuid, ReferencedChunkHash))
							{
								if (ReferencedChunkHash != 0)
								{
									TArray< FGuid >& HashChunkList = ExistingChunkHashInventory.FindOrAdd(ReferencedChunkHash);
									if (!HashChunkList.Contains(ReferencedChunkGuid))
									{
										++NumChunksFound;
										HashChunkList.Add(ReferencedChunkGuid);
									}
								}
								else
								{
									GLog->Logf(TEXT("BuildDataGenerator: INFO: Ignored an existing chunk %s with a failed hash value of zero to avoid performance problems while chunking"), *ReferencedChunkGuid.ToString());
								}
							}
							else
							{
								GLog->Logf(TEXT("BuildDataGenerator: WARNING: Missing chunk hash for %s in manifest %s"), *ReferencedChunkGuid.ToString(), *ManifestFile);
							}
						}
					}
					else
					{
						GLog->Logf(TEXT("BuildDataGenerator: INFO: Ignoring non-chunked manifest %s"), *ManifestFilename);
					}
				}
				else
				{
					GLog->Logf(TEXT("BuildDataGenerator: WARNING: Could not read Manifest file. Data recognition will suffer (%s)"), *ManifestFilename);
				}
			}
			const double LoadAllManifestTime = FPlatformTime::Seconds() - StartLoadAllManifest;
			GLog->Logf(TEXT("BuildDataGenerator: Used %d manifests to enumerate %llu chunks in %.1f seconds"), AllManifests.Num(), NumChunksFound, LoadAllManifestTime);
		}
		else
		{
			GLog->Logf(TEXT("BuildDataGenerator: Cloud directory does not exist: %s"), *CloudDir);
		}

		ExistingChunksEnumerated = true;
	}

	// Do we have a chunk matching this data?
	if( ExistingChunkHashInventory.Num() > 0 )
	{
		TArray< FGuid >* ChunkList = ExistingChunkHashInventory.Find( ChunkHash );
		if( ChunkList != NULL )
		{
			// We need to load each chunk in this list and compare data
			for( auto ChunkIt = ChunkList->CreateConstIterator(); ChunkIt && !bFoundMatchingChunk ; ++ChunkIt)
			{
				FGuid Guid = *ChunkIt;
				if (!ExistingChunkGuidInventory.Contains(Guid))
				{
					ExistingChunkGuidInventory.Add(Guid, DiscoverChunkFilename(Guid, ChunkHash));
				}
				const FString& SourceFile = ExistingChunkGuidInventory[ Guid ];
				// Read the file
				uint8* TempChunkData = new uint8[ FBuildPatchData::ChunkDataSize ];
				ChunkData.Serialize( TempChunkData );
				bool bChunkIsUsable = true;
				if( CompareDataToChunk( SourceFile, TempChunkData, Guid, bChunkIsUsable ) )
				{
					// We have a chunk match!!
					bFoundMatchingChunk = true;
					ChunkGuid = Guid;
				}
				// Check if this chunk should be dumped
				if(!bChunkIsUsable)
				{
					GLog->Logf(TEXT("BuildDataGenerator: Chunk %s unusable, removed from inventory."), *ChunkGuid.ToString());
					ChunkList->Remove(Guid);
					--ChunkIt;
				}
				delete[] TempChunkData;
			}
		}
	}

	return bFoundMatchingChunk;
}

FString FBuildDataGenerator::DiscoverChunkFilename(const FGuid& ChunkGuid, const uint64& ChunkHash)
{
	static double AccumTime = 0.0;
	const double StartDiscovery = FPlatformTime::Seconds();
	const FString CloudDir = FBuildPatchServicesModule::GetCloudDirectory();
	FString ChunkFilenameChecked;
	for (EBuildPatchAppManifestVersion::Type VersionCounter = static_cast<EBuildPatchAppManifestVersion::Type>(EBuildPatchAppManifestVersion::LatestPlusOne - 1);
		VersionCounter >= EBuildPatchAppManifestVersion::Original;
		VersionCounter = static_cast<EBuildPatchAppManifestVersion::Type>(VersionCounter - 1))
	{
		const FString ChunkFilename = VersionCounter < EBuildPatchAppManifestVersion::DataFileRenames ?
			FBuildPatchUtils::GetChunkOldFilename(CloudDir, ChunkGuid) :
			FBuildPatchUtils::GetChunkNewFilename(VersionCounter, CloudDir, ChunkGuid, ChunkHash);
		if (ChunkFilenameChecked != ChunkFilename)
		{
			ChunkFilenameChecked = ChunkFilename;
			if (FPaths::FileExists(ChunkFilename))
			{
				const double DiscoveryTime = FPlatformTime::Seconds() - StartDiscovery;
				AccumTime += DiscoveryTime;
				GLog->Logf(TEXT("BuildDataGenerator: DiscoverChunkFilename: Chunk %s found in %f secs. Accum time %.2f secs"), *ChunkGuid.ToString(), DiscoveryTime, AccumTime);
				return ChunkFilename;
			}
		}
	}
	GLog->Logf(TEXT("BuildDataGenerator: DiscoverChunkFilename: Chunk %s not found"), *ChunkGuid.ToString());
	return TEXT("");
}

bool FBuildDataGenerator::FindExistingFileData(const FString& InSourceFile, const FSHAHashData& InFileHash, const FDateTime& DataThresholdTime, FGuid& OutFileGuid)
{
	bool bFoundMatchingFile = false;

	// Perform an inventory on Cloud files if not already done
	if( ExistingFilesEnumerated == false )
	{
		IFileManager& FileManager = IFileManager::Get();
		FString JSONOutput;
		TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > DebugWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create( &JSONOutput );
		DebugWriter->WriteObjectStart();

		TArray< FGuid > FoundFiles;

		// The directory containing old filename version files
		const FString CloudFileDir = FBuildPatchServicesModule::GetCloudDirectory() / TEXT( "Files" );
		// The directory containing new filename version files
		const FString CloudFile2Dir = FBuildPatchServicesModule::GetCloudDirectory() / TEXT( "FilesV2" );

		GLog->Logf( TEXT( "BuildDataGenerator: Enumerating Files from %s" ), *CloudFile2Dir );
		if( FileManager.DirectoryExists( *CloudFile2Dir ) )
		{
			const double StartEnumerate = FPlatformTime::Seconds();

			// Find all files
			TArray<FString> AllFiles;
			FileManager.FindFilesRecursive( AllFiles, *CloudFile2Dir, TEXT("*.file"), true, false );

			for( auto FileIt = AllFiles.CreateConstIterator(); FileIt; ++FileIt )
			{
				FString SourceFile = *FileIt;
				FSHAHashData FoundHash;
				FGuid FoundGuid;
				FBuildPatchUtils::GetFileDetailFromNewFilename( SourceFile, FoundGuid, FoundHash );

				// Add to inventory
				FoundFiles.Add( FoundGuid );
				TArray< FString >& FileList = ExistingFileInventory.FindOrAdd( FoundHash );
				FileList.Add( SourceFile );

				const void* HashBuffer = &FoundHash;
				DebugWriter->WriteValue( FoundGuid.ToString(), FString::FromBlob( static_cast<const uint8*>( HashBuffer ), sizeof( FoundHash ) ) );
			}

			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			GLog->Logf( TEXT( "BuildDataGenerator: Found %d new name files in %.1f seconds" ), AllFiles.Num(), EnumerateTime );
		}
		else
		{
			GLog->Logf( TEXT( "BuildDataGenerator: Cloud directory does not exist: %s" ), *CloudFile2Dir );
		}

		GLog->Logf( TEXT( "BuildDataGenerator: Enumerating Files From %s" ), *CloudFileDir );
		if( IFileManager::Get().DirectoryExists( *CloudFileDir ) )
		{
			const double StartEnumerate = FPlatformTime::Seconds();
			const int32 PreviousNumFiles = FoundFiles.Num();

			// Find all files
			TArray<FString> AllFiles;
			IFileManager::Get().FindFilesRecursive( AllFiles, *CloudFileDir, TEXT("*.file"), true, false );

			for( auto FileIt = AllFiles.CreateConstIterator(); FileIt; ++FileIt )
			{
				FString SourceFile = *FileIt;

				// Skip any for GUID we already found
				FGuid FoundGuid;
				FBuildPatchUtils::GetGUIDFromFilename( SourceFile, FoundGuid );
				if( FoundFiles.Contains( FoundGuid ) )
				{
					continue;
				}

				// Read the file
				FArchive* FileReader = IFileManager::Get().CreateFileReader( *SourceFile );
				if( FileReader != NULL )
				{
					// Read the header
					FChunkHeader Header;
					*FileReader << Header;

					// Check magic
					if( Header.IsValidMagic() && Header.HashType == FChunkHeader::HASH_SHA1 )
					{
						TArray< FString >& FileList = ExistingFileInventory.FindOrAdd( Header.SHAHash );
						FileList.Add( SourceFile );
						FoundFiles.Add( Header.Guid );

						const void* HashBuffer = &Header.SHAHash;
						DebugWriter->WriteValue( Header.Guid.ToString(), FString::FromBlob( static_cast<const uint8*>( HashBuffer ), sizeof( Header.SHAHash ) ) );
					}
					else
					{
						GLog->Logf( TEXT( "BuildDataGenerator: Failed magic/hashtype check on file [%d:%d] %s" ), Header.Magic, Header.HashType, *SourceFile );
					}

					FileReader->Close();
					delete FileReader;
				}
				else
				{
					GLog->Logf( TEXT( "BuildDataGenerator: Failed to read chunk %s" ), *SourceFile );
				}
			}

			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			const int32 NewNumFiles = FoundFiles.Num();
			GLog->Logf( TEXT( "BuildDataGenerator: Found %d extra old files in %.1f seconds" ), NewNumFiles - PreviousNumFiles, EnumerateTime );
		}
		else
		{
			GLog->Logf( TEXT( "BuildDataGenerator: Cloud directory does not exist: %s" ), *CloudFileDir );
		}

		DebugWriter->WriteObjectEnd();
		DebugWriter->Close();

		FArchive* FileOut = IFileManager::Get().CreateFileWriter( *( CloudFileDir + TEXT( "DebugFileList.txt" ) ) );
		if( FileOut != NULL )
		{
			FileOut->Serialize(TCHAR_TO_ANSI(*JSONOutput), JSONOutput.Len());
			FileOut->Close();
			delete FileOut;
		}

		ExistingFilesEnumerated = true;
	}

	// Do we have a file matching this data?
	if( ExistingFileInventory.Num() > 0 )
	{
		TArray< FString >* FileList = ExistingFileInventory.Find( InFileHash );
		if( FileList != NULL )
		{
			// We need to load each chunk in this list and compare data
			for( auto FileIt = FileList->CreateConstIterator(); FileIt && !bFoundMatchingFile ; ++FileIt)
			{
				FString CloudFilename = *FileIt;
				// Check the file date
				FDateTime ModifiedDate = IFileManager::Get().GetTimeStamp( *CloudFilename );
				if (ModifiedDate < DataThresholdTime)
				{
					// We don't want to reuse this file's Guid, as it's older than any existing files we want to consider
					continue;
				}
				// Compare the files
				FArchive* SourceFile = IFileManager::Get().CreateFileReader( *InSourceFile );
				FArchive* FoundFile = IFileManager::Get().CreateFileReader( *CloudFilename );
				if( SourceFile != NULL && FoundFile != NULL)
				{
					FChunkHeader FoundHeader;
					*FoundFile << FoundHeader;
					const int64 SourceFileSize = SourceFile->TotalSize();
					const int64 FoundFileSize = FoundFile->TotalSize();
					if( SourceFileSize == FoundHeader.DataSize )
					{
						// Currently only support stored raw!
						check( FoundHeader.StoredAs == FChunkHeader::STORED_RAW );
						// Makes no sense here for the sizes to not add up
						check( FoundFileSize == ( FoundHeader.DataSize + FoundHeader.HeaderSize ) );
						// Move FoundFile to start of file data
						FoundFile->Seek( FoundHeader.HeaderSize );
						// Compare
						bool bSameData = true;
						uint8* TempSourceBuffer = new uint8[ FileBufferSize ];
						uint8* TempFoundBuffer = new uint8[ FileBufferSize ];
						while ( bSameData && ( ( SourceFile->AtEnd() || FoundFile->AtEnd() ) == false ) )
						{
							const int64 SizeLeft = SourceFileSize - SourceFile->Tell();
							const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
							SourceFile->Serialize( TempSourceBuffer, ReadLen );
							FoundFile->Serialize( TempFoundBuffer, ReadLen );
							bSameData = FMemory::Memcmp( TempSourceBuffer, TempFoundBuffer, ReadLen ) == 0;
						}
						delete[] TempSourceBuffer;
						delete[] TempFoundBuffer;
						// Did we match?
						if( bSameData && SourceFile->AtEnd() && FoundFile->AtEnd() )
						{
							// Yes we did!
							bFoundMatchingFile = true;
							OutFileGuid = FoundHeader.Guid;
						}
					}
				}
				if( SourceFile != NULL )
				{
					SourceFile->Close();
					delete SourceFile;
				}
				if( FoundFile != NULL )
				{
					FoundFile->Close();
					delete FoundFile;
				}
			}
		}
	}

	return bFoundMatchingFile;
}

bool FBuildDataGenerator::SaveOutFileData(const FString& SourceFile, const FSHAHashData& FileHash, const FGuid& FileGuid)
{
	bool bAlreadySaved = false;
	bool bSuccess = false;
	IFileManager& FileManager = IFileManager::Get();

	const FString NewFilename = FBuildPatchUtils::GetFileNewFilename( EBuildPatchAppManifestVersion::GetLatestVersion(), FBuildPatchServicesModule::GetCloudDirectory(), FileGuid, FileHash );
	bAlreadySaved = FPaths::FileExists( NewFilename );

#if SAVE_OLD_FILEDATA_FILENAMES
	const FString OldFilename = FBuildPatchUtils::GetFileOldFilename( FBuildPatchServicesModule::GetCloudDirectory(), FileGuid );
	bAlreadySaved = bAlreadySaved && FPaths::FileExists( OldFilename );
#endif

	if( !bAlreadySaved )
	{
		FArchive* FileOut = FileManager.CreateFileWriter( *NewFilename );
		FArchive* FileIn = FileManager.CreateFileReader( *SourceFile );

		bSuccess = FileOut != NULL && FileIn != NULL;

#if SAVE_OLD_FILEDATA_FILENAMES
		FArchive* OldFileOut = FileManager.CreateFileWriter( *OldFilename );
		bSuccess = bSuccess && OldFileOut != NULL;
#endif

		if( bSuccess )
		{
			const int64 FileSize = FileIn->TotalSize();

			// LSwift: No support for too large files currently
			check( FileSize <= 0xFFFFFFFF );

			// Setup Header
			FChunkHeader Header;
			*FileOut << Header;
			Header.HeaderSize = FileOut->Tell();
			Header.DataSize = FileSize;
			Header.Guid = FileGuid;
			Header.HashType = FChunkHeader::HASH_SHA1;
			Header.SHAHash = FileHash;
			Header.StoredAs = FChunkHeader::STORED_RAW;

			// Write out file
			FileOut->Seek( 0 );
			*FileOut << Header;

#if SAVE_OLD_FILEDATA_FILENAMES
			*OldFileOut << Header;
#endif

			uint8* FileDataBuffer = new uint8[ FileBufferSize ];
			while( !FileIn->AtEnd() )
			{
				const int64 SizeLeft = FileSize - FileIn->Tell();
				const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
				FileIn->Serialize( FileDataBuffer, ReadLen );
				FileOut->Serialize( FileDataBuffer, ReadLen );

#if SAVE_OLD_FILEDATA_FILENAMES
				OldFileOut->Serialize( FileDataBuffer, ReadLen );
#endif

			}
			delete[] FileDataBuffer;
		}

		// Close files
		if( FileIn != NULL )
		{
			FileIn->Close();
			delete FileIn;
		}
		if( FileOut != NULL )
		{
			FileOut->Close();
			delete FileOut;
		}

#if SAVE_OLD_FILEDATA_FILENAMES
		if( OldFileOut != NULL )
		{
			OldFileOut->Close();
			delete OldFileOut;
		}
#endif

	}

	return bSuccess;
}

bool FBuildDataGenerator::CompareDataToChunk( const FString& ChunkFilePath, uint8* ChunkData, FGuid& ChunkGuid, bool& OutSourceChunkIsValid )
{
	bool bMatching = false;

	// Read the file
	TSharedRef< FBuildGenerationChunkCache::FChunkReader > ChunkReader = FBuildGenerationChunkCache::Get().GetChunkReader( ChunkFilePath );
	if( ChunkReader->IsValidChunk() )
	{
		ChunkGuid = ChunkReader->GetChunkGuid();
		// Default true
		bMatching = true;
		// Compare per small block (for early outing!)
		const uint32 CompareSize = 64;
		uint8* ReadBuffer;
		uint32 NumCompared = 0;
		while( bMatching && ChunkReader->BytesLeft() > 0 && NumCompared < FBuildPatchData::ChunkDataSize )
		{
			const uint32 ReadLen = FMath::Min< uint32 >( CompareSize, ChunkReader->BytesLeft() );
			ChunkReader->ReadNextBytes( &ReadBuffer, ReadLen );
			bMatching = FMemory::Memcmp( &ChunkData[ NumCompared ], ReadBuffer, ReadLen ) == 0;
			NumCompared += ReadLen;
		}
	}

	// Set chunk valid state after loading in case reading discovered bad or ignorable data
	OutSourceChunkIsValid = ChunkReader->IsValidChunk();

	return bMatching;
}

void FBuildDataGenerator::StripIgnoredFiles( TArray< FString >& AllFiles, const FString& DepotDirectory, const FString& IgnoreListFile )
{
	struct FRemoveMatchingStrings
	{ 
		const TSet<FString>& IgnoreList;
		FRemoveMatchingStrings( const TSet<FString>& InIgnoreList )
			: IgnoreList(InIgnoreList) {}

		bool operator()(const FString& RemovalCandidate) const
		{
			const bool bRemove = IgnoreList.Contains(RemovalCandidate);
			if (bRemove)
			{
				GLog->Logf(TEXT("    - %s"), *RemovalCandidate);
			}
			return bRemove;
		}
	};

	GLog->Logf(TEXT("Stripping ignorable files"));
	const int32 OriginalNumFiles = AllFiles.Num();
	FString IgnoreFileList = TEXT( "" );
	FFileHelper::LoadFileToString( IgnoreFileList, *IgnoreListFile );
	TArray< FString > IgnoreFiles;
	IgnoreFileList.ParseIntoArray( IgnoreFiles, TEXT( "\r\n" ), true );

	// Normalize all paths first
	for (FString& Filename : AllFiles)
	{
		FPaths::NormalizeFilename(Filename);
	}
	for (FString& Filename : IgnoreFiles)
	{
		int32 TabLocation = Filename.Find(TEXT("\t"));
		if (TabLocation != INDEX_NONE)
		{
			// Strip tab deliminated timestamp if it exists
			Filename = Filename.Left(TabLocation);
		}
		Filename = DepotDirectory / Filename;
		FPaths::NormalizeFilename(Filename);
	}

	// Convert ignore list to set
	TSet<FString> IgnoreSet(MoveTemp(IgnoreFiles));

	// Filter file list
	FRemoveMatchingStrings FileFilter(IgnoreSet);
	AllFiles.RemoveAll(FileFilter);

	const int32 NewNumFiles = AllFiles.Num();
	GLog->Logf( TEXT( "Stripped %d ignorable file(s)" ), ( OriginalNumFiles - NewNumFiles ) );
}

#endif //WITH_BUILDPATCHGENERATION

