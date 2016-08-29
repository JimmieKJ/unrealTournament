// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchInstaller.cpp: Implements the FBuildPatchInstaller class which
	controls the process of installing a build described by a build manifest.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

using namespace BuildPatchConstants;

#define LOCTEXT_NAMESPACE "BuildPatchInstaller"

#define NUM_DOWNLOAD_READINGS	5

#define TIME_PER_READING		0.5

#define MEGABYTE_VALUE			1048576

#define KILOBYTE_VALUE			1024

#define NUM_FILE_MOVE_RETRIES	5

#define NUM_INSTALLER_RETRIES	5

namespace
{
	// A simple scope timer class
	class FScopeTimer
	{
	public:
		FScopeTimer(float& InValueToSet, FCriticalSection* InSynchObject)
			: Time(InValueToSet)
			, SynchObject(InSynchObject)
		{
			StartSecs = FPlatformTime::Seconds();
		}
		~FScopeTimer()
		{
			StartSecs = FPlatformTime::Seconds() - StartSecs;
			SynchObject->Lock();
			Time = StartSecs;
			SynchObject->Unlock();
		}

	private:
		double StartSecs;
		float& Time;
		FCriticalSection* SynchObject;
	};
}

/* FBuildPatchInstaller implementation
*****************************************************************************/
FBuildPatchInstaller::FBuildPatchInstaller(FBuildPatchBoolManifestDelegate InOnCompleteDelegate, FBuildPatchAppManifestPtr CurrentManifest, FBuildPatchAppManifestRef InstallManifest, const FString& InInstallDirectory, const FString& InStagingDirectory, FBuildPatchInstallationInfo& InstallationInfoRef, bool ShouldStageOnly, const FString& InLocalMachineConfigFile, bool bInIsRepairing, bool bShouldForceSkipPrereqs)
	: Thread( NULL )
	, OnCompleteDelegate( InOnCompleteDelegate )
	, CurrentBuildManifest( CurrentManifest )
	, NewBuildManifest( InstallManifest )
	, InstallDirectory( InInstallDirectory )
	, StagingDirectory( InStagingDirectory )
	, DataStagingDir( InStagingDirectory / TEXT( "PatchData" ) )
	, InstallStagingDir( InStagingDirectory / TEXT( "Install" ) )
	, CloudDirectories( FBuildPatchServicesModule::GetCloudDirectories() )
	, BackupDirectory( FBuildPatchServicesModule::GetBackupDirectory() )
	, PreviousMoveMarker( InstallDirectory / TEXT( "$movedMarker" ) )
	, LocalMachineConfigFile( InLocalMachineConfigFile )
	, ThreadLock()
	, bIsFileData( InstallManifest->IsFileDataManifest() )
	, bIsChunkData( !bIsFileData )
	, bIsRepairing(bInIsRepairing)
	, bShouldStageOnly(ShouldStageOnly)
	, bSuccess( true )
	, bIsRunning( false )
	, bIsInited( false )
	, bForceSkipPrereqs(bShouldForceSkipPrereqs)
	, DownloadSpeedValue( -1.0 )
	, DownloadHealthValue(EBuildPatchDownloadHealth::Excellent)
	, DownloadBytesLeft( 0 )
	, BuildStats()
	, BuildProgress()
	, InitialNumChunkDownloads( 0.0f )
	, InitialNumChunkConstructions( 0.0f )
	, TotalInitialDownloadSize( 0 )
	, TimePausedAt( 0.0 )
	, InstallationInfo( InstallationInfoRef )
{
}

