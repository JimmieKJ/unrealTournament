// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchChunkCache.cpp: Implements classes involved with chunks for the build system.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

// How many chunks we should be able to keep in RAM while required
#define CHUNK_CACHE_SIZE		256

// How many chunks we forward acquire each time we need to load one from disk
// This value is how many upcoming chunks to scan through, and load from disk whichever ones
// are on disk. It essentially stands for the minimum number of chunk parts that will be written
// to the installation in between any chunks that source from HDD
#define FORWARD_READ_COUNT		40

/* FThreadSafeChunkCache implementation
*****************************************************************************/
bool FBuildPatchChunkCache::FThreadSafeChunkCache::Contains( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ThreadLock );
	return ChunkStore.Contains( ChunkGuid );
}

int32 FBuildPatchChunkCache::FThreadSafeChunkCache::Num()
{
	FScopeLock ScopeLock( &ThreadLock );
	return ChunkStore.Num() + ReservedChunks.Num();
}

void FBuildPatchChunkCache::FThreadSafeChunkCache::Add( const FGuid& ChunkGuid, FChunkFile* ChunkFile )
{
	FScopeLock ScopeLock( &ThreadLock );
	// Must not be NULL
	check( ChunkFile != NULL );
	// Must not exist already
	check( Contains( ChunkGuid ) == false );
	// We only accept chunks with reservations
	check( HasReservation( ChunkGuid ) );
	// Remove reservation and add, returning new entry in order to receive data
	RemoveReservation( ChunkGuid );
	// Add the entry
	ChunkStore.Add( ChunkGuid, ChunkFile );
}

FChunkFile* FBuildPatchChunkCache::FThreadSafeChunkCache::Get( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ThreadLock );
	return ChunkStore[ ChunkGuid ];
}

void FBuildPatchChunkCache::FThreadSafeChunkCache::Remove( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ThreadLock );
	RemoveReservation( ChunkGuid );
	if( ChunkStore.Contains( ChunkGuid ) )
	{
		// Get and release the data lock in case the data is currently being read
		ChunkStore[ ChunkGuid ]->GetDataLock( NULL, NULL );
		ChunkStore[ ChunkGuid ]->ReleaseDataLock();
		delete ChunkStore[ ChunkGuid ];
		ChunkStore.Remove( ChunkGuid );
	}
}

void FBuildPatchChunkCache::FThreadSafeChunkCache::PurgeUnreferenced()
{
	FScopeLock ScopeLock( &ThreadLock );
	// Get a copy of our key list
	TArray< FGuid > CachedChunkGuids;
	// Copy the keys so that we can iterate without invalidating
	ChunkStore.GetKeys( CachedChunkGuids );
	// Check each entry for 0 ref count
	for ( auto CachedChunkGuidsIt = CachedChunkGuids.CreateConstIterator(); CachedChunkGuidsIt; ++CachedChunkGuidsIt )
	{
		const FGuid& ChunkGuid = *CachedChunkGuidsIt;
		FChunkFile& ChunkFile = *ChunkStore[ ChunkGuid ];
		if( ChunkFile.GetRefCount() == 0 )
		{
			Remove( ChunkGuid );
		}
	}
}

void FBuildPatchChunkCache::FThreadSafeChunkCache::Empty()
{
	FScopeLock ScopeLock( &ThreadLock );
	for ( auto ChunkStoreIt = ChunkStore.CreateConstIterator(); ChunkStoreIt; ++ChunkStoreIt )
	{
		delete ChunkStoreIt->Value;
	}
	ChunkStore.Empty();
}

bool FBuildPatchChunkCache::FThreadSafeChunkCache::TryAddReservation( const FGuid& ChunkGuid )
{
	// Important to keep lock throughout this call
	FScopeLock ScopeLock( &ThreadLock );

	// Application error if we already have this chunk
	check( ChunkStore.Contains( ChunkGuid ) == false );

	// Pass if already reserved
	if( ReservedChunks.Contains( ChunkGuid ) )
	{
		return true;
	}

	// Fail if we cannot fit yet
	if( Num() >= CHUNK_CACHE_SIZE )
	{
		return false;
	}

	// Make the reservation
	ReservedChunks.AddUnique( ChunkGuid );
	return true;
}

void FBuildPatchChunkCache::FThreadSafeChunkCache::RemoveReservation( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ThreadLock );
	ReservedChunks.Remove( ChunkGuid );
}

bool FBuildPatchChunkCache::FThreadSafeChunkCache::HasReservation( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ThreadLock );
	return ReservedChunks.Contains( ChunkGuid );
}

