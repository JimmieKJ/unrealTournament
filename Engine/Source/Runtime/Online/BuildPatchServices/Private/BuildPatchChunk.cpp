// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchChunk.cpp: Implements classes involved with chunks for the build system.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

// The chunk header magic codeword, for quick checking that the opened file is a chunk file.
#define CHUNK_HEADER_MAGIC		0xB1FE3AA2

// The chunk header version number.
#define CHUNK_HEADER_VERSION	2

/* FSHAHashData implementation
*****************************************************************************/
FSHAHashData::FSHAHashData()
{
	FMemory::Memset(Hash, 0, FSHA1::DigestSize);
}

bool FSHAHashData::operator==(const FSHAHashData& Other) const
{
	return FMemory::Memcmp(Hash, Other.Hash, FSHA1::DigestSize) == 0;
}

bool FSHAHashData::operator!=(const FSHAHashData& Other) const
{
	return !(*this == Other);
}

FString FSHAHashData::ToString() const
{
	return BytesToHex(Hash, FSHA1::DigestSize);
}

/* FChunkHeader implementation
*****************************************************************************/
FChunkHeader::FChunkHeader()
	: Magic(CHUNK_HEADER_MAGIC)
	, Version(CHUNK_HEADER_VERSION)
	, HashType(HASH_ROLLING)
{

}

const bool FChunkHeader::IsValidMagic() const
{
	return Magic == CHUNK_HEADER_MAGIC;
}

FArchive& operator<<(FArchive& Ar, FChunkHeader& Header)
{
	// The constant sizes for each version of a header struct. Must be updated
	// If new member variables are added the version MUST be bumped and handled properly here,
	// and these values must never change.
	static const uint32 Version1Size = 41;
	static const uint32 Version2Size = 62;
	// Calculate how much space left in the archive for reading data ( will be 0 when writing )
	const int64 ArchiveSizeLeft = Ar.TotalSize() - Ar.Tell();
	// Make sure the archive has enough data to read from, or we are saving instead.
	bool bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= Version1Size);
	if (bSuccess)
	{
		Ar << Header.Magic
			<< Header.Version
			<< Header.HeaderSize
			<< Header.DataSize
			<< Header.Guid
			<< Header.RollingHash
			<< Header.StoredAs;

		// From version 2, we have a hash type choice. Previous versions default as only rolling
		if (Header.Version >= 2)
		{
			bSuccess = Ar.IsSaving() || (ArchiveSizeLeft >= Version2Size);
			if (bSuccess)
			{
				Ar.Serialize(Header.SHAHash.Hash, FSHA1::DigestSize);
				Ar << Header.HashType;
			}
		}
	}

	// If we had a size error, zero out the header values
	if (!bSuccess)
	{
		Header.Magic = 0;
		Header.Version = 0;
		Header.HeaderSize = 0;
		Header.DataSize = 0;
		Header.Guid.Invalidate();
		Header.RollingHash = 0;
		Header.StoredAs = 0;
	}

	return Ar;
}

/* FChunkFile implementation
*****************************************************************************/
FChunkFile::FChunkFile( const uint32& InReferenceCount, const bool& bInIsFromDisk )
	: bIsFromDisk( bInIsFromDisk )
	, ReferenceCount(InReferenceCount)
{
	LastAccessTime = FPlatformTime::Seconds();
}

void FChunkFile::GetDataLock( uint8** OutChunkData, FChunkHeader** OutChunkHeader )
{
	ThreadLock.Lock();
	if( OutChunkData )
	{
		(*OutChunkData) = ChunkData;
	}
	if( OutChunkHeader )
	{
		(*OutChunkHeader) = &ChunkHeader;
	}
}

void FChunkFile::ReleaseDataLock()
{
	LastAccessTime = FPlatformTime::Seconds();
	ThreadLock.Unlock();
}

const uint32 FChunkFile::GetRefCount() const
{
	return ReferenceCount;
}

uint32 FChunkFile::Dereference()
{
	FScopeLock ScopeLock( &ThreadLock );
	check( ReferenceCount > 0 );
	return --ReferenceCount;
}

const double FChunkFile::GetLastAccessTime() const
{
	return LastAccessTime;
}

#if WITH_BUILDPATCHGENERATION

/* FQueuedChunkWriter implementation
*****************************************************************************/
FChunkWriter::FQueuedChunkWriter::FQueuedChunkWriter()
{
	bMoreChunks = true;
}

FChunkWriter::FQueuedChunkWriter::~FQueuedChunkWriter()
{
	for( auto QueuedIt = ChunkFileQueue.CreateConstIterator(); QueuedIt; ++QueuedIt)
	{
		FChunkFile* ChunkFile = *QueuedIt;
		delete ChunkFile;
	}
	ChunkFileQueue.Empty();
}