FBuildPatchInstaller::~FBuildPatchInstaller()
{
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FBuildPatchInstaller::SetRequiredInstallTags(const TSet<FString>& Tags)
{
	// We do not yet support changing tags after installing was started
	if (Thread != nullptr)
	{
		return false;
	}

	// Check provided tags are all valid
	TSet<FString> ValidTags;
	NewBuildManifest->GetFileTagList(ValidTags);
	if (Tags.Difference(ValidTags).Num() > 0)
	{
		return false;
	}

	// Store for use later
	InstallTags = Tags;
	return true;
}

bool FBuildPatchInstaller::StartInstallation()
{
	if (Thread == nullptr)
	{
		// Pre-process install tags. Doing this logic here means it doesn't need repeating around lower level code
		// No tags means full installation
		if (InstallTags.Num() == 0)
		{
			NewBuildManifest->GetFileTagList(InstallTags);
		}

		// Always require the empty tag
		InstallTags.Add(TEXT(""));

		// Load configuration now
		LoadLocalMachineConfig();

		// Start thread!
		const TCHAR* ThreadName = TEXT("BuildPatchInstallerThread");
		Thread = FRunnableThread::Create(this, ThreadName);
	}
	return Thread != nullptr;
}

bool FBuildPatchInstaller::Init()
{
	// Make sure the installation directory exists
	IFileManager::Get().MakeDirectory( *InstallDirectory, true );

	// Init build stats that count
	BuildStats.ProcessPausedTime = 0.0f;

	// We are ready to go if our delegates are bound and directories successfully created
	bool bInstallerInitSuccess = OnCompleteDelegate.IsBound();
	bInstallerInitSuccess &= IFileManager::Get().DirectoryExists( *InstallDirectory );

	// Currently we don't handle init failures, so make sure we are not missing them
	check( bInstallerInitSuccess );
	return bInstallerInitSuccess;
}

uint32 FBuildPatchInstaller::Run()
{
	// Make sure this function can never be parallelized
	static FCriticalSection SingletonFunctionLockCS;
	FScopeLock SingletonFunctionLock(&SingletonFunctionLockCS);
	FBuildPatchInstallError::Reset();

	SetRunning(true);
	SetInited(true);
	SetDownloadSpeed(-1);
	UpdateDownloadProgressInfo(true);

	// Register the current manifest with the installation info, to make sure we pull from it
	if (CurrentBuildManifest.IsValid())
	{
		InstallationInfo.RegisterAppInstallation(CurrentBuildManifest.ToSharedRef(), InstallDirectory);
	}

	// Keep track of files that failed verify
	TArray<FString> CorruptFiles;

	// Init prereqs progress value
	const bool bInstallPrereqs = !bForceSkipPrereqs && !NewBuildManifest->GetPrereqPath().IsEmpty();

	// Get the start time
	double StartTime = FPlatformTime::Seconds();
	double CleanUpTime = 0;

	// Keep retrying the install while it is not canceled, or caused by download error
	bool bProcessSuccess = false;
	bool bCanRetry = true;
	int32 InstallRetries = NUM_INSTALLER_RETRIES;
	while (!bProcessSuccess && bCanRetry)
	{
		// No longer queued
		BuildProgress.SetStateProgress(EBuildPatchProgress::Queued, 1.0f);

		// Run the install
		bool bInstallSuccess = RunInstallation(CorruptFiles);
		BuildProgress.SetStateProgress(EBuildPatchProgress::PrerequisitesInstall, bInstallPrereqs ? 0.0f : 1.0f);
		if (bInstallSuccess)
		{
			BuildProgress.SetStateProgress(EBuildPatchProgress::Downloading, 1.0f);
			BuildProgress.SetStateProgress(EBuildPatchProgress::Installing, 1.0f);
		}

		// Backup local changes then move generated files
		bInstallSuccess = bInstallSuccess && RunBackupAndMove();

		// There is no more potential for initializing
		BuildProgress.SetStateProgress(EBuildPatchProgress::Initializing, 1.0f);

		// Setup file attributes
		bInstallSuccess = bInstallSuccess && RunFileAttributes(bIsRepairing);

		// Run Verification
		CorruptFiles.Empty();
		bProcessSuccess = bInstallSuccess && RunVerification(CorruptFiles);

		// Clean staging if INSTALL success
		BuildProgress.SetStateProgress(EBuildPatchProgress::CleanUp, 0.0f);
		CleanUpTime = FPlatformTime::Seconds();
		if (bInstallSuccess)
		{
			if (bShouldStageOnly)
			{
				GLog->Logf(TEXT("BuildPatchServices: Deleting litter from staging area"));
				IFileManager::Get().DeleteDirectory(*DataStagingDir, false, true);
				IFileManager::Get().Delete(*(InstallStagingDir / TEXT("$resumeData")), false, true);
			}
			else
			{
				GLog->Logf(TEXT("BuildPatchServices: Deleting staging area"));
				IFileManager::Get().DeleteDirectory(*StagingDirectory, false, true);
			}
		}
		CleanUpTime = FPlatformTime::Seconds() - CleanUpTime;
		BuildProgress.SetStateProgress(EBuildPatchProgress::CleanUp, 1.0f);

		// Set if we can retry
		--InstallRetries;
		bCanRetry = InstallRetries > 0 && !FBuildPatchInstallError::IsInstallationCancelled() && !FBuildPatchInstallError::IsNoRetryError();

		// If successful or we will retry, remove the moved files marker
		if (bProcessSuccess || bCanRetry)
		{
			GLog->Logf(TEXT("BuildPatchServices: Reset MM"));
			IFileManager::Get().Delete(*PreviousMoveMarker, false, true);
		}

		// Setup end of attempt stats
		float TempFinalProgress = BuildProgress.GetProgressNoMarquee();
		{
			FScopeLock Lock(&ThreadLock);
			BuildStats.FinalProgress = TempFinalProgress;
			// If we failed, and will retry, record this failure type
			if (!bProcessSuccess && bCanRetry)
			{
				BuildStats.RetryFailureTypes.Add(FBuildPatchInstallError::GetErrorState());
				BuildStats.RetryErrorCodes.Add(FBuildPatchInstallError::GetErrorCode());
			}
		}
	}

	if (bProcessSuccess)
	{
		// Run the prerequisites installer if this is our first install and the manifest has prerequisites info
		if (bInstallPrereqs)
		{
			// @TODO: We also want to trigger prereq install if this is an update and the prereq installer differs in the update
			bProcessSuccess &= RunPrereqInstaller();
		}
	}

	// Set final stat values and log out results
	{
		FScopeLock Lock(&ThreadLock);
		bSuccess = bProcessSuccess;
		BuildStats.ProcessSuccess = bProcessSuccess;
		BuildStats.ProcessExecuteTime = (FPlatformTime::Seconds() - StartTime) - BuildStats.ProcessPausedTime;
		BuildStats.ErrorCode = FBuildPatchInstallError::GetErrorCode();
		BuildStats.FailureReasonText = FBuildPatchInstallError::GetErrorText();
		BuildStats.CleanUpTime = CleanUpTime;
		BuildStats.FailureType = FBuildPatchInstallError::GetErrorState();
		BuildStats.NumInstallRetries = NUM_INSTALLER_RETRIES - (InstallRetries+1);

		// Log stats
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: AppName: %s"), *BuildStats.AppName);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: AppInstalledVersion: %s"), *BuildStats.AppInstalledVersion);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: AppPatchVersion: %s"), *BuildStats.AppPatchVersion);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: CloudDirectory: %s"), *BuildStats.CloudDirectory);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumFilesInBuild: %u"), BuildStats.NumFilesInBuild);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumFilesOutdated: %u"), BuildStats.NumFilesOutdated);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumFilesToRemove: %u"), BuildStats.NumFilesToRemove);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumChunksRequired: %u"), BuildStats.NumChunksRequired);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ChunksQueuedForDownload: %u"), BuildStats.ChunksQueuedForDownload);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ChunksLocallyAvailable: %u"), BuildStats.ChunksLocallyAvailable);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumChunksDownloaded: %u"), BuildStats.NumChunksDownloaded);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumChunksRecycled: %u"), BuildStats.NumChunksRecycled);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumChunksCacheBooted: %u"), BuildStats.NumChunksCacheBooted);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumDriveCacheChunkLoads: %u"), BuildStats.NumDriveCacheChunkLoads);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumFailedDownloads: %u"), BuildStats.NumFailedDownloads);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumBadDownloads: %u"), BuildStats.NumBadDownloads);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumRecycleFailures: %u"), BuildStats.NumRecycleFailures);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumDriveCacheLoadFailures: %u"), BuildStats.NumDriveCacheLoadFailures);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: TotalDownloadedData: %lld"), BuildStats.TotalDownloadedData);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: AverageDownloadSpeed: %.3f MB/sec"), BuildStats.AverageDownloadSpeed / 1024.0 / 1024.0);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: TheoreticalDownloadTime: %s"), *FPlatformTime::PrettyTime(BuildStats.TheoreticalDownloadTime));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: VerifyTime: %s"), *FPlatformTime::PrettyTime(BuildStats.VerifyTime));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: CleanUpTime: %s"), *FPlatformTime::PrettyTime(BuildStats.CleanUpTime));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: PrereqTime: %s"), *FPlatformTime::PrettyTime(BuildStats.PrereqTime));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ProcessExecuteTime: %s"), *FPlatformTime::PrettyTime(BuildStats.ProcessExecuteTime));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ProcessPausedTime: %.1f sec"), BuildStats.ProcessPausedTime);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ProcessSuccess: %s"), BuildStats.ProcessSuccess ? TEXT("TRUE") : TEXT("FALSE"));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ErrorCode: %s"), *BuildStats.ErrorCode);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: FailureReasonText: %s"), *BuildStats.FailureReasonText.BuildSourceString());
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: FailureType %s"), *FBuildPatchInstallError::EnumToString(BuildStats.FailureType));
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: NumInstallRetries: %d"), BuildStats.NumInstallRetries);
		check(BuildStats.NumInstallRetries == BuildStats.RetryFailureTypes.Num() && BuildStats.NumInstallRetries == BuildStats.RetryErrorCodes.Num());
		for (uint32 RetryIdx = 0; RetryIdx < BuildStats.NumInstallRetries; ++RetryIdx)
		{
			GLog->Logf(TEXT("BuildPatchServices: Build Stat: RetryFailureType %d: %s"), RetryIdx, *FBuildPatchInstallError::EnumToString(BuildStats.RetryFailureTypes[RetryIdx]));
			GLog->Logf(TEXT("BuildPatchServices: Build Stat: RetryErrorCodes %d: %s"), RetryIdx, *BuildStats.RetryErrorCodes[RetryIdx]);
		}
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: FinalProgressValue: %f"), BuildStats.FinalProgress);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: OverallRequestSuccessRate: %f"), BuildStats.OverallRequestSuccessRate);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: ExcellentDownloadHealthTime: %f"), BuildStats.ExcellentDownloadHealthTime);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: GoodDownloadHealthTime: %f"), BuildStats.GoodDownloadHealthTime);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: OkDownloadHealthTime: %f"), BuildStats.OkDownloadHealthTime);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: PoorDownloadHealthTime: %f"), BuildStats.PoorDownloadHealthTime);
		GLog->Logf(TEXT("BuildPatchServices: Build Stat: DisconnectedDownloadHealthTime: %f"), BuildStats.DisconnectedDownloadHealthTime);
	}

	// Mark that we are done
	SetRunning(false);

	return bSuccess ? 0 : 1;
}