/* FBuildPatchChunkCache implementation
*****************************************************************************/
FBuildPatchChunkCache::FBuildPatchChunkCache( const FBuildPatchAppManifestRef& InInstallManifet, const FBuildPatchAppManifestPtr& InCurrentManifest, const FString& InChunkCacheStage, const FString& InCurrentInstallDir, FBuildPatchProgress* InBuildProgress, TArray<FString>& FilesToConstruct, FBuildPatchInstallationInfo& InstallationInfoRef )
	: ChunkCacheStage(InChunkCacheStage)
	, CurrentInstallDir( InCurrentInstallDir )
	, InstallManifet( InInstallManifet )
	, CurrentManifest( InCurrentManifest )
	, BuildProgress( InBuildProgress )
	, bEnableChunkRecycle( InCurrentManifest.IsValid() )
	, bDownloadsStarted( 0 )
	, SkippedChunkDownloadSize( 0 )
	, InstallationInfo( InstallationInfoRef )
{
	// Setup chunk information
	InstallManifet->GetChunksRequiredForFiles( FilesToConstruct, FullChunkRefList, false );
	for( int32 FullChunkRefListIdx = FullChunkRefList.Num() - 1; FullChunkRefListIdx >= 0; --FullChunkRefListIdx )
	{
		const FGuid& ChunkGuid = FullChunkRefList[ FullChunkRefListIdx ];
		ChunkOrigins.Add( ChunkGuid, EChunkOrigin::Download );
		if( ChunkUseOrderStack.Num() == 0 || ChunkUseOrderStack.Last() != ChunkGuid )
		{
			ChunkUseOrderStack.Push( ChunkGuid );
		}
	}
	// Get unique list of downloads
	InstallManifet->GetChunksRequiredForFiles( FilesToConstruct, ChunksToDownload, true );
	// Check for chunks that can be constructed locally
	TArray< FGuid > ChunksToConstruct;
	InstallationInfo.EnumerateProducibleChunks(FullChunkRefList, ChunksToConstruct);
	for( auto ChunksToConstructIt = ChunksToConstruct.CreateConstIterator(); ChunksToConstructIt; ++ChunksToConstructIt )
	{
		const FGuid& ChunkGuid = *ChunksToConstructIt;
		ChunkOrigins.Add( ChunkGuid, EChunkOrigin::Recycle );
		ChunksToDownload.Remove( ChunkGuid );
	}

	// Init chunk stats
	NumFilesToConstruct = FilesToConstruct.Num();
	NumChunksToDownload = ChunksToDownload.Num();
	NumChunksToRecycle = ChunksToConstruct.Num();
	NumRequiredChunks = NumChunksToDownload + NumChunksToRecycle;
	TotalChunkDownloadSize = InstallManifet->GetDataSize(ChunksToDownload);
	NumChunksRecycled.Reset();
	NumChunksCacheBooted.Reset();
	NumDriveCacheChunkLoads.Reset();
	NumRecycleFailures.Reset();
	NumDriveCacheLoadFailures.Reset();
}

/* FBuildPatchChunkCache implementation
*****************************************************************************/
const uint32& FBuildPatchChunkCache::GetStatNumFilesToConstruct() const
{
	return NumFilesToConstruct;
}

const uint32& FBuildPatchChunkCache::GetStatNumChunksToDownload() const
{
	return NumChunksToDownload;
}

const uint32& FBuildPatchChunkCache::GetStatNumChunksToRecycle() const
{
	return NumChunksToRecycle;
}

const uint32& FBuildPatchChunkCache::GetStatNumRequiredChunks() const
{
	return NumRequiredChunks;
}

const int64& FBuildPatchChunkCache::GetStatTotalChunkDownloadSize() const
{
	return TotalChunkDownloadSize;
}

const int32 FBuildPatchChunkCache::GetCounterChunksRecycled() const
{
	return NumChunksRecycled.GetValue();
}

const int32 FBuildPatchChunkCache::GetCounterChunksCacheBooted() const
{
	return NumChunksCacheBooted.GetValue();
}

const int32 FBuildPatchChunkCache::GetCounterDriveCacheChunkLoads() const
{
	return NumDriveCacheChunkLoads.GetValue();
}

const int32 FBuildPatchChunkCache::GetCounterRecycleFailures() const
{
	return NumRecycleFailures.GetValue();
}

const int32 FBuildPatchChunkCache::GetCounterDriveCacheLoadFailures() const
{
	return NumDriveCacheLoadFailures.GetValue();
}

