// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchFileConstructor.cpp: Implements the BuildPatchFileConstructor class
	that handles creating files in a manifest from the chunks that make it.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

using namespace BuildPatchConstants;

// This define the number of bytes on a half-finished file that we ignore from the end
// incase of previous partial write.
#define NUM_BYTES_RESUME_IGNORE     1024

/**
 * This struct handles loading and saving of simple resume information, that will allow us to decide which
 * files should be resumed from. It will also check that we are creating the same version and app as we expect to be.
 */
struct FResumeData
{
public:
	// Save the staging directory
	const FString StagingDir;

	// The filename to the resume data information
	const FString ResumeDataFile;

	// A string determining the app and version we are installing
	const FString PatchVersion;

	// The set of files that were started
	TSet<FString> FilesStarted;

	// The set of files that were completed, determined by expected filesize
	TSet<FString> FilesCompleted;

	// The manifest for the app we are installing
	FBuildPatchAppManifestRef BuildManifest;

	// Whether we have resume data for this install
	bool bHasResumeData;

	// Whether we have resume data for a different install.
	bool bHasIncompatibleResumeData;

public:
	/**
	 * Constructor - reads in the resume data
	 * @param InStagingDir      The install staging directory
	 * @param InBuildManifest   The manifest we are installing from
	 */
	FResumeData(const FString& InStagingDir, const FBuildPatchAppManifestRef& InBuildManifest)
		: StagingDir(InStagingDir)
		, ResumeDataFile(InStagingDir / TEXT("$resumeData"))
		, PatchVersion(InBuildManifest->GetAppName() + InBuildManifest->GetVersionString())
		, BuildManifest(InBuildManifest)
		, bHasResumeData(false)
		, bHasIncompatibleResumeData(false)
	{
		// Load data from previous resume file
		bHasResumeData = FPlatformFileManager::Get().GetPlatformFile().FileExists(*ResumeDataFile);
		GLog->Logf(TEXT("BuildPatchResumeData file found %d"), bHasResumeData);
		if (bHasResumeData)
		{
			FString PrevResumeData;
			TArray< FString > ResumeDataLines;
			FFileHelper::LoadFileToString(PrevResumeData, *ResumeDataFile);
			PrevResumeData.ParseIntoArray(ResumeDataLines, TEXT("\n"), true);
			// Line 1 will be the previously attempted version
			FString PreviousVersion = (ResumeDataLines.Num() > 0) ? MoveTemp(ResumeDataLines[0]) : TEXT("");
			bHasResumeData = PreviousVersion == PatchVersion;
			bHasIncompatibleResumeData = !bHasResumeData;
			GLog->Logf(TEXT("BuildPatchResumeData version matched %d %s == %s"), bHasResumeData, *PreviousVersion, *PatchVersion);
		}
	}

	/**
	 * Saves out the resume data
	 */
	void SaveOut()
	{
		// Save out the patch version
		FFileHelper::SaveStringToFile(PatchVersion + TEXT("\n"), *ResumeDataFile);
	}

	/**
	 * Checks whether the file was completed during last install attempt and adds it to FilesCompleted if so
	 * @param Filename    The filename to check
	 */
	void CheckFile( const FString& Filename )
	{
		// If we had resume data, check file size is correct
		if(bHasResumeData)
		{
			const FString FullFilename = StagingDir / Filename;
			const int64 DiskFileSize = IFileManager::Get().FileSize( *FullFilename );
			const int64 CompleteFileSize = BuildManifest->GetFileSize( Filename );
			if (DiskFileSize > 0 && DiskFileSize <= CompleteFileSize)
			{
				FilesStarted.Add(Filename);
			}
			if( DiskFileSize == CompleteFileSize )
			{
				FilesCompleted.Add( Filename );
			}
		}
	}
};

/* FBuildPatchFileConstructor implementation
 *****************************************************************************/