bool FBuildPatchInstaller::CheckForExternallyInstalledFiles()
{
	// Check the marker file for a previous installation unfinished
	if (IPlatformFile::GetPlatformPhysical().FileExists(*PreviousMoveMarker))
	{
		return true;
	}

	// If we are patching, but without the marker, we should not return true, the existing files will be old installation
	if (CurrentBuildManifest.IsValid())
	{
		return false;
	}

	// Check if any required file is potentially already in place, by comparing file size as a quick 'same file' check
	TArray<FString> BuildFiles;
	NewBuildManifest->GetFileList(BuildFiles);
	for (const FString& BuildFile : BuildFiles)
	{
		if (NewBuildManifest->GetFileSize(BuildFile) == IFileManager::Get().FileSize(*(InstallDirectory / BuildFile)))
		{
			return true;
		}
	}
	return false;
}

bool FBuildPatchInstaller::RunInstallation(TArray<FString>& CorruptFiles)
{
	GLog->Logf(TEXT("BuildPatchServices: Starting Installation"));
	// Save the staging directories
	FPaths::NormalizeDirectoryName(DataStagingDir);
	FPaths::NormalizeDirectoryName(InstallStagingDir);

	// Make sure staging directories exist
	IFileManager::Get().MakeDirectory(*DataStagingDir, true);
	IFileManager::Get().MakeDirectory(*InstallStagingDir, true);

	// Reset any error from a previous install
	FBuildPatchInstallError::Reset();
	FBuildPatchAnalytics::ResetCounters();
	BuildProgress.Reset();
	BuildProgress.SetStateProgress(EBuildPatchProgress::Queued, 1.0f);
	BuildProgress.SetStateProgress(EBuildPatchProgress::Initializing, 0.01f);
	BuildProgress.SetStateProgress(EBuildPatchProgress::CleanUp, 0.0f);

	// Remove any inventory
	FBuildPatchFileConstructor::PurgeFileDataInventory();

	// Store some totals
	const uint32 NumFilesInBuild = NewBuildManifest->GetNumFiles();

	// Save stats
	{
		FScopeLock Lock(&ThreadLock);
		BuildStats.AppName = NewBuildManifest->GetAppName();
		BuildStats.AppPatchVersion = NewBuildManifest->GetVersionString();
		BuildStats.AppInstalledVersion = CurrentBuildManifest.IsValid() ? CurrentBuildManifest->GetVersionString() : TEXT("NONE");
		BuildStats.CloudDirectory = CloudDirectories[0];
		BuildStats.NumFilesInBuild = NumFilesInBuild;
	}

	// Get the list of required files, by the tags
	TaggedFiles.Empty();
	NewBuildManifest->GetTaggedFileList(InstallTags, TaggedFiles);

	// Check if we should skip out of this process due to existing installation,
	// that will mean we start with the verification stage
	bool bFirstTimeRun = CorruptFiles.Num() == 0;
	if (bFirstTimeRun && CheckForExternallyInstalledFiles())
	{
		GLog->Logf(TEXT("BuildPatchServices: Detected previous staging completed, or existing files in target directory"));
		// Set weights for verify only
		BuildProgress.SetStateWeight(EBuildPatchProgress::Downloading, 0.0f);
		BuildProgress.SetStateWeight(EBuildPatchProgress::Installing, 0.0f);
		BuildProgress.SetStateWeight(EBuildPatchProgress::MovingToInstall, 0.0f);
		BuildProgress.SetStateWeight(EBuildPatchProgress::SettingAttributes, 0.2f);
		BuildProgress.SetStateWeight(EBuildPatchProgress::BuildVerification, 1.0f);
		// Mark all installation steps complete
		BuildProgress.SetStateProgress(EBuildPatchProgress::Initializing, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchProgress::Resuming, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchProgress::Downloading, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchProgress::Installing, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, 1.0f);
		return true;
	}

	// Get the list of files actually needing construction
	TArray<FString> FilesToConstruct;
	if (CorruptFiles.Num() > 0)
	{
		FilesToConstruct.Append(CorruptFiles);
	}
	else
	{
		TSet<FString> OutdatedFiles;
		FBuildPatchAppManifest::GetOutdatedFiles(CurrentBuildManifest, NewBuildManifest, InstallDirectory, OutdatedFiles);
		FilesToConstruct = OutdatedFiles.Intersect(TaggedFiles).Array();
	}
	GLog->Logf(TEXT("BuildPatchServices: Requiring %d files"), FilesToConstruct.Num());

	// Make sure all the files won't exceed the maximum path length
	for (const auto& FileToConstruct : FilesToConstruct)
	{
		if ((InstallStagingDir / FileToConstruct).Len() >= MAX_PATH)
		{
			GWarn->Logf(TEXT("BuildPatchServices: ERROR: Could not create new file due to exceeding maximum path length %s"), *(InstallStagingDir / FileToConstruct));
			FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::PathLengthExceeded, PathLengthErrorCodes::StagingDirectory);
			return false;
		}
	}

	// Check drive space
	uint64 TotalSize = 0;
	uint64 AvailableSpace = 0;
	if (FPlatformMisc::GetDiskTotalAndFreeSpace(InstallDirectory, TotalSize, AvailableSpace))
	{
		const int64 DriveSpace = AvailableSpace;
		const int64 RequiredSpace = NewBuildManifest->GetFileSize(FilesToConstruct);
		if (DriveSpace < RequiredSpace)
		{
			GWarn->Logf(TEXT("BuildPatchServices: ERROR: Could not begin install due to their not being enough HDD space. Needs %db, Free %db"), RequiredSpace, DriveSpace);
			FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::OutOfDiskSpace, DiskSpaceErrorCodes::InitialSpaceCheck);
			return false;
		}
	}

	// Create the downloader
	FBuildPatchDownloader::Create(DataStagingDir, CloudDirectories, NewBuildManifest, &BuildProgress);

	// Set initial health
	SetDownloadHealth(FBuildPatchDownloader::Get().GetDownloadHealth());

	// Create chunk cache
	if (bIsChunkData)
	{
		FBuildPatchChunkCache::Init(NewBuildManifest, CurrentBuildManifest, DataStagingDir, InstallDirectory, &BuildProgress, FilesToConstruct, InstallationInfo);
	}

	// Hold the file constructor thread
	FBuildPatchFileConstructor* FileConstructor = NULL;

	// Stats for build
	const uint32 NumFilesToConstruct = bIsFileData ? NumFilesInBuild : FBuildPatchChunkCache::Get().GetStatNumFilesToConstruct();
	const uint32 NumRequiredChunks = bIsFileData ? NumFilesInBuild : FBuildPatchChunkCache::Get().GetStatNumRequiredChunks();
	const uint32 NumChunksToDownload = bIsFileData ? NumFilesInBuild : FBuildPatchChunkCache::Get().GetStatNumChunksToDownload();
	const uint32 NumChunksToConstruct = bIsFileData ? 0 : FBuildPatchChunkCache::Get().GetStatNumChunksToRecycle();

	// Save stats
	{
		FScopeLock Lock(&ThreadLock);
		BuildStats.NumFilesOutdated = NumFilesToConstruct;
		BuildStats.NumChunksRequired = NumRequiredChunks;
		BuildStats.ChunksQueuedForDownload = NumChunksToDownload;
		BuildStats.ChunksLocallyAvailable = NumChunksToConstruct;
	}

	// Save initial counts as float for use with progress updates
	InitialNumChunkDownloads = NumChunksToDownload;
	InitialNumChunkConstructions = NumChunksToConstruct;

	// Setup some weightings for the progress tracking
	const float NumRequiredChunksFloat = NumRequiredChunks;
	const bool bHasFileAttributes = NewBuildManifest->HasFileAttributes();
	const float AttributesWeight = bHasFileAttributes ? bIsRepairing ? 1.0f / 50.0f : 1.0f / 20.0f : 0.0f;
	BuildProgress.SetStateWeight(EBuildPatchProgress::Downloading, NumRequiredChunksFloat > 0.0f ? InitialNumChunkDownloads / NumRequiredChunksFloat : 0.0f);
	BuildProgress.SetStateWeight(EBuildPatchProgress::Installing, NumRequiredChunksFloat > 0.0f ? 0.1f + (InitialNumChunkConstructions / NumRequiredChunksFloat) : 0.0f);
	BuildProgress.SetStateWeight(EBuildPatchProgress::MovingToInstall, NumFilesToConstruct > 0 ? 0.05f : 0.0f);
	BuildProgress.SetStateWeight(EBuildPatchProgress::SettingAttributes, AttributesWeight);
	BuildProgress.SetStateWeight(EBuildPatchProgress::BuildVerification, 1.1f / 9.0f);

	// If this is a repair operation, start off with install and download complete
	if (bIsRepairing)
	{
		GLog->Logf(TEXT("BuildPatchServices: Performing a repair operation"));
		BuildProgress.SetStateProgress(EBuildPatchProgress::Downloading, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchProgress::Installing, 1.0f);
		BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, 1.0f);
	}

	// Start the file constructor
	GLog->Logf(TEXT("BuildPatchServices: Starting file contruction worker"));
	FileConstructor = new FBuildPatchFileConstructor(CurrentBuildManifest, NewBuildManifest, InstallDirectory, InstallStagingDir, FilesToConstruct, &BuildProgress);

	// Initializing is now complete if we are constructing files
	BuildProgress.SetStateProgress(EBuildPatchProgress::Initializing, NumFilesToConstruct > 0 ? 1.0f : 0.0f);

	// If this is file data, queue the download list
	if (bIsFileData)
	{
		TArray< FGuid > RequiredFileData;
		NewBuildManifest->GetChunksRequiredForFiles(FilesToConstruct, RequiredFileData);
		FBuildPatchDownloader::Get().AddChunksToDownload(RequiredFileData);
		TotalInitialDownloadSize = NewBuildManifest->GetDataSize(RequiredFileData);
	}
	else
	{
		TotalInitialDownloadSize = FBuildPatchChunkCache::Get().GetStatTotalChunkDownloadSize();
	}

	// Wait for the file constructor to complete
	while (FileConstructor->IsComplete() == false)
	{
		UpdateDownloadProgressInfo();
		FPlatformProcess::Sleep(0.1f);
	}
	FileConstructor->Wait();
	delete FileConstructor;
	FileConstructor = NULL;
	GLog->Logf(TEXT("BuildPatchServices: File construction complete"));

	// Wait for downloader to complete
	FBuildPatchDownloader::Get().NotifyNoMoreChunksToAdd();
	while (FBuildPatchDownloader::Get().IsComplete() == false)
	{
		UpdateDownloadProgressInfo();
		FPlatformProcess::Sleep(0.0f);
	}
	TArray< FBuildPatchDownloadRecord > AllChunkDownloads = FBuildPatchDownloader::Get().GetDownloadRecordings();
	double LastDownloadSpeedSeen = GetDownloadSpeed();
	SetDownloadSpeed(-1);

	// Cache the final download health
	SetDownloadHealth(FBuildPatchDownloader::Get().GetDownloadHealth());

	// Calculate the average download speed from the recordings
	// NB: Because we are threading several downloads at once this is not simply averaging every download. We have to know about
	//     how much data is being received simultaneously too. We also need to ignore download pauses.
	int64 TotalDownloadedBytes = 0;
	double TotalTimeDownloading = 0;
	double RecoredEndTime = 0;
	if (AllChunkDownloads.Num() > 0)
	{
		// Sort by start time
		AllChunkDownloads.Sort();
		// Start with first record
		TotalTimeDownloading = AllChunkDownloads[0].EndTime - AllChunkDownloads[0].StartTime;
		TotalDownloadedBytes = AllChunkDownloads[0].DownloadSize;
		RecoredEndTime = AllChunkDownloads[0].EndTime;
		// For every other record..
		for (int32 RecordIdx = 1; RecordIdx < AllChunkDownloads.Num(); ++RecordIdx)
		{
			// Do we have some time to count
			if (RecoredEndTime < AllChunkDownloads[RecordIdx].EndTime)
			{
				// Was there a break in downloading
				if (AllChunkDownloads[RecordIdx].StartTime > RecoredEndTime)
				{
					TotalTimeDownloading += AllChunkDownloads[RecordIdx].EndTime - AllChunkDownloads[RecordIdx].StartTime;
				}
				// Otherwise don't count time overlap
				else
				{
					TotalTimeDownloading += AllChunkDownloads[RecordIdx].EndTime - RecoredEndTime;
				}
				RecoredEndTime = AllChunkDownloads[RecordIdx].EndTime;
			}
			// Count all bytes
			TotalDownloadedBytes += AllChunkDownloads[RecordIdx].DownloadSize;
		}
	}

	// Set final stats
	{
		FScopeLock Lock(&ThreadLock);
		BuildStats.TotalDownloadedData = TotalDownloadedBytes;
		BuildStats.NumChunksDownloaded = AllChunkDownloads.Num();
		const double TotalDownloadedBytesDouble = TotalDownloadedBytes;
		BuildStats.AverageDownloadSpeed = TotalTimeDownloading > 0 ? TotalDownloadedBytesDouble / TotalTimeDownloading : 0;
		BuildStats.TheoreticalDownloadTime = TotalTimeDownloading;
		BuildStats.NumChunksRecycled = bIsFileData ? 0 : FBuildPatchChunkCache::Get().GetCounterChunksRecycled();
		BuildStats.NumChunksCacheBooted = bIsFileData ? 0 : FBuildPatchChunkCache::Get().GetCounterChunksCacheBooted();
		BuildStats.NumDriveCacheChunkLoads = bIsFileData ? 0 : FBuildPatchChunkCache::Get().GetCounterDriveCacheChunkLoads();
		BuildStats.NumFailedDownloads = FBuildPatchDownloader::Get().GetNumFailedDownloads();
		BuildStats.NumBadDownloads = FBuildPatchDownloader::Get().GetNumBadDownloads();
		BuildStats.NumRecycleFailures = bIsFileData ? 0 : FBuildPatchChunkCache::Get().GetCounterRecycleFailures();
		BuildStats.NumDriveCacheLoadFailures = bIsFileData ? 0 : FBuildPatchChunkCache::Get().GetCounterDriveCacheLoadFailures();
		BuildStats.OverallRequestSuccessRate = FBuildPatchDownloader::Get().GetDownloadSuccessRate();
		TArray<float> HealthTimers = FBuildPatchDownloader::Get().GetDownloadHealthTimers();
		BuildStats.ExcellentDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Excellent];
		BuildStats.GoodDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Good];
		BuildStats.OkDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::OK];
		BuildStats.PoorDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Poor];
		BuildStats.DisconnectedDownloadHealthTime = HealthTimers[(int32)EBuildPatchDownloadHealth::Disconnected];
		BuildStats.FinalDownloadSpeed = LastDownloadSpeedSeen;
	}

	// Perform static cleanup
	if (bIsChunkData)
	{
		FBuildPatchChunkCache::Shutdown();
	}
	FBuildPatchDownloader::Shutdown();
	FBuildPatchFileConstructor::PurgeFileDataInventory();

	GLog->Logf(TEXT("BuildPatchServices: Staged install complete"));

	return !FBuildPatchInstallError::HasFatalError();
}