FChunkFile* FBuildPatchChunkCache::GetChunkFile( const FGuid& ChunkGuid )
{
	// Data setup check
	check( ChunkOrigins.Contains( ChunkGuid ) );

	// If we don't already have this chunk yet, then we are ready to grab the next batch.
	// We do it here because we want any loading from disk to be performed on the same thread as writing to disk
	FChunkFile* ChunkFilePtr = NULL;
	if( ChunkCache.Contains( ChunkGuid ) == false )
	{
		// We generate a list of the next chunks to acquire
		TArray< FGuid > NextChunkBatch;
		NextChunkBatch.AddUnique( ChunkGuid );
		ChunkInfoLock.Lock();
		int32 ForwardReadIdx = 1;
		const int32 ChunkUseOrderStackNum = ChunkUseOrderStack.Num();
		while ( ChunkUseOrderStackNum >= ForwardReadIdx && ( NextChunkBatch.Num() < FORWARD_READ_COUNT ) && !FBuildPatchInstallError::HasFatalError() )
		{
			const FGuid NextChunkGuid = ChunkUseOrderStack[ ChunkUseOrderStackNum - ForwardReadIdx ];
			++ForwardReadIdx;
			if( ChunkCache.Contains( NextChunkGuid ) == false )
			{
				NextChunkBatch.AddUnique( NextChunkGuid );
				ReserveChunkInventorySlotForce( NextChunkGuid );
			}
		}
		ChunkInfoLock.Unlock();

		// Load each chunk that comes from disk
		for( auto NextChunkBatchIt = NextChunkBatch.CreateConstIterator(); NextChunkBatchIt && !FBuildPatchInstallError::HasFatalError(); ++NextChunkBatchIt )
		{
			FScopeLock ScopeLock( &ChunkOriginLock );
			const FGuid& NextChunkGuid = *NextChunkBatchIt;
			const EChunkOrigin::Type& ChunkOrigin = ChunkOrigins[ NextChunkGuid ];
			BuildProgress->WaitWhilePaused();
			// Check if there were any errors while paused, like cancelling
			if( FBuildPatchInstallError::HasFatalError() )
			{
				return NULL;
			}
			switch ( ChunkOrigin )
			{
				case EChunkOrigin::Harddisk:
					{
						// Force cache space for this chunk
						ReserveChunkInventorySlotForce( NextChunkGuid );
						// Read from disk
						bool bReadFromDisk = ReadChunkFromDriveCache( NextChunkGuid );
						if( bReadFromDisk == false && !FBuildPatchInstallError::HasFatalError() )
						{
							// Queue for download instead
							ChunkOrigins[ NextChunkGuid ] = EChunkOrigin::Download;
							FBuildPatchDownloader::Get().AddChunkToDownload( NextChunkGuid, true );
							// Count fail
							NumDriveCacheLoadFailures.Increment();
						}
					}
					break;
				case EChunkOrigin::Recycle:
					{
						// Force cache space for this chunk
						ReserveChunkInventorySlotForce( NextChunkGuid );
						// Recycle from build
						bool bRecycled = RecycleChunkFromBuild( NextChunkGuid );
						if( bRecycled == false && !FBuildPatchInstallError::HasFatalError() )
						{
							// Queue for download instead
							ChunkOrigins[ NextChunkGuid ] = EChunkOrigin::Download;
							FBuildPatchDownloader::Get().AddChunkToDownload( NextChunkGuid, true );
							// Count fail
							NumRecycleFailures.Increment();
						}
					}
					break;
				case EChunkOrigin::Download:
					// We skip over downloads that will be running on another thread
					continue;
				default:
					check( false );
					break;
			}
		}

		// Make sure the currently required chunk is available
		WaitForChunk( ChunkGuid );
	}

	// Check if there were any errors
	if( FBuildPatchInstallError::HasFatalError() )
	{
		return NULL;
	}

	// Get the chunk file, should assert if was removed
	ChunkFilePtr = ChunkCache.Get( ChunkGuid );

	// Check and update chunk information
	{
		FScopeLock ScopeLock( &ChunkInfoLock );
		// The chunk we just handed out must be the first in FullChunkRefList list, otherwise we are constructing out of order.
		check( FullChunkRefList.Num() > 0 && FullChunkRefList[ 0 ] == ChunkGuid );
		// Must also be the chunk on top of ChunkUseOrderStack
		check( ChunkUseOrderStack.Num() > 0 && ChunkUseOrderStack.Last() == ChunkGuid );
		// Remove this chunk from the full reference list
		FullChunkRefList.RemoveAt( 0 ); 
		// If this was the last sequential reference to this chunk, also remove from order stack
		if( FullChunkRefList.Num() > 0 && FullChunkRefList[ 0 ] != ChunkUseOrderStack.Last() )
		{
			ChunkUseOrderStack.Pop();
			// The next chunk in both lists must be the same now
			check( FullChunkRefList[ 0 ] == ChunkUseOrderStack.Last() );
		}
	}

	// Return the pointer
	return ChunkFilePtr;
}