bool FChunkWriter::FQueuedChunkWriter::Init()
{
	IFileManager::Get().MakeDirectory( *ChunkDirectory, true );
	return IFileManager::Get().DirectoryExists( *ChunkDirectory );
}

uint32 FChunkWriter::FQueuedChunkWriter::Run()
{
	// Loop until there's no more chunks
	while ( ShouldBeRunning() )
	{
		FChunkFile* ChunkFile = GetNextChunk();
		if( ChunkFile != NULL )
		{
			const FGuid& ChunkGuid = ChunkFile->ChunkHeader.Guid;
			const uint64& ChunkHash = ChunkFile->ChunkHeader.RollingHash;
#if SAVE_OLD_CHUNKDATA_FILENAMES
			const FString OldChunkFilename = FBuildPatchUtils::GetChunkOldFilename( ChunkDirectory, ChunkGuid );
#endif
			const FString NewChunkFilename = FBuildPatchUtils::GetChunkNewFilename( EBuildPatchAppManifestVersion::GetLatestVersion(), ChunkDirectory, ChunkGuid, ChunkHash );

			// To be a bit safer, make a few attempts at writing chunks
			int32 RetryCount = 5;
			bool bChunkSaveSuccess = false;
			while ( RetryCount > 0 )
			{
				// Write out chunks
				bChunkSaveSuccess = WriteChunkData( NewChunkFilename, ChunkFile, ChunkGuid );
#if SAVE_OLD_CHUNKDATA_FILENAMES
				bChunkSaveSuccess = bChunkSaveSuccess && WriteChunkData( OldChunkFilename, ChunkFile, ChunkGuid );
#endif
				// Check success
				if( bChunkSaveSuccess )
				{
					RetryCount = 0;
				}
				else
				{
					// Retry after a second if failed
					--RetryCount;
					FPlatformProcess::Sleep( 1.0f );
				}
			}

			// If we really could not save out chunk data successfully, this build will never work, so panic flush logs and then cause a hard error.
			if( !bChunkSaveSuccess )
			{
				GLog->PanicFlushThreadedLogs();
				check( bChunkSaveSuccess );
			}

			// Delete the data memory
			delete ChunkFile;
		}
		FPlatformProcess::Sleep( 0.0f );
	}
	return 0;
}

const bool FChunkWriter::FQueuedChunkWriter::WriteChunkData(const FString& ChunkFilename, FChunkFile* ChunkFile, const FGuid& ChunkGuid)
{
	// Chunks are saved with GUID, so if a file already exists it will never be different.
	// Skip with return true if already exists
	if( FPaths::FileExists( ChunkFilename ) )
	{
		const int64 ChunkFilesSize = IFileManager::Get().FileSize(*ChunkFilename);
		ChunkFileSizesCS.Lock();
		ChunkFileSizes.Add(ChunkGuid, ChunkFilesSize);
		ChunkFileSizesCS.Unlock();
		return true;
	}
	FArchive* FileOut = IFileManager::Get().CreateFileWriter( *ChunkFilename );
	bool bSuccess = FileOut != NULL;
	if( bSuccess )
	{
		// Setup to handle compression
		bool bDataIsCompressed = true;
		uint8* ChunkDataSource = ChunkFile->ChunkData;
		int32 ChunkDataSourceSize = FBuildPatchData::ChunkDataSize;
		TArray< uint8 > TempCompressedData;
		TempCompressedData.Empty( ChunkDataSourceSize );
		TempCompressedData.AddUninitialized( ChunkDataSourceSize );
		int32 CompressedSize = ChunkDataSourceSize;

		// Compressed can increase in size, but the function will return as failure in that case
		// we can allow that to happen since we would not keep larger compressed data anyway.
		bDataIsCompressed = FCompression::CompressMemory(
			static_cast< ECompressionFlags >( COMPRESS_ZLIB | COMPRESS_BiasMemory ),
			TempCompressedData.GetData(),
			CompressedSize,
			ChunkFile->ChunkData,
			FBuildPatchData::ChunkDataSize );

		// If compression succeeded, set data vars
		if( bDataIsCompressed )
		{
			ChunkDataSource = TempCompressedData.GetData();
			ChunkDataSourceSize = CompressedSize;
		}

		// Setup Header
		FChunkHeader& Header = ChunkFile->ChunkHeader;
		*FileOut << Header;
		Header.HeaderSize = FileOut->Tell();
		Header.StoredAs = bDataIsCompressed ? FChunkHeader::STORED_COMPRESSED : FChunkHeader::STORED_RAW;
		Header.DataSize = ChunkDataSourceSize;
		Header.HashType = FChunkHeader::HASH_ROLLING;

		// Write out files
		FileOut->Seek( 0 );
		*FileOut << Header;
		FileOut->Serialize( ChunkDataSource, ChunkDataSourceSize );
		const int64 ChunkFilesSize = FileOut->TotalSize();
		FileOut->Close();

		ChunkFileSizesCS.Lock();
		ChunkFileSizes.Add(ChunkGuid, ChunkFilesSize);
		ChunkFileSizesCS.Unlock();

		bSuccess = !FileOut->GetError();

		delete FileOut;
	}
	// Log errors
	if( !bSuccess )
	{
		GLog->Logf( TEXT( "BuildPatchServices: Error: Could not save out generated chunk file %s" ), *ChunkFilename );
	}
	return bSuccess;
}