FBuildPatchFileConstructor::FBuildPatchFileConstructor( FBuildPatchAppManifestPtr InInstalledManifest, FBuildPatchAppManifestRef InBuildManifest, const FString& InInstallDirectory, const FString& InStageDirectory, const TArray< FString >& InConstructList, FBuildPatchProgress* InBuildProgress )
	: Thread( NULL )
	, bIsRunning( false )
	, bIsInited( false )
	, bInitFailed( false )
	, bIsDownloadStarted( false )
	, ThreadLock()
	, InstalledManifest( InInstalledManifest )
	, BuildManifest( InBuildManifest )
	, BuildProgress( InBuildProgress )
	, InstallDirectory( InInstallDirectory )
	, StagingDirectory( InStageDirectory )
	, TotalJobSize( 0 )
	, ByteProcessed( 0 )
	, FilesToConstruct( InConstructList )
{
	// Init progress
	BuildProgress->SetStateProgress( EBuildPatchProgress::Installing, 0.0f );
	// Count initial job size
	for( auto ConstructIt = InConstructList.CreateConstIterator(); ConstructIt; ++ConstructIt )
	{
		TotalJobSize += InBuildManifest->GetFileSize( *ConstructIt );
	}
	// Start thread!
	const TCHAR* ThreadName = TEXT( "FileConstructorThread" );
	Thread = FRunnableThread::Create(this, ThreadName);
}

FBuildPatchFileConstructor::~FBuildPatchFileConstructor()
{
	// Wait for and deallocate the thread
	if( Thread != NULL )
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = NULL;
	}
}

bool FBuildPatchFileConstructor::Init()
{
	// We are ready to go if our delegates are bound and directories successfully created
	bool bStageDirExists = IFileManager::Get().DirectoryExists(*StagingDirectory);
	if (!bStageDirExists)
	{
		UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Stage directory missing %s"), *StagingDirectory);
		FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingStageDirectory);
	}
	bool bInstallDirExists = IFileManager::Get().DirectoryExists(*InstallDirectory);
	if (!bInstallDirExists)
	{
		UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Install directory missing %s"), *InstallDirectory);
		FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::MissingInstallDirectory);
	}
	bool bReady = bStageDirExists && bInstallDirExists;
	SetInitFailed(!bReady);
	return bReady;
}

uint32 FBuildPatchFileConstructor::Run()
{
	SetRunning( true );
	SetInited( true );
	const bool bIsFileData = BuildManifest->IsFileDataManifest();

	// Save the list of completed files
	TArray< FString > ConstructedFiles;

	// Check for resume data
	FResumeData ResumeData( StagingDirectory, BuildManifest );

	// If we found incompatible resume data, we need to clean out the staging folder
	// We don't delete the folder itself though as we should presume it was created with desired attributes
	if (ResumeData.bHasIncompatibleResumeData)
	{
		GLog->Logf(TEXT("BuildPatchServices: Deleting incompatible stage files"));
		DeleteDirectoryContents(StagingDirectory);
	}

	// Save for started version
	ResumeData.SaveOut();

	// Start resume progress at zero or one
	BuildProgress->SetStateProgress( EBuildPatchProgress::Resuming, ResumeData.bHasResumeData ? 0.0f : 1.0f );

	// While we have files to construct, run
	FString FileToConstruct;
	while( GetFileToConstruct( FileToConstruct ) && !FBuildPatchInstallError::HasFatalError() )
	{
		// Check resume status, currently we are only supporting sequential resume, so once we start downloading, we can't resume any more.
		// this only comes up if the resume data has been changed externally.
		ResumeData.CheckFile( FileToConstruct );
		const bool bFilePreviouslyComplete = !bIsDownloadStarted && ResumeData.FilesCompleted.Contains( FileToConstruct );
		const bool bFilePreviouslyStarted = !bIsDownloadStarted && ResumeData.FilesStarted.Contains( FileToConstruct );

		// Construct or skip the file
		bool bFileSuccess;
		if( bFilePreviouslyComplete )
		{
			bFileSuccess = true;
			CountBytesProcessed( BuildManifest->GetFileSize( FileToConstruct ) );
			// Inform the chunk cache of the chunk skip
			if( !bIsFileData )
			{
				FBuildPatchChunkCache::Get().SkipFile( FileToConstruct );
			}
		}
		else
		{
			bFileSuccess = ConstructFileFromChunks( FileToConstruct, bFilePreviouslyStarted );
		}

		// If the file succeeded, add to lists
		if( bFileSuccess )
		{
			ConstructedFiles.Add( FileToConstruct );
		}
		else
		{
			// This will only record and log if a failure was not already registered
			FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::UnknownFail);
		}

		// Pause
		BuildProgress->WaitWhilePaused();
	}

	BuildProgress->SetStateProgress(EBuildPatchProgress::Resuming, 1.0f);

	// Set constructed files
	ThreadLock.Lock();
	FilesConstructed = MoveTemp(ConstructedFiles);
	ThreadLock.Unlock();

	SetRunning( false );
	return 0;
}