void FBuildPatchInstaller::CleanupEmptyDirectories(const FString& RootDirectory)
{
	TArray<FString> SubDirNames;
	IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
	for(auto DirName : SubDirNames)
	{
		CleanupEmptyDirectories(*(RootDirectory / DirName));
	}

	TArray<FString> SubFileNames;
	IFileManager::Get().FindFilesRecursive(SubFileNames, *RootDirectory, TEXT("*.*"), true, false);
	if (SubFileNames.Num() == 0)
	{
#if PLATFORM_MAC
		// On Mac we need to delete the .DS_Store file, but FindFiles() skips .DS_Store files.
		IFileManager::Get().Delete(*(RootDirectory / TEXT(".DS_Store")), false, true);
#endif

		bool bDeleteSuccess = IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
		const uint32 LastError = FPlatformMisc::GetLastError();
		GLog->Logf(TEXT("BuildPatchServices: Deleted Empty Folder (%u,%u) %s"), bDeleteSuccess ? 1 : 0, LastError, *RootDirectory);
	}
}

void FBuildPatchInstaller::LoadLocalMachineConfig()
{
	check(IsInGameThread());
	TArray<FString> ConfigStrings;
	GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), ConfigStrings, LocalMachineConfigFile);
	ThreadLock.Lock();
	InstalledPrereqs.Append(ConfigStrings);
	ThreadLock.Unlock();
}