bool FBuildPatchChunkCache::ReadChunkFromDriveCache( const FGuid& ChunkGuid )
{
	bool bSuccess = true;

	// Get the chunk filename
	const FString Filename = FBuildPatchUtils::GetChunkOldFilename( ChunkCacheStage, ChunkGuid );

	// Read the chunk
	FArchive* FileReader = IFileManager::Get().CreateFileReader( *Filename );
	bSuccess = FileReader != NULL;
	if( bSuccess )
	{
		// Get file size
		const int64 FileSize = FileReader->TotalSize();

		// Create the ChunkFile data structure
		FChunkFile* NewChunkFile = new FChunkFile( GetRemainingReferenceCount( ChunkGuid ), true );

		// Lock data
		FChunkHeader* ChunkHeader;
		uint8* ChunkData;
		NewChunkFile->GetDataLock( &ChunkData, &ChunkHeader );

		// Read the header
		*FileReader << *ChunkHeader;

		// Check header magic
		bSuccess = ChunkHeader->IsValidMagic();
		if ( bSuccess )
		{
			// Check a rolling hash has the right data size
			bSuccess = ( ChunkHeader->HashType == FChunkHeader::HASH_ROLLING && ChunkHeader->DataSize == FBuildPatchData::ChunkDataSize );
			if( bSuccess )
			{
				// Check Header and data size
				bSuccess = ( ChunkHeader->HeaderSize + ChunkHeader->DataSize ) == FileSize;
				if( bSuccess )
				{
					// Read the data
					FileReader->Serialize( ChunkData, FBuildPatchData::ChunkDataSize );
					// Verify the data hash
					bSuccess = ChunkHeader->RollingHash == FRollingHash< FBuildPatchData::ChunkDataSize >::GetHashForDataSet( ChunkData );
					if( !bSuccess )
					{
						FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, Filename, INDEX_NONE, TEXT( "DriveCache" ), TEXT( "Hash Check Failed" ) );
						GLog->Logf( TEXT( "FBuildPatchChunkCache: ERROR: ReadChunkFromDriveCache chunk failed hash check %s" ), *ChunkGuid.ToString() );
					}
					else
					{
						// Count loads
						NumDriveCacheChunkLoads.Increment();
						GLog->Logf( TEXT( "FBuildPatchChunkCache: ReadChunkFromDriveCache loaded chunk %s" ), *ChunkGuid.ToString() );
					}
				}
				else
				{
					FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, Filename, INDEX_NONE, TEXT( "DriveCache" ), TEXT( "Incorrect File Size" ) );
					GLog->Logf( TEXT( "FBuildPatchChunkCache: ERROR: ReadChunkFromDriveCache header info does not match file size %s" ), *ChunkGuid.ToString() );
				}
			}
			else
			{
				FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, Filename, INDEX_NONE, TEXT( "DriveCache" ), TEXT( "Datasize/Hashtype Mismatch" ) );
				GLog->Logf( TEXT( "FBuildPatchChunkCache: ERROR: ReadChunkFromDriveCache mismatch datasize/hashtype combination %s" ), *ChunkGuid.ToString() );
			}
		}
		else
		{
			FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, Filename, INDEX_NONE, TEXT( "DriveCache" ), TEXT( "Corrupt Header" ) );
			GLog->Logf( TEXT( "FBuildPatchChunkCache: ERROR: ReadChunkFromDriveCache corrupt header %s" ), *ChunkGuid.ToString() );
		}

		// Release data
		NewChunkFile->ReleaseDataLock();

		// Add the newly filled data to the cache if successful
		if( bSuccess )
		{
			ChunkCache.Add( ChunkGuid, NewChunkFile );
		}
		// If there was a problem, remove from cache and reservation
		else
		{
			ChunkCache.Remove( ChunkGuid );
		}

		// Close the file
		FileReader->Close();
		delete FileReader;

	}
	else
	{
		FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, Filename, FPlatformMisc::GetLastError(), TEXT( "DriveCache" ), TEXT( "Open File Fail" ) );
		GLog->Logf( TEXT( "BuildPatchServices: ERROR: GetChunkData could not open chunk file %s" ), *ChunkGuid.ToString() );
	}

	return bSuccess;
}