const bool FChunkWriter::FQueuedChunkWriter::ShouldBeRunning()
{
	bool rtn;
	MoreChunksCS.Lock();
	rtn = bMoreChunks;
	MoreChunksCS.Unlock();
	rtn = rtn || HasQueuedChunk();
	return rtn;
}

const bool FChunkWriter::FQueuedChunkWriter::HasQueuedChunk()
{
	bool rtn;
	ChunkFileQueueCS.Lock();
	rtn = ChunkFileQueue.Num() > 0;
	ChunkFileQueueCS.Unlock();
	return rtn;
}

const bool FChunkWriter::FQueuedChunkWriter::CanQueueChunk()
{
	bool rtn;
	ChunkFileQueueCS.Lock();
	rtn = ChunkFileQueue.Num() < FBuildPatchData::ChunkQueueSize;
	ChunkFileQueueCS.Unlock();
	return rtn;
}

FChunkFile* FChunkWriter::FQueuedChunkWriter::GetNextChunk()
{
	FChunkFile* rtn = NULL;
	ChunkFileQueueCS.Lock();
	if( ChunkFileQueue.Num() > 0 )
	{
		rtn = ChunkFileQueue.Last();
		ChunkFileQueue.RemoveAt( ChunkFileQueue.Num() - 1 );
	}
	ChunkFileQueueCS.Unlock();
	return rtn;
}

void FChunkWriter::FQueuedChunkWriter::QueueChunk( const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash )
{
	// Create the FChunkFile and copy data
	FChunkFile* NewChunk = new FChunkFile();
	NewChunk->ChunkHeader.Guid = ChunkGuid;
	NewChunk->ChunkHeader.RollingHash = ChunkHash;
	FMemory::Memcpy( NewChunk->ChunkData, ChunkData, FBuildPatchData::ChunkDataSize );
	// Wait until we can fit this chunk in the queue
	while ( !CanQueueChunk() )
	{
		FPlatformProcess::Sleep( 0.0f );
	}
	// Queue the chunk
	ChunkFileQueueCS.Lock();
	ChunkFileQueue.Add( NewChunk );
	ChunkFileQueueCS.Unlock();
}

void FChunkWriter::FQueuedChunkWriter::SetNoMoreChunks()
{
	MoreChunksCS.Lock();
	bMoreChunks = false;
	MoreChunksCS.Unlock();
}

void FChunkWriter::FQueuedChunkWriter::GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes)
{
	FScopeLock ScopeLock(&ChunkFileSizesCS);
	OutChunkFileSizes.Empty(ChunkFileSizes.Num());
	OutChunkFileSizes.Append(ChunkFileSizes);
}

/* FChunkWriter implementation
*****************************************************************************/
FChunkWriter::FChunkWriter( const FString& ChunkDirectory )
{
	QueuedChunkWriter.ChunkDirectory = ChunkDirectory;
	WriterThread = FRunnableThread::Create(&QueuedChunkWriter, TEXT("QueuedChunkWriterThread"));
}

FChunkWriter::~FChunkWriter()
{
	if( WriterThread != NULL )
	{
		delete WriterThread;
		WriterThread = NULL;
	}
}

void FChunkWriter::QueueChunk( const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash )
{
	QueuedChunkWriter.QueueChunk( ChunkData, ChunkGuid, ChunkHash);
}

void FChunkWriter::NoMoreChunks()
{
	QueuedChunkWriter.SetNoMoreChunks();
}

void FChunkWriter::WaitForThread()
{
	if( WriterThread != NULL )
	{
		WriterThread->WaitForCompletion();
	}
}

void FChunkWriter::GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes)
{
	QueuedChunkWriter.GetChunkFilesizes(OutChunkFileSizes);
}

#endif // WITH_BUILDPATCHGENERATION