void FBuildPatchInstaller::SaveLocalMachineConfig()
{
	check(IsInGameThread());
	ThreadLock.Lock();
	GConfig->SetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), InstalledPrereqs.Array(), LocalMachineConfigFile);
	ThreadLock.Unlock();
}

bool FBuildPatchInstaller::RunBackupAndMove()
{
	// We skip this step if performing stage only
	bool bMoveSuccess = true;
	if (bShouldStageOnly)
	{
		GLog->Logf(TEXT("BuildPatchServices: Skipping backup and stage relocation"));
		BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, 1.0f);
	}
	else
	{
		GLog->Logf(TEXT("BuildPatchServices: Running backup and stage relocation"));
		// If there's no error, move all complete files
		bMoveSuccess = FBuildPatchInstallError::HasFatalError() == false;
		if (bMoveSuccess)
		{
			// First handle files that should be removed for patching
			TArray< FString > FilesToRemove;
			if (CurrentBuildManifest.IsValid())
			{
				FBuildPatchAppManifest::GetRemovableFiles(CurrentBuildManifest.ToSharedRef(), NewBuildManifest, FilesToRemove);
			}
			// And also files that may no longer be required (removal of tags)
			TArray<FString> NewBuildFiles;
			NewBuildManifest->GetFileList(NewBuildFiles);
			TSet<FString> NewBuildFilesSet(NewBuildFiles);
			TSet<FString> RemovableBuildFiles = NewBuildFilesSet.Difference(TaggedFiles);
			FilesToRemove.Append(RemovableBuildFiles.Array());
			// Add to build stats
			ThreadLock.Lock();
			BuildStats.NumFilesToRemove = FilesToRemove.Num();
			ThreadLock.Unlock();
			for (const FString& OldFilename : FilesToRemove)
			{
				BackupFileIfNecessary(OldFilename);
				const bool bDeleteSuccess = IFileManager::Get().Delete(*(InstallDirectory / OldFilename), false, true, true);
				const uint32 LastError = FPlatformMisc::GetLastError();
				GLog->Logf(TEXT("BuildPatchServices: Removed (%u,%u) %s"), bDeleteSuccess ? 1 : 0, LastError, *OldFilename);
			}

			// Now handle files that have been constructed
			bool bSavedMoveMarkerFile = false;
			TArray< FString > ConstructionFiles;
			NewBuildManifest->GetFileList(ConstructionFiles);
			BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, 0.0f);
			const float NumConstructionFilesFloat = ConstructionFiles.Num();
			for (auto ConstructionFilesIt = ConstructionFiles.CreateConstIterator(); ConstructionFilesIt && bMoveSuccess && !FBuildPatchInstallError::HasFatalError(); ++ConstructionFilesIt)
			{
				const FString& ConstructionFile = *ConstructionFilesIt;
				const FString SrcFilename = InstallStagingDir / ConstructionFile;
				const FString DestFilename = InstallDirectory / ConstructionFile;
				const float FileIndexFloat = ConstructionFilesIt.GetIndex();
				// Skip files not constructed
				if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*SrcFilename))
				{
					BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, FileIndexFloat / NumConstructionFilesFloat);
					continue;
				}
				// Create the move marker file
				if (!bSavedMoveMarkerFile)
				{
					bSavedMoveMarkerFile = true;
					GLog->Logf(TEXT("BuildPatchServices: Create MM"));
					FArchive* MoveMarkerFile = IFileManager::Get().CreateFileWriter(*PreviousMoveMarker, FILEWRITE_EvenIfReadOnly);
					if (MoveMarkerFile != NULL)
					{
						MoveMarkerFile->Close();
						delete MoveMarkerFile;
					}
					// Make sure we have some progress if we do some work
					if (BuildProgress.GetStateWeight(EBuildPatchProgress::MovingToInstall) <= 0.0f)
					{
						BuildProgress.SetStateWeight(EBuildPatchProgress::MovingToInstall, 0.1f);
					}
				}
				// Backup file if need be
				BackupFileIfNecessary(ConstructionFile);
				// Delete existing if there
				IFileManager::Get().Delete(*DestFilename, false, true, false);
				// Move the file to the installation directory
				int32 MoveRetries = NUM_FILE_MOVE_RETRIES;
				bMoveSuccess = IFileManager::Get().Move(*DestFilename, *SrcFilename, true, true, true, true);
				uint32 ErrorCode = FPlatformMisc::GetLastError();
				while (!bMoveSuccess && MoveRetries > 0)
				{
					--MoveRetries;
					FBuildPatchAnalytics::RecordConstructionError(ConstructionFile, ErrorCode, TEXT("Failed To Move"));
					UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to move file %s (%d), trying copy"), *ConstructionFile, ErrorCode);
					bMoveSuccess = IFileManager::Get().Copy(*DestFilename, *SrcFilename, true, true, true) == COPY_OK;
					ErrorCode = FPlatformMisc::GetLastError();
					if (!bMoveSuccess)
					{
						UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to copy file %s (%d), retying after 0.5 sec"), *ConstructionFile, ErrorCode);
						FPlatformProcess::Sleep(0.5f);
						bMoveSuccess = IFileManager::Get().Move(*DestFilename, *SrcFilename, true, true, true, true);
						ErrorCode = FPlatformMisc::GetLastError();
					}
					else
					{
						IFileManager::Get().Delete(*SrcFilename, false, true, false);
					}
				}
				if (!bMoveSuccess)
				{
					UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to move file %s"), *FPaths::GetCleanFilename(ConstructionFile));
					FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::MoveFileToInstall, MoveErrorCodes::StageToInstall);
				}
				else
				{
					FilesInstalled.Add(ConstructionFile);
					BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, FileIndexFloat / NumConstructionFilesFloat);
				}
			}

			// After we've completed deleting/moving patch files to the install directory, clean up any empty directories left over
			CleanupEmptyDirectories(InstallDirectory);

			bMoveSuccess = bMoveSuccess && (FBuildPatchInstallError::HasFatalError() == false);
			if (bMoveSuccess)
			{
				BuildProgress.SetStateProgress(EBuildPatchProgress::MovingToInstall, 1.0f);
			}
		}
		GLog->Logf(TEXT("BuildPatchServices: Relocation complete %d"), bMoveSuccess?1:0);
	}
	return bMoveSuccess;
}