bool FBuildPatchChunkCache::RecycleChunkFromBuild( const FGuid& ChunkGuid )
{
	// Must never double acquire
	check( ChunkCache.Contains( ChunkGuid ) == false );

	// Debug leaving any files open
	bool bSuccess = true;

	// Get the app manifest that this chunk can be sourced from
	FBuildPatchAppManifestPtr ChunkSourceAppManifest = InstallationInfo.GetManifestContainingChunk(ChunkGuid);
	if (!ChunkSourceAppManifest.IsValid())
	{
		return false;
	}

	// Get the install directory for this manifest
	const FString ChunkSourceInstallDir = InstallationInfo.GetManifestInstallDir(ChunkSourceAppManifest);
	if(ChunkSourceInstallDir.Len() <= 0)
	{
		return false;
	}

	// We need to generate an inventory of all chunk parts in this build that refer to the chunk that we require
	TMap< FGuid, TArray< FFileChunkPart > > ChunkPartInventory;
	TArray< FGuid > Array;
	Array.Add( ChunkGuid );
	ChunkSourceAppManifest->EnumerateChunkPartInventory(Array, ChunkPartInventory);

	// Attempt construction of the chunk from the parts
	FArchive* BuildFileIn = NULL;
	FString  BuildFileOpened;
	int64 BuildFileInSize = 0;

	// We must have a hash for this chunk or else we cant verify it
	uint64 ChunkHash = 0;
	TArray< FFileChunkPart >* FileChunkPartsPtr = ChunkPartInventory.Find( ChunkGuid );
	bSuccess = (FileChunkPartsPtr != NULL && ChunkSourceAppManifest->GetChunkHash(ChunkGuid, ChunkHash));
	if( bSuccess )
	{
		const TArray< FFileChunkPart >& FileChunkParts = *FileChunkPartsPtr;
		TArray< uint8 > TempArray;
		TempArray.AddUninitialized( FBuildPatchData::ChunkDataSize );
		uint8* TempChunkConstruction = TempArray.GetData();
		FMemory::Memzero( TempChunkConstruction, FBuildPatchData::ChunkDataSize );
		bSuccess = FileChunkParts.Num() > 0;
		for( auto FileChunkPartIt = FileChunkParts.CreateConstIterator(); FileChunkPartIt && bSuccess && !FBuildPatchInstallError::HasFatalError(); ++FileChunkPartIt )
		{
			const FFileChunkPart& FileChunkPart = *FileChunkPartIt;
			FString FullFilename = ChunkSourceInstallDir / FileChunkPart.Filename;
			// Close current build file ?
			if( BuildFileIn != NULL && BuildFileOpened != FullFilename )
			{
				BuildFileIn->Close();
				delete BuildFileIn;
				BuildFileIn = NULL;
				BuildFileOpened = TEXT( "" );
				BuildFileInSize = 0;
			}
			// Open build file ?
			if( BuildFileIn == NULL )
			{
				BuildFileIn = IFileManager::Get().CreateFileReader( *FullFilename );
				bSuccess = BuildFileIn != NULL;
				if( !bSuccess )
				{
					BuildFileOpened = TEXT( "" );
					FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, FileChunkPart.Filename, FPlatformMisc::GetLastError(), TEXT( "ChunkRecycle" ), TEXT( "Source File Missing" ) );
					GWarn->Logf( TEXT( "BuildPatchChunkConstruction: Warning: Failed to load source file for chunk. %s" ), *FullFilename );
				}
				else
				{
					BuildFileOpened = FullFilename;
					BuildFileInSize = BuildFileIn->TotalSize();
				}
			}
			// Grab the section of the chunk
			if( BuildFileIn != NULL )
			{
				// Make sure we don't attempt to read off the end of the file
				const int64 LastRequiredByte = FileChunkPart.FileOffset + FileChunkPart.ChunkPart.Size;
				if( BuildFileInSize >= LastRequiredByte )
				{
					BuildFileIn->Seek( FileChunkPart.FileOffset );
					BuildFileIn->Serialize( TempChunkConstruction + FileChunkPart.ChunkPart.Offset, FileChunkPart.ChunkPart.Size );
				}
				else
				{
					bSuccess = false;
					FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, FileChunkPart.Filename, INDEX_NONE, TEXT( "ChunkRecycle" ), TEXT( "Source File Too Small" ) );
					GWarn->Logf( TEXT( "BuildPatchChunkConstruction: Warning: Source file too small for chunk position. %s" ), *FullFilename );
				}
			}
		}

		// Check no other fatal errors were registered in the meantime
		bSuccess = bSuccess && !FBuildPatchInstallError::HasFatalError();

		// Check chunk hash
		if( bSuccess )
		{
			bSuccess = FRollingHash< FBuildPatchData::ChunkDataSize >::GetHashForDataSet( TempChunkConstruction ) == ChunkHash;
			if( !bSuccess )
			{
				FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, TEXT( "" ), INDEX_NONE, TEXT( "ChunkRecycle" ), TEXT( "Chunk Hash Fail" ) );
				GWarn->Logf( TEXT( "BuildPatchChunkConstruction: Warning: Hash check failed for recycled chunk %s" ), *ChunkGuid.ToString() );
			}
		}

		// Save the chunk to cache if all went well
		if( bSuccess )
		{
			// It was added asynchronously!!
			check( ChunkCache.Contains( ChunkGuid ) == false );

			// Create the ChunkFile data structure
			FChunkFile* NewChunkFile = new FChunkFile( GetRemainingReferenceCount( ChunkGuid ), true );

			// Lock data
			FChunkHeader* ChunkHeader;
			uint8* ChunkData;
			NewChunkFile->GetDataLock( &ChunkData, &ChunkHeader );

			// Copy the data
			FMemoryReader MemReader( TempArray );
			MemReader.Serialize( ChunkData, FBuildPatchData::ChunkDataSize );

			// Setup the header
			ChunkHeader->Guid = ChunkGuid;
			ChunkHeader->StoredAs = FChunkHeader::STORED_RAW;
			ChunkHeader->DataSize = FBuildPatchData::ChunkDataSize; // This would change if compressing/encrypting
			ChunkHeader->HashType = FChunkHeader::HASH_ROLLING;
			ChunkHeader->RollingHash = ChunkHash;

			// Release data
			NewChunkFile->ReleaseDataLock();

			// Count chunk
			NumChunksRecycled.Increment();

			// Add it to our cache.
			ChunkCache.Add( ChunkGuid, NewChunkFile );
		}

		// Close any open file
		if( BuildFileIn != NULL )
		{
			BuildFileIn->Close();
			delete BuildFileIn;
			BuildFileIn = NULL;
		}
	}
	
	return bSuccess;
}