void FBuildPatchFileConstructor::Wait()
{
	if( Thread != NULL )
	{
		Thread->WaitForCompletion();
	}
}

bool FBuildPatchFileConstructor::IsComplete()
{
	FScopeLock Lock( &ThreadLock );
	return ( !bIsRunning && bIsInited ) || bInitFailed;
}

void FBuildPatchFileConstructor::GetFilesConstructed( TArray< FString >& ConstructedFiles )
{
	FScopeLock Lock( &ThreadLock );
	ConstructedFiles.Empty();
	ConstructedFiles.Append( FilesConstructed );
}

void FBuildPatchFileConstructor::AddFileDataToInventory( const FGuid& FileGuid, const FString& Filename )
{
	FScopeLock Lock( &FileDataAvailabilityLock );
	FileDataAvailability.Add( FileGuid, Filename );
}

void FBuildPatchFileConstructor::PurgeFileDataInventory()
{
	FScopeLock Lock( &FileDataAvailabilityLock );
	FileDataAvailability.Empty();
}

const bool FBuildPatchFileConstructor::IsFileDataAvailable( const FGuid& FileGuid ) const
{
	// If we should be pausing, return false so that the construction waits
	if( BuildProgress->GetPauseState() )
	{
		return false;
	}
	// If we have a fatal error, we can say true, which will cause the constructor to error out
	bool bRtn = FBuildPatchInstallError::HasFatalError();
	if ( bRtn == false )
	{
		FScopeLock Lock( &FileDataAvailabilityLock );
		bRtn = FileDataAvailability.Contains( FileGuid );
	}
	return bRtn;
}

const FString FBuildPatchFileConstructor::GetFileDataFilename( const FGuid& ChunkGuid ) const
{
	FString rtn = TEXT( "" );
	{
		FScopeLock Lock( &FileDataAvailabilityLock );
		rtn = FileDataAvailability.FindRef( ChunkGuid );
	}
	return rtn;
}

void FBuildPatchFileConstructor::SetRunning( bool bRunning )
{
	FScopeLock Lock( &ThreadLock );
	bIsRunning = bRunning;
}

void FBuildPatchFileConstructor::SetInited( bool bInited )
{
	FScopeLock Lock( &ThreadLock );
	bIsInited = bInited;
}

void FBuildPatchFileConstructor::SetInitFailed( bool bFailed )
{
	FScopeLock Lock( &ThreadLock );
	bInitFailed = bFailed;
}

void FBuildPatchFileConstructor::CountBytesProcessed( const int64& ByteCount )
{
	ByteProcessed += ByteCount;
	const double Total = TotalJobSize;
	const double Current = ByteProcessed;
	BuildProgress->SetStateProgress( EBuildPatchProgress::Installing, Current / Total );
}

bool FBuildPatchFileConstructor::GetFileToConstruct(FString& Filename)
{
	FScopeLock Lock(&ThreadLock);
	const bool bFileAvailable = FilesToConstruct.IsValidIndex(0);
	if (bFileAvailable)
	{
		Filename = FilesToConstruct[0];
		FilesToConstruct.RemoveAt(0);
	}
	return bFileAvailable;
}