bool FBuildPatchInstaller::RunFileAttributes(bool bForce)
{
	// Only provide stage directory if stage-only mode
	FString EmptyString;
	FString& OptionalStageDirectory = bShouldStageOnly ? InstallStagingDir : EmptyString;

	// Construct the attributes class
	auto Attributes = FBuildPatchFileAttributesFactory::Create(NewBuildManifest, CurrentBuildManifest, InstallDirectory, OptionalStageDirectory, &BuildProgress);
	Attributes->ApplyAttributes(bForce);

	// We don't fail on this step currently
	return true;
}

bool FBuildPatchInstaller::RunVerification(TArray< FString >& CorruptFiles)
{
	// Make sure this function can never be parallelized
	static FCriticalSection SingletonFunctionLockCS;
	FScopeLock SingletonFunctionLock(&SingletonFunctionLockCS);

	BuildProgress.SetStateProgress(EBuildPatchProgress::BuildVerification, 0.0f);

	// Verify the installation
	double VerifyTime = 0;
	double VerifyPauseTime;
	GLog->Logf(TEXT("BuildPatchServices: Verifying install"));
	CorruptFiles.Empty();
	VerifyTime = FPlatformTime::Seconds();

	// Setup the verify delegates
	auto ProgressDelegate = FBuildPatchFloatDelegate::CreateRaw(this, &FBuildPatchInstaller::UpdateVerificationProgress);
	auto IsPausedDelegate = FBuildPatchBoolRetDelegate::CreateRaw(this, &FBuildPatchInstaller::IsPaused);

	// Only provide stage directory if stage-only mode
	FString EmptyString;
	FString& OptionalStageDirectory = bShouldStageOnly ? InstallStagingDir : EmptyString;

	// Construct the verifier
	auto Verifier = FBuildPatchVerificationFactory::Create(NewBuildManifest, ProgressDelegate, IsPausedDelegate, InstallDirectory, OptionalStageDirectory);

	// Verify the build
	Verifier->SetRequiredFiles(TaggedFiles.Array());
	bool bVerifySuccess = Verifier->VerifyAgainstDirectory(CorruptFiles, VerifyPauseTime);
	VerifyTime = FPlatformTime::Seconds() - VerifyTime - VerifyPauseTime;
	if (!bVerifySuccess)
	{
		UE_LOG(LogBuildPatchServices, Error, TEXT("Build verification failed on %u file(s)"), CorruptFiles.Num());
		FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::BuildVerifyFail, VerifyErrorCodes::FinalCheck);
	}

	ThreadLock.Lock();
	BuildStats.VerifyTime = VerifyTime;
	ThreadLock.Unlock();

	BuildProgress.SetStateProgress(EBuildPatchProgress::BuildVerification, 1.0f);

	// Delete/Backup any incorrect files if failure was not cancellation
	if (!FBuildPatchInstallError::IsInstallationCancelled())
	{
		for (const FString& CorruptFile : CorruptFiles)
		{
			BackupFileIfNecessary(CorruptFile, true);
			if (!bShouldStageOnly)
			{
				IFileManager::Get().Delete(*(InstallDirectory / CorruptFile), false, true);
			}
			IFileManager::Get().Delete(*(InstallStagingDir / CorruptFile), false, true);
		}
	}

	GLog->Logf(TEXT("BuildPatchServices: Verify stage complete %d"), bVerifySuccess ? 1 : 0);

	return bVerifySuccess;
}

bool FBuildPatchInstaller::BackupFileIfNecessary(const FString& Filename, bool bDiscoveredByVerification /*= false */)
{
	const FString InstalledFilename = InstallDirectory / Filename;
	const FString BackupFilename = BackupDirectory / Filename;
	const bool bBackupOriginals = !BackupDirectory.IsEmpty();
	// Skip if not doing backups
	if (!bBackupOriginals)
	{
		return true;
	}
	// Skip if no file to backup
	const bool bInstalledFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*InstalledFilename);
	if (!bInstalledFileExists)
	{
		return true;
	}
	// Skip if already backed up
	const bool bAlreadyBackedUp = FPlatformFileManager::Get().GetPlatformFile().FileExists(*BackupFilename);
	if (bAlreadyBackedUp)
	{
		return true;
	}
	// Skip if the target file was already copied to the installation
	const bool bAlreadyInstalled = FilesInstalled.Contains(Filename);
	if (bAlreadyInstalled)
	{
		return true;
	}
	// If discovered by verification, but the patching system did not touch the file, we know it must be backed up.
	// If patching system touched the file it would already have been backed up
	if (bDiscoveredByVerification && CurrentBuildManifest.IsValid() && !FBuildPatchAppManifest::IsFileOutdated(CurrentBuildManifest.ToSharedRef(), NewBuildManifest, Filename))
	{
		return IFileManager::Get().Move(*BackupFilename, *InstalledFilename, true, true, true);
	}
	bool bUserEditedFile = bDiscoveredByVerification;
	const bool bCheckFileChanges = !bDiscoveredByVerification;
	if (bCheckFileChanges)
	{
		const FFileManifestData* OldFileManifest = CurrentBuildManifest.IsValid() ? CurrentBuildManifest->GetFileManifest(Filename) : nullptr;
		const FFileManifestData* NewFileManifest = NewBuildManifest->GetFileManifest(Filename);
		const int64 InstalledFilesize = IFileManager::Get().FileSize(*InstalledFilename);
		const int64 OriginalFileSize = OldFileManifest ? OldFileManifest->GetFileSize() : INDEX_NONE;
		const int64 NewFileSize = NewFileManifest ? NewFileManifest->GetFileSize() : INDEX_NONE;
		const FSHAHashData HashZero;
		const FSHAHashData& HashOld = OldFileManifest ? OldFileManifest->FileHash : HashZero;
		const FSHAHashData& HashNew = NewFileManifest ? NewFileManifest->FileHash : HashZero;
		const bool bFileSizeDiffers = OriginalFileSize != InstalledFilesize && NewFileSize != InstalledFilesize;
		bUserEditedFile = bFileSizeDiffers || FBuildPatchUtils::VerifyFile(InstalledFilename, HashOld, HashNew) == 0;
	}
	// Finally, use the above logic to determine if we must do the backup
	const bool bNeedBackup = bUserEditedFile;
	bool bBackupSuccess = true;
	if (bNeedBackup)
	{
		GLog->Logf(TEXT("BuildPatchServices: Backing up %s"), *Filename);
		bBackupSuccess = IFileManager::Get().Move(*BackupFilename, *InstalledFilename, true, true, true);
	}
	return bBackupSuccess;
}