void FBuildPatchChunkCache::AddDataToCache( const FGuid& ChunkGuid, const TArray< uint8 >& ChunkDataFile )
{
	// Must never double acquire
	check( ChunkCache.Contains( ChunkGuid ) == false );

	// Create the ChunkFile data structure
	FChunkFile* NewChunkFile = new FChunkFile( GetRemainingReferenceCount( ChunkGuid ), false );

	// Lock data
	FChunkHeader* ChunkHeader;
	uint8* ChunkData;
	NewChunkFile->GetDataLock( &ChunkData, &ChunkHeader );

	// Copy the data
	FMemoryReader MemReader( ChunkDataFile );
	MemReader << *ChunkHeader;
	MemReader.Serialize( ChunkData, FBuildPatchData::ChunkDataSize );

	// Release data
	NewChunkFile->ReleaseDataLock();

	// Add it to our cache.
	ChunkCache.Add( ChunkGuid, NewChunkFile );
}

bool FBuildPatchChunkCache::ReserveChunkInventorySlot( const FGuid& ChunkGuid )
{
	// We shouldn't be trying to reserve for something already loaded
	check( ChunkCache.Contains( ChunkGuid ) == false );

	// If already reserved, return immediate
	if( ChunkCache.HasReservation( ChunkGuid ) )
	{
		return true;
	}

	// We must be a required chunk if not already reserved
	ChunkInfoLock.Lock();
	check( ChunkUseOrderStack.Contains( ChunkGuid ) );
	ChunkInfoLock.Unlock();

	// We only allow chunks if they will be used soon enough
	if( GetChunkOrderIndex( ChunkGuid ) >= CHUNK_CACHE_SIZE )
	{
		return false;
	}

	// Run a purge first
	ChunkCache.PurgeUnreferenced();

	// Try reserve
	return ChunkCache.TryAddReservation( ChunkGuid );
}

void FBuildPatchChunkCache::SkipFile( const FString& Filename )
{
	check( bDownloadsStarted.GetValue() == 0 );
	// We're going to skip over this file so we need to remove all the references to the chunks it uses from our
	// data lists to avoid downloading them and keep track of the next chunks we need to get hold of
	GLog->Logf( TEXT("FBuildPatchChunkCache::SkipFile %s"), *Filename );
	// Get the file manifest
	const FFileManifestData* FileManifest = InstallManifet->GetFileManifest( Filename );
	// Go through each chunk part, and remove it from out data lists
	FScopeLock ScopeLock( &ChunkInfoLock );
	for (const auto& ChunkPart : FileManifest->FileChunkParts)
	{
		SkipChunkPart(ChunkPart);
	}
}