bool FBuildPatchFileConstructor::ConstructFileFromChunks( const FString& Filename, bool bResumeExisting )
{
	const bool bIsFileData = BuildManifest->IsFileDataManifest();
	bResumeExisting = bResumeExisting && !bIsFileData;
	bool bSuccess = true;
	FString NewFilename = StagingDirectory / Filename;

	// Calculate the hash as we write the data
	FSHA1 HashState;
	FSHAHashData HashValue;

	// First make sure we can get the file manifest
	const FFileManifestData* FileManifest = BuildManifest->GetFileManifest(Filename);
	bSuccess = FileManifest != nullptr;
	if( bSuccess )
	{
		if( !FileManifest->SymlinkTarget.IsEmpty() )
		{
#if PLATFORM_MAC
			bSuccess = symlink(TCHAR_TO_UTF8(*FileManifest->SymlinkTarget), TCHAR_TO_UTF8(*NewFilename)) == 0;
#else
			const bool bSymlinkNotImplemented = false;
			check(bSymlinkNotImplemented);
			bSuccess = false;
#endif
			return bSuccess;
		}

		// Check for resuming of existing file
		int64 StartPosition = 0;
		int32 StartChunkPart = 0;
		if( bResumeExisting )
		{
			// We have to read in the existing file so that the hash check can still be done.
			FArchive* NewFileReader = IFileManager::Get().CreateFileReader( *NewFilename );
			if( NewFileReader != NULL )
			{
				// Read buffer
				uint8* ReadBuffer = new uint8[ FBuildPatchData::ChunkDataSize ];
				// Reuse a certain amount of the file
				StartPosition = FMath::Max<int64>( 0, NewFileReader->TotalSize() - NUM_BYTES_RESUME_IGNORE );
				// We'll also find the correct chunkpart to start writing from
				int64 ByteCounter = 0;
				for( int32 ChunkPartIdx = StartChunkPart; ChunkPartIdx < FileManifest->FileChunkParts.Num() && !FBuildPatchInstallError::HasFatalError(); ++ChunkPartIdx )
				{
					const FChunkPartData& ChunkPart = FileManifest->FileChunkParts[ ChunkPartIdx ];
					const int64 NextBytePosition = ByteCounter + ChunkPart.Size;
					if( NextBytePosition <= StartPosition )
					{
						// Read data for hash check
						NewFileReader->Serialize( ReadBuffer, ChunkPart.Size );
						HashState.Update( ReadBuffer, ChunkPart.Size );
						// Count bytes read from file
						ByteCounter = NextBytePosition;
						// Set to resume from next chunk part
						StartChunkPart = ChunkPartIdx + 1;
						// Inform the chunk cache of the chunk part skip
						FBuildPatchChunkCache::Get().SkipChunkPart( ChunkPart );
						// Wait if paused
						BuildProgress->WaitWhilePaused();
					}
					else
					{
						// No more parts on disk
						break;
					}
				}
				// Set start position to the byte we got up to
				StartPosition = ByteCounter;
				// Clean read buffer
				delete[] ReadBuffer;
				// Close file
				NewFileReader->Close();
				delete NewFileReader;
			}
		}

		// Now we can make sure the chunk cache knows to start downloading chunks
		if( !bIsFileData && !bIsDownloadStarted && !FBuildPatchInstallError::HasFatalError() )
		{
			bIsDownloadStarted = true;
			FBuildPatchChunkCache::Get().BeginDownloads();
		}

		// Attempt to create the file
		FArchive* NewFile = IFileManager::Get().CreateFileWriter( *NewFilename, bResumeExisting ? EFileWrite::FILEWRITE_Append : 0 );
		uint32 LastError = FPlatformMisc::GetLastError();
		bSuccess = NewFile != NULL;
		if( bSuccess )
		{
			// Whenever we start writing again, there's no more resuming to be done
			BuildProgress->SetStateProgress( EBuildPatchProgress::Resuming, 1.0f );

			// Seek to file write position
			NewFile->Seek( StartPosition );

			// For each chunk, load it, and place it's data into the file
			for( int32 ChunkPartIdx = StartChunkPart; ChunkPartIdx < FileManifest->FileChunkParts.Num() && bSuccess && !FBuildPatchInstallError::HasFatalError(); ++ChunkPartIdx )
			{
				const FChunkPartData& ChunkPart = FileManifest->FileChunkParts[ChunkPartIdx];
				if( bIsFileData )
				{
					bSuccess = InsertFileData( ChunkPart, *NewFile, HashState );
				}
				else
				{
					bSuccess = InsertChunkData( ChunkPart, *NewFile, HashState );
				}
				if( bSuccess )
				{
					CountBytesProcessed( ChunkPart.Size );
					// Wait if paused
					BuildProgress->WaitWhilePaused();
				}
				else
				{
					// Only report or log if the first error
					if (FBuildPatchInstallError::HasFatalError() == false)
					{
						FBuildPatchAnalytics::RecordConstructionError(Filename, INDEX_NONE, TEXT("Missing Chunk"));
						UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to chunk %s"), *Filename, *ChunkPart.Guid.ToString());
					}
					FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::InitializationError, ConstructionErrorCodes::MissingChunkData);
				}
			}

			// Close the file writer
			NewFile->Close();
			delete NewFile;
		}
		else
		{
			// Check drive space
			bool bError = false;
			uint64 TotalSize = 0;
			uint64 AvailableSpace = 0;
			if (FPlatformMisc::GetDiskTotalAndFreeSpace(InstallDirectory, TotalSize, AvailableSpace))
			{
				const int64 DriveSpace = AvailableSpace;
				const int64 RequiredSpace = FileManifest->GetFileSize();
				if (DriveSpace < RequiredSpace)
				{
					bError = true;
					// Only report or log if the first error
					if (FBuildPatchInstallError::HasFatalError() == false)
					{
						FBuildPatchAnalytics::RecordConstructionError(Filename, INDEX_NONE, TEXT("Not Enough Disk Space"));
						UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Out of disk space. Avail:%u, Needed:%u, File:%s"), DriveSpace, RequiredSpace, *Filename);
					}
					// Always set
					FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::OutOfDiskSpace, DiskSpaceErrorCodes::DuringInstallation);
				}
			}

			// Otherwise we just couldn't make the file
			if (!bError)
			{
				// Only report or log if the first error
				if (FBuildPatchInstallError::HasFatalError() == false)
				{
					FBuildPatchAnalytics::RecordConstructionError(Filename, LastError, TEXT("Could Not Create File"));
					UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Could not create %s"), *Filename);
				}
				// Always set
				FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::FileCreateFail);
			}
		}
	}
	else
	{
		// Only report or log if the first error
		if (FBuildPatchInstallError::HasFatalError() == false)
		{
			FBuildPatchAnalytics::RecordConstructionError(Filename, INDEX_NONE, TEXT("Missing File Manifest"));
			UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Missing file manifest for %s"), *Filename);
		}
		// Always set
		FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
	}

	// Verify the hash for the file that we created
	if( bSuccess )
	{
		HashState.Final();
		HashState.GetHash( HashValue.Hash );
		bSuccess = HashValue == FileManifest->FileHash;
		if( !bSuccess )
		{
			// Only report or log if the first error
			if (FBuildPatchInstallError::HasFatalError() == false)
			{
				FBuildPatchAnalytics::RecordConstructionError(Filename, INDEX_NONE, TEXT("Serialised Verify Fail"));
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Verify failed after constructing %s"), *Filename);
			}
			// Always set
			FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::OutboundCorrupt);
		}
	}