bool FBuildPatchInstaller::RunPrereqInstaller()
{
	FScopeTimer ScopeTimer(BuildStats.PrereqTime, &ThreadLock);
	BuildProgress.SetStateProgress(EBuildPatchProgress::PrerequisitesInstall, 0.0f);

	// The prereq fields support some known variables
	static const TCHAR* RootDirectoryVariable = TEXT("$[RootDirectory]");
	static const TCHAR* LogDirectoryVariable = TEXT("$[LogDirectory]");
	static const TCHAR* QuoteVariable = TEXT("$[Quote]");
	static const TCHAR* Quote = TEXT("\"");
	const FString InstallDirWithSlash = InstallDirectory / TEXT("");
	const FString StageDirWithSlash = InstallStagingDir / TEXT("");
	const FString LogDirWithSlash = FPaths::ConvertRelativePathToFull(FPaths::GameLogDir() / TEXT(""));

	// Get the hash string for the prereq so we can use it to check and set if already installed previously
	FString PrereqHashString;
	{
		FString PrereqFilename = NewBuildManifest->GetPrereqPath();
		PrereqFilename.ReplaceInline(TEXT("\\"), TEXT("/"));
		FSHAHashData PrereqHash;
		if (NewBuildManifest->GetFileHash(PrereqFilename, PrereqHash))
		{
			PrereqHashString = PrereqHash.ToString();
		}
	}

	// Check to see if we stored a successful run of this prerequisite already, and can therefore skip it.
	// We only skip if we are not attempting a repair.
	if (!bIsRepairing && !PrereqHashString.IsEmpty())
	{
		FScopeLock ScopeLock(&ThreadLock);
		if (InstalledPrereqs.Contains(PrereqHashString))
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Skipping already installed prerequisites installer"));
			BuildProgress.SetStateProgress(EBuildPatchProgress::PrerequisitesInstall, 1.0f);
			return true;
		}
	}

	FString PrereqPath;
	bool bUsingInstallRoot = false;
	bool bUsingStageRoot = false;
	if (bShouldStageOnly)
	{
		PrereqPath = NewBuildManifest->GetPrereqPath();
		if (PrereqPath.ReplaceInline(RootDirectoryVariable, *StageDirWithSlash) == 0)
		{
			PrereqPath = StageDirWithSlash + NewBuildManifest->GetPrereqPath();
		}
		if (IFileManager::Get().FileSize(*PrereqPath) > 0)
		{
			bUsingStageRoot = true;
		}
	}
	if (!bUsingStageRoot)
	{
		PrereqPath = NewBuildManifest->GetPrereqPath();
		if (PrereqPath.ReplaceInline(RootDirectoryVariable, *InstallDirWithSlash) == 0)
		{
			PrereqPath = InstallDirWithSlash + NewBuildManifest->GetPrereqPath();
		}
		if (IFileManager::Get().FileSize(*PrereqPath) > 0)
		{
			bUsingInstallRoot = true;
		}
	}
	// If we found no prerequisite above, then we have nothing to run and this is an error in the shipped build
	if (!bUsingInstallRoot && !bUsingStageRoot)
	{
		PrereqPath = NewBuildManifest->GetPrereqPath();
	}

	const FString PrereqCommandline = NewBuildManifest->GetPrereqArgs()
		.Replace(RootDirectoryVariable, *InstallDirWithSlash)
		.Replace(LogDirectoryVariable, *LogDirWithSlash)
		.Replace(QuoteVariable, Quote);

	UE_LOG(LogBuildPatchServices, Log, TEXT("Running prerequisites installer %s %s"), *PrereqPath, *PrereqCommandline);

	// Prerequisites have to be ran elevated otherwise a background run of the prereq which asks for elevation itself
	// on some OSs will result in a minimised or un-focused request.
	int32 ReturnCode = INDEX_NONE;
	bool bPrereqInstallSuccessful = FPlatformProcess::ExecElevatedProcess(*PrereqPath, *PrereqCommandline, &ReturnCode);
	if (!bPrereqInstallSuccessful)
	{
		ReturnCode = FPlatformMisc::GetLastError();
		UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to start the prerequisites install process %u"), ReturnCode);
		FBuildPatchAnalytics::RecordPrereqInstallationError(NewBuildManifest->GetAppName(), NewBuildManifest->GetVersionString(), PrereqPath, PrereqCommandline, ReturnCode, TEXT("Failed to start installer"));
		FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::PrerequisiteError, *FString::Printf(TEXT("%s%u"), PrerequisiteErrorPrefixes::ExecuteCode, ReturnCode));
	}
	else
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Prerequisites executable failed with code %u"), ReturnCode);
			FBuildPatchAnalytics::RecordPrereqInstallationError(NewBuildManifest->GetAppName(), NewBuildManifest->GetVersionString(), PrereqPath, PrereqCommandline, ReturnCode, TEXT("Failed to install"));
			bPrereqInstallSuccessful = false;
			FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::PrerequisiteError, *FString::Printf(TEXT("%s%u"), PrerequisiteErrorPrefixes::ReturnCode, ReturnCode));
		}
	}

	if (bPrereqInstallSuccessful)
	{
		FScopeLock ScopeLock(&ThreadLock);
		BuildProgress.SetStateProgress(EBuildPatchProgress::PrerequisitesInstall, 1.0f);
		InstalledPrereqs.Add(PrereqHashString);
	}

	return bPrereqInstallSuccessful;
}

void FBuildPatchInstaller::SetRunning( bool bRunning )
{
	FScopeLock Lock( &ThreadLock );
	bIsRunning = bRunning;
}

void FBuildPatchInstaller::SetInited( bool bInited )
{
	FScopeLock Lock( &ThreadLock );
	bIsInited = bInited;
}

void FBuildPatchInstaller::SetDownloadSpeed( double ByteSpeed )
{
	FScopeLock Lock( &ThreadLock );
	DownloadSpeedValue = ByteSpeed;
}

void FBuildPatchInstaller::SetDownloadBytesLeft( const int64& BytesLeft )
{
	FScopeLock Lock( &ThreadLock );
	DownloadBytesLeft = BytesLeft;
}

void FBuildPatchInstaller::UpdateDownloadProgressInfo( bool bReset )
{
	// Static variables for persistent values
	static double LastTime = FPlatformTime::Seconds();
	static double NowTime = 0;
	static double DeltaTime = 0;
	static double LastReadingTime = 0;
	static double LastDataReadings[NUM_DOWNLOAD_READINGS] = { 0 };
	static double LastTimeReadings[NUM_DOWNLOAD_READINGS] = { 0 };
	static uint32 ReadingIdx = 0;
	static bool bProgressIsDownload = true;
	static double AverageDownloadSpeed = 0;

	// Reset internals?
	if( bReset )
	{
		LastTime = FPlatformTime::Seconds();
		LastReadingTime = LastTime;
		NowTime = 0;
		DeltaTime = 0;
		for (int32 i = 0; i < NUM_DOWNLOAD_READINGS; ++i)
		{
			LastDataReadings[i] = 0;
			LastTimeReadings[i] = 0;
		}
		ReadingIdx = 0;
		bProgressIsDownload = true;
		AverageDownloadSpeed = 0;
		return;
	}

	// Return if not downloading yet
	if( !NewBuildManifest->IsFileDataManifest() && !FBuildPatchChunkCache::Get().HaveDownloadsStarted() )
	{
		return;
	}

	// Cache the download health.
	EBuildPatchDownloadHealth CurrentDownloadHealth = FBuildPatchDownloader::Get().GetDownloadHealth();
	SetDownloadHealth(CurrentDownloadHealth);

	// Calculate percentage complete based on number of chunks
	const int64 DownloadNumBytesLeft = FBuildPatchDownloader::Get().GetNumBytesLeft();
	const float DownloadSizeFloat = TotalInitialDownloadSize;
	const float DownloadBytesLeftFloat = DownloadNumBytesLeft;
	const float DownloadProgress = 1.0f - (TotalInitialDownloadSize > 0 ? DownloadBytesLeftFloat / DownloadSizeFloat : 0.0f);
	BuildProgress.SetStateProgress( EBuildPatchProgress::Downloading, DownloadProgress );

	// Calculate the average download speed
	NowTime = FPlatformTime::Seconds();
	DeltaTime += NowTime - LastTime;
	if( DeltaTime > TIME_PER_READING )
	{
		const double BytesDownloaded = FBuildPatchDownloader::Get().GetByteDownloadCountReset();
		const double TimeSinceLastReading = NowTime - LastReadingTime;
		LastReadingTime = NowTime;
		LastDataReadings[ReadingIdx] = BytesDownloaded;
		LastTimeReadings[ReadingIdx] = TimeSinceLastReading;
		ReadingIdx = (ReadingIdx + 1) % NUM_DOWNLOAD_READINGS;
		DeltaTime = 0;
		double TotalData = 0;
		double TotalTime = 0;
		for (uint32 i = 0; i < NUM_DOWNLOAD_READINGS; ++i)
		{
			TotalData += LastDataReadings[i];
			TotalTime += LastTimeReadings[i];
		}
		AverageDownloadSpeed = TotalData / TotalTime;
	}

	// Use 0 speed while disconnected - this avoids the slow trail off of averaging.
	if (CurrentDownloadHealth == EBuildPatchDownloadHealth::Disconnected)
	{
		// Don't adjust download bytes left either.
		SetDownloadSpeed(0.0f);
	}
	else
	{
		// Set download values
		SetDownloadSpeed( DownloadProgress < 1.0f ? AverageDownloadSpeed : -1.0f );
		SetDownloadBytesLeft( DownloadNumBytesLeft );
	}

	// If we are paused download speed should register as not downloading
	if (BuildProgress.GetPauseState())
	{
		SetDownloadSpeed(-1);
	}

	// Set last time
	LastTime = NowTime;
}