void FBuildPatchChunkCache::SkipChunkPart( const FChunkPartData& ChunkPart )
{
	check( bDownloadsStarted.GetValue() == 0 );
	FScopeLock ScopeLock( &ChunkInfoLock );
	const FGuid& ChunkGuid = ChunkPart.Guid;
	// The chunk we are skipping must be the first in FullChunkRefList list, otherwise we are calling out of order.
	check( FullChunkRefList.Num() > 0 && FullChunkRefList[ 0 ] == ChunkGuid );
	// Must also be the chunk on top of ChunkUseOrderStack
	check( ChunkUseOrderStack.Num() > 0 && ChunkUseOrderStack.Last() == ChunkGuid );
	// Remove this chunk from the full reference list
	FullChunkRefList.RemoveAt( 0 ); 
	// If this was the last sequential reference to this chunk, also remove from order stack
	if( FullChunkRefList.Num() > 0 && FullChunkRefList[ 0 ] != ChunkUseOrderStack.Last() )
	{
		ChunkUseOrderStack.Pop();
		// The next chunk in both lists must be the same now
		check( FullChunkRefList[ 0 ] == ChunkUseOrderStack.Last() );
	}
	// If this chunk is no longer referenced, remove from download list
	if( ChunkUseOrderStack.Contains( ChunkGuid ) == false )
	{
		ChunksToDownload.Remove( ChunkGuid );
		SkippedChunkDownloadSize += InstallManifet->GetDataSize( ChunkGuid );
	}
	// Keep UI in check
	const float OriginalCount = TotalChunkDownloadSize;
	const float SkipCount = SkippedChunkDownloadSize;
	BuildProgress->SetStateProgress( EBuildPatchProgress::Downloading, SkipCount / OriginalCount );
}

void FBuildPatchChunkCache::BeginDownloads()
{
	check( bDownloadsStarted.GetValue() == 0 );
	// Queue downloads
	ChunkInfoLock.Lock();
	FBuildPatchDownloader::Get().AddChunksToDownload( ChunksToDownload, FBuildPatchData::ChunkData );
	ChunkInfoLock.Unlock();
	bDownloadsStarted.Set( 1 );
}

const bool FBuildPatchChunkCache::HaveDownloadsStarted() const
{
	return bDownloadsStarted.GetValue() > 0;
}

int32 FBuildPatchChunkCache::GetChunkOrderIndex( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ChunkInfoLock );
	int32 LastIndex = 0;
	if( ChunkUseOrderStack.FindLast( ChunkGuid, LastIndex ) )
	{
		return ChunkUseOrderStack.Num() - LastIndex;
	}
	// Apparently FindLast does not return 0 if the only instance is the first one!
	else if( ChunkUseOrderStack.Num() > 0 && ChunkUseOrderStack[ 0 ] == ChunkGuid )
	{
		return ChunkUseOrderStack.Num();
	}
	// If we get here there's been a fault
	check( false );
	return CHUNK_CACHE_SIZE;
}

int32 FBuildPatchChunkCache::GetRemainingReferenceCount( const FGuid& ChunkGuid )
{
	FScopeLock ScopeLock( &ChunkInfoLock );
	int32 RefCount = 0;
	for( auto FullChunkRefListIt = FullChunkRefList.CreateConstIterator(); FullChunkRefListIt; ++FullChunkRefListIt )
	{
		if( *FullChunkRefListIt == ChunkGuid )
		{
			++RefCount;
		}
	}
	return RefCount;
}

void FBuildPatchChunkCache::WaitForChunk( const FGuid& ChunkGuid )
{
	// Wait for the chunk to be available
	double StartedWaiting = FPlatformTime::Seconds();
	while( ChunkCache.Contains( ChunkGuid ) == false && !FBuildPatchInstallError::HasFatalError() )
	{
		FPlatformProcess::Sleep( 0.1f );
		const double CurrentTime = FPlatformTime::Seconds();
		if( BuildProgress->GetPauseState() )
		{
			BuildProgress->WaitWhilePaused();
			StartedWaiting = CurrentTime;
		}
		if( ( CurrentTime - StartedWaiting ) > 5.0 )
		{
			GWarn->Logf( TEXT( "FBuildPatchChunkCache: Still waiting for chunk %s" ), *ChunkGuid.ToString() );
			StartedWaiting = CurrentTime;
		}
	}
}