#if PLATFORM_MAC
	if( bSuccess && FileManifest->bIsUnixExecutable )
	{
		// Enable executable permission bit
		struct stat FileInfo;
		if (stat(TCHAR_TO_UTF8(*NewFilename), &FileInfo) == 0)
		{
			bSuccess = chmod(TCHAR_TO_UTF8(*NewFilename), FileInfo.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
		}
	}
#endif
	
#if PLATFORM_ANDROID || PLATFORM_ANDROIDESDEFERRED
	if (bSuccess)
	{
		IFileManager::Get().SetTimeStamp(*NewFilename, FDateTime::UtcNow());
	}
#endif

	// Delete the staging file if unsuccessful by means of construction fail (i.e. keep if canceled or download issue)
	if( !bSuccess && FBuildPatchInstallError::GetErrorState() == EBuildPatchInstallError::FileConstructionFail )
	{
		IFileManager::Get().Delete( *NewFilename, false, true );
	}

	return bSuccess;
}

bool FBuildPatchFileConstructor::InsertFileData(const FChunkPartData& ChunkPart, FArchive& DestinationFile, FSHA1& HashState)
{
	bool bSuccess = false;
	bool bLogged = false;

	// Wait for the file data to be available
	while( IsFileDataAvailable( ChunkPart.Guid ) == false )
	{
		FPlatformProcess::Sleep( 0.1f );
	}

	// Read the file
	TArray<uint8> FileData;
	FChunkHeader Header;
	const FString DataFilename = GetFileDataFilename(ChunkPart.Guid);
	bSuccess = FFileHelper::LoadFileToArray(FileData, *DataFilename);
	if (!bSuccess && !bLogged)
	{
		bLogged = true;
		FBuildPatchAnalytics::RecordConstructionError(DataFilename, FPlatformMisc::GetLastError(), TEXT("File Data Missing"));
		GLog->Logf(TEXT("BuildPatchFileConstructor: ERROR: InsertFileData could not open data file %s"), *DataFilename);
	}
	// Decompress data
	bSuccess = bSuccess && FBuildPatchUtils::UncompressFileDataFile(FileData, &Header);
	if (!bSuccess && !bLogged)
	{
		bLogged = true;
		FBuildPatchAnalytics::RecordConstructionError(DataFilename, INDEX_NONE, TEXT("File Data Uncompress Fail"));
		GLog->Logf(TEXT("BuildPatchFileConstructor: ERROR: InsertFileData: could not uncompress %s"), *FPaths::GetCleanFilename(DataFilename));
	}
	// Verify integrity
	bSuccess = bSuccess && FBuildPatchUtils::VerifyChunkFile(FileData);
	if (!bSuccess && !bLogged)
	{
		bLogged = true;
		FBuildPatchAnalytics::RecordConstructionError(DataFilename, INDEX_NONE, TEXT("File Data Verify Fail"));
		GLog->Logf(TEXT("BuildPatchFileConstructor: ERROR: InsertFileData: verification failed for %s"), *FPaths::GetCleanFilename(DataFilename));
	}
	// Check correct GUID
	bSuccess = bSuccess && (!ChunkPart.Guid.IsValid() || (ChunkPart.Guid == Header.Guid));
	if (!bSuccess && !bLogged)
	{
		bLogged = true;
		FBuildPatchAnalytics::RecordConstructionError(DataFilename, INDEX_NONE, TEXT("File Data GUID Mismatch"));
		GLog->Logf(TEXT("BuildPatchFileConstructor: ERROR: InsertFileData: mismatch GUID for %s"), *FPaths::GetCleanFilename(DataFilename));
	}

	// Continue if all was fine
	if (bSuccess)
	{
		switch (Header.StoredAs)
		{
			case FChunkHeader::STORED_RAW:
			{
				// Check we are able to get the chunk part
				const int64 StartOfPartPos = Header.HeaderSize + ChunkPart.Offset;
				const int64 EndOfPartPos = StartOfPartPos + ChunkPart.Size;
				bSuccess = EndOfPartPos <= FileData.Num();
				if (bSuccess)
				{
					HashState.Update(FileData.GetData() + StartOfPartPos, ChunkPart.Size);
					DestinationFile.Serialize(FileData.GetData() + StartOfPartPos, ChunkPart.Size);
				}
				else
				{
					FBuildPatchAnalytics::RecordConstructionError(DataFilename, INDEX_NONE, TEXT("File Data Part OOB"));
					GLog->Logf(TEXT("BuildPatchFileConstructor: ERROR: InsertFileData: part out of bounds for %s"), *FPaths::GetCleanFilename(DataFilename));
				}
			}
			break;
		default:
			FBuildPatchAnalytics::RecordConstructionError(DataFilename, INDEX_NONE, TEXT("File Data Unknown Storage"));
			GLog->Logf(TEXT("BuildPatchFileConstructor: ERROR: InsertFileData: incorrect storage method %d %s"), Header.StoredAs, *FPaths::GetCleanFilename(DataFilename));
			bSuccess = false;
			break;
		}
	}

	return bSuccess;
}