//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
FText FBuildPatchInstaller::GetDownloadSpeedText()
{
	static const FText DownloadSpeedFormat = LOCTEXT("BuildPatchInstaller_DownloadSpeedFormat", "{Current} / {Total} ({Speed}/sec)");

	FScopeLock Lock(&ThreadLock);
	FText SpeedDisplayedText;
	if (DownloadSpeedValue >= 0)
	{
		FNumberFormattingOptions SpeedFormattingOptions;
		SpeedFormattingOptions.MaximumFractionalDigits = 1;
		SpeedFormattingOptions.MinimumFractionalDigits = 0;

		FNumberFormattingOptions SizeFormattingOptions;
		SizeFormattingOptions.MaximumFractionalDigits = 1;
		SizeFormattingOptions.MinimumFractionalDigits = 1;

		FFormatNamedArguments Args;
		Args.Add(TEXT("Speed"), FText::AsMemory(DownloadSpeedValue, &SpeedFormattingOptions));
		Args.Add(TEXT("Total"), FText::AsMemory(TotalInitialDownloadSize, &SizeFormattingOptions));
		Args.Add(TEXT("Current"), FText::AsMemory(TotalInitialDownloadSize - DownloadBytesLeft, &SizeFormattingOptions));
		return FText::Format(DownloadSpeedFormat, Args);
	}

	return FText();
}

double FBuildPatchInstaller::GetDownloadSpeed() const
{
	FScopeLock Lock(&ThreadLock);
	return DownloadSpeedValue;
}

int64 FBuildPatchInstaller::GetInitialDownloadSize() const
{
	FScopeLock Lock(&ThreadLock);
	return TotalInitialDownloadSize;
}

int64 FBuildPatchInstaller::GetTotalDownloaded() const
{
	FScopeLock Lock(&ThreadLock);
	return TotalInitialDownloadSize - DownloadBytesLeft;
}

bool FBuildPatchInstaller::IsComplete()
{
	FScopeLock Lock( &ThreadLock );
	return !bIsRunning && bIsInited;
}

bool FBuildPatchInstaller::IsCanceled()
{
	FScopeLock Lock( &ThreadLock );
	return BuildStats.FailureType == EBuildPatchInstallError::UserCanceled;
}

bool FBuildPatchInstaller::IsPaused()
{
	return BuildProgress.GetPauseState();
}

bool FBuildPatchInstaller::IsResumable()
{
	FScopeLock Lock( &ThreadLock );
	if (BuildStats.FailureType == EBuildPatchInstallError::PathLengthExceeded)
	{
		return false;
	}
	return !BuildStats.ProcessSuccess;
}

bool FBuildPatchInstaller::HasError()
{
	FScopeLock Lock( &ThreadLock );
	if (BuildStats.FailureType == EBuildPatchInstallError::UserCanceled)
	{
		return false;
	}
	return !BuildStats.ProcessSuccess;
}

EBuildPatchInstallError FBuildPatchInstaller::GetErrorType()
{
	FScopeLock Lock(&ThreadLock);
	return BuildStats.FailureType;
}

FString FBuildPatchInstaller::GetErrorCode()
{
	FScopeLock Lock(&ThreadLock);
	return BuildStats.ErrorCode;
}

//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
FText FBuildPatchInstaller::GetPercentageText()
{
	static const FText PleaseWait = LOCTEXT( "BuildPatchInstaller_GenericProgress", "Please Wait" );

	FScopeLock Lock( &ThreadLock );

	float Progress = GetUpdateProgress() * 100.0f;
	if( Progress <= 0.0f )
	{
		return PleaseWait;
	}

	FNumberFormattingOptions PercentFormattingOptions;
	PercentFormattingOptions.MaximumFractionalDigits = 0;
	PercentFormattingOptions.MinimumFractionalDigits = 0;

	return FText::AsPercent(GetUpdateProgress(), &PercentFormattingOptions);
}

FText FBuildPatchInstaller::GetStatusText()
{
	return BuildProgress.GetStateText();
}

float FBuildPatchInstaller::GetUpdateProgress()
{
	return BuildProgress.GetProgress();
}

FBuildInstallStats FBuildPatchInstaller::GetBuildStatistics()
{
	FScopeLock Lock( &ThreadLock );
	return BuildStats;
}

EBuildPatchDownloadHealth FBuildPatchInstaller::GetDownloadHealth() const
{
	FScopeLock Lock(&ThreadLock);
	return DownloadHealthValue;
}

void FBuildPatchInstaller::SetDownloadHealth(EBuildPatchDownloadHealth DownloadHealth)
{
	FScopeLock Lock(&ThreadLock);
	DownloadHealthValue = DownloadHealth;
}

FText FBuildPatchInstaller::GetErrorText()
{
	FScopeLock Lock( &ThreadLock );
	return BuildStats.FailureReasonText;
}

void FBuildPatchInstaller::CancelInstall()
{
	FBuildPatchInstallError::SetFatalError(EBuildPatchInstallError::UserCanceled, UserCancelErrorCodes::UserRequested);
	FBuildPatchHTTP::CancelAllHttpRequests();
	// Make sure we are not paused
	if( IsPaused() )
	{
		TogglePauseInstall();
	}
}

bool FBuildPatchInstaller::TogglePauseInstall()
{
	if( IsPaused() )
	{
		// We are now resuming, so record time paused for
		FScopeLock Lock( &ThreadLock );
		const double PausedForSec = FPlatformTime::Seconds() - TimePausedAt;
		BuildStats.ProcessPausedTime += PausedForSec;
	}
	else
	{
		// If there is an error, we don't allow the pause
		if( FBuildPatchInstallError::HasFatalError() )
		{
			return false;
		}
		// Set the time we pause at
		TimePausedAt = FPlatformTime::Seconds();
	}
	return BuildProgress.TogglePauseState();
}

void FBuildPatchInstaller::UpdateVerificationProgress( float Percent )
{
	BuildProgress.SetStateProgress( EBuildPatchProgress::BuildVerification, Percent );
}

void FBuildPatchInstaller::ExecuteCompleteDelegate()
{
	// Should be executed in main thread, and already be complete
	check( IsInGameThread() );
	check( IsComplete() );
	// Take opportunity to save configuration
	SaveLocalMachineConfig();
	// Call the complete delegate
	OnCompleteDelegate.ExecuteIfBound(bSuccess, NewBuildManifest);
}

void FBuildPatchInstaller::WaitForThread() const
{
	if (Thread != NULL)
	{
		Thread->WaitForCompletion();
	}
}

#undef LOCTEXT_NAMESPACE