void FBuildPatchChunkCache::ReserveChunkInventorySlotForce( const FGuid& ChunkGuid )
{
	// If already reserved, return immediate
	if( ChunkCache.HasReservation( ChunkGuid ) || ChunkCache.Contains( ChunkGuid ) )
	{
		return;
	}

	// Begin by checking if any slots can be freed
	ChunkCache.PurgeUnreferenced();

	// Try to add the reservation
	bool bReservationAccepted = ChunkCache.TryAddReservation( ChunkGuid );

	// If we couldn't reserve, we need to boot out a chunk for this required one
	if( bReservationAccepted == false )
	{
		// We create a unique ref array from the use order so that chunks not needed
		// for longer times end up nearer the bottom of the array
		TArray< FGuid > ChunkPriorityList;
		ChunkInfoLock.Lock();
		for( int32 ChunkUseOrderStackIdx = ChunkUseOrderStack.Num() - 1; ChunkUseOrderStackIdx >= 0 ; --ChunkUseOrderStackIdx )
		{
			ChunkPriorityList.AddUnique( ChunkUseOrderStack[ ChunkUseOrderStackIdx ] );
		}
		ChunkInfoLock.Unlock();

		// Starting at the bottom of the list, we look for a chunk that is contained in the cache
		for( int32 ChunkPriorityListIdx = ChunkPriorityList.Num() - 1; ChunkPriorityListIdx >= 0 && !bReservationAccepted; --ChunkPriorityListIdx )
		{
			const FGuid& LowPriChunk = ChunkPriorityList[ ChunkPriorityListIdx ];
			BuildProgress->WaitWhilePaused();
			// Check if there were any errors while paused, like canceling
			if( FBuildPatchInstallError::HasFatalError() )
			{
				return;
			}
			if( ChunkCache.Contains( LowPriChunk ) )
			{
				GWarn->Logf( TEXT( "FBuildPatchChunkCache: Booting chunk %s" ), *LowPriChunk.ToString() );

				// Save chunk to disk so we don't have to download again
				bool bSuccess = true;
				const FString NewChunkFilename = FBuildPatchUtils::GetChunkOldFilename( ChunkCacheStage, LowPriChunk );
				FChunkFile* LowPriChunkFile = ChunkCache.Get( LowPriChunk );
				FChunkHeader* LowPriChunkHeader;
				uint8* LowPriChunkData;
				LowPriChunkFile->GetDataLock( &LowPriChunkData, &LowPriChunkHeader );
				FArchive* FileOut = IFileManager::Get().CreateFileWriter( *NewChunkFilename );
				bSuccess = FileOut != NULL;
				const int32 LastError = FPlatformMisc::GetLastError();
				if( bSuccess )
				{
					// Setup Header
					*FileOut << *LowPriChunkHeader;
					LowPriChunkHeader->HeaderSize = FileOut->Tell();
					LowPriChunkHeader->StoredAs = FChunkHeader::STORED_RAW;
					LowPriChunkHeader->DataSize = FBuildPatchData::ChunkDataSize; // This would change if compressing/encrypting
					LowPriChunkHeader->HashType = FChunkHeader::HASH_ROLLING;

					// Write out file
					FileOut->Seek( 0 );
					*FileOut << *LowPriChunkHeader;
					FileOut->Serialize( LowPriChunkData, FBuildPatchData::ChunkDataSize );
					FileOut->Close();

					delete FileOut;
				}
				LowPriChunkFile->ReleaseDataLock();

				// Setup new chunk origin
				if( bSuccess )
				{
					ChunkOrigins[ LowPriChunk ] = EChunkOrigin::Harddisk;
				}
				else
				{
					// Queue download if save failed
					ChunkOrigins[ LowPriChunk ] = EChunkOrigin::Download;
					FBuildPatchDownloader::Get().AddChunkToDownload( LowPriChunk );
					FBuildPatchAnalytics::RecordChunkCacheError( ChunkGuid, NewChunkFilename, LastError, TEXT( "ChunkBooting" ), TEXT( "Chunk Save Failed" ) );
				}

				// Boot this chunk
				ChunkCache.Remove( LowPriChunk );

				// Try get the reservation again!
				bReservationAccepted = ChunkCache.TryAddReservation( ChunkGuid );

				// Count the boot
				NumChunksCacheBooted.Increment();
			}
		}

		// We must have been able to make room
		check( bReservationAccepted );
	}
}

/* FBuildPatchChunkCache system singleton setup
*****************************************************************************/
TSharedPtr< FBuildPatchChunkCache, ESPMode::ThreadSafe > FBuildPatchChunkCache::SingletonInstance = NULL;

void FBuildPatchChunkCache::Init(const FBuildPatchAppManifestRef& InInstallManifet, const FBuildPatchAppManifestPtr& InCurrentManifest, const FString& InChunkCacheStage, const FString& InCurrentInstallDir, FBuildPatchProgress* InBuildProgress, TArray<FString>& FilesToConstruct, FBuildPatchInstallationInfo& InstallationInfoRef)
{
	// We won't allow misuse of these functions
	check(!SingletonInstance.IsValid());
	SingletonInstance = MakeShareable(new FBuildPatchChunkCache(InInstallManifet, InCurrentManifest, InChunkCacheStage, InCurrentInstallDir, InBuildProgress, FilesToConstruct, InstallationInfoRef));
}

bool FBuildPatchChunkCache::IsAvailable()
{
	return SingletonInstance.IsValid();
}

FBuildPatchChunkCache& FBuildPatchChunkCache::Get()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	return *SingletonInstance.Get();
}

void FBuildPatchChunkCache::Shutdown()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	SingletonInstance->ChunkCache.Empty();
	SingletonInstance.Reset();
}