bool FBuildPatchFileConstructor::InsertChunkData(const FChunkPartData& ChunkPart, FArchive& DestinationFile, FSHA1& HashState)
{
	uint8* Data;
	uint8* DataStart;
	FChunkFile* ChunkFile = FBuildPatchChunkCache::Get().GetChunkFile( ChunkPart.Guid );
	if( ChunkFile != NULL && !FBuildPatchInstallError::HasFatalError() )
	{
		ChunkFile->GetDataLock( &Data, NULL );
		DataStart = &Data[ ChunkPart.Offset ];
		HashState.Update( DataStart, ChunkPart.Size );
		DestinationFile.Serialize( DataStart, ChunkPart.Size );
		ChunkFile->Dereference();
		ChunkFile->ReleaseDataLock();
		return true;
	}
	return false;
}


void FBuildPatchFileConstructor::DeleteDirectoryContents(const FString& RootDirectory)
{
	TArray<FString> SubDirNames;
	IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
	for (const FString& DirName : SubDirNames)
	{
		IFileManager::Get().DeleteDirectory(*(RootDirectory / DirName), false, true);
	}

	TArray<FString> SubFileNames;
	IFileManager::Get().FindFiles(SubFileNames, *(RootDirectory / TEXT("*")), true, false);
	for (const FString& FileName : SubFileNames)
	{
		IFileManager::Get().Delete(*(RootDirectory / FileName), false, true);
	}
}

/**
 * Static FBuildPatchFileConstructor variables
 */
TMap< FGuid, FString > FBuildPatchFileConstructor::FileDataAvailability;
FCriticalSection FBuildPatchFileConstructor::FileDataAvailabilityLock;

