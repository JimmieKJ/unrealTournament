// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IBuildInstaller.h: Declares the IBuildInstaller interface.
=============================================================================*/

#pragma once

typedef TSharedPtr< class IBuildInstaller, ESPMode::ThreadSafe > IBuildInstallerPtr;
typedef TSharedRef< class IBuildInstaller, ESPMode::ThreadSafe > IBuildInstallerRef;

/**
 * A struct to hold stats for the build process.
 */
struct FBuildInstallStats
{
	// Constructor
	FBuildInstallStats()
		: NumFilesInBuild(0)
		, NumFilesOutdated(0)
		, NumFilesToRemove(0)
		, NumChunksRequired(0)
		, ChunksQueuedForDownload(0)
		, ChunksLocallyAvailable(0)
		, NumChunksDownloaded(0)
		, NumChunksRecycled(0)
		, NumChunksCacheBooted(0)
		, NumDriveCacheChunkLoads(0)
		, NumRecycleFailures(0)
		, NumDriveCacheLoadFailures(0)
		, TotalDownloadedData(0)
		, AverageDownloadSpeed(0.0)
		, TheoreticalDownloadTime(0.0f)
		, VerifyTime(0.0f)
		, CleanUpTime(0.0f)
		, ProcessExecuteTime(0.0f)
		, ProcessPausedTime(0.0f)
		, ProcessSuccess(false)
	{}

	// The name of the app being installed
	FString AppName;
	// The version string currently installed, or "NONE"
	FString AppInstalledVersion;
	// The version string patching to
	FString AppPatchVersion;
	// The cloud directory used for this install
	FString CloudDirectory;
	// The total number of files in the build
	uint32 NumFilesInBuild;
	// The total number of files outdated
	uint32 NumFilesOutdated;
	// The total number of files in the previous build that can be deleted
	uint32 NumFilesToRemove;
	// The total number of chunks making up those files
	uint32 NumChunksRequired;
	// The number of required chunks queued for download
	uint32 ChunksQueuedForDownload;
	// The number of chunks locally available in the build
	uint32 ChunksLocallyAvailable;
	// The total number of chunks that were downloaded
	uint32 NumChunksDownloaded;
	// The number of chunks successfully recycled
	uint32 NumChunksRecycled;
	// The number of chunks that had to be booted from the cache
	uint32 NumChunksCacheBooted;
	// The number of chunks that had to be loaded from the drive cache
	uint32 NumDriveCacheChunkLoads;
	// The number of chunks that failed to be recycled from existing build
	uint32 NumRecycleFailures;
	// The number of chunks that failed to load from the drive cache
	uint32 NumDriveCacheLoadFailures;
	// The total number of bytes downloaded
	int64 TotalDownloadedData;
	// The average chunk download speed
	double AverageDownloadSpeed;
	// The theoretical download time (data/speed)
	float TheoreticalDownloadTime;
	// The total time that the install verification took to complete
	float VerifyTime;
	// The total time that the install cleanup took to complete
	float CleanUpTime;
	// The total time that the install process took to complete
	float ProcessExecuteTime;
	// The amount of time that was spent paused
	float ProcessPausedTime;
	// Whether the process was successful
	bool ProcessSuccess;
	// The reason for failure, or "SUCCESS"
	FString FailureReason;
	// The localised, more generic failure reason
	FText FailureReasonText;
};

/**
 * Interface to a Build Manifest.
 */
class IBuildInstaller
{
public:
	/**
	 * Virtual destructor.
	 */
	virtual ~IBuildInstaller() { }

	/**
	 * Get whether the install has complete
	 * @return	true if the thread completed
	 */
	virtual bool IsComplete() = 0;

	/**
	 * Get whether the install was canceled. Only valid if complete.
	 * @return	true if installation was canceled
	 */
	virtual bool IsCanceled() = 0;

	/**
	 * Get whether the install is currently paused.
	 * @return	true if installation is paused
	 */
	virtual bool IsPaused() = 0;

	/**
	 * Get whether the install can be resumed.
	 * @return	true if installation is resumable
	 */
	virtual bool IsResumable() = 0;

	/**
	 * Get whether the install failed. Only valid if complete.
	 * @return	true if installation was a failure
	 */
	virtual bool HasError() = 0;

	/**
	 * This is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	 *
	 * Get the percentage complete text for the current process
	 * @return	percentage complete text progress
	 */
	virtual FText GetPercentageText() = 0;

	/**
	 * This is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	 *
	 * Get the download speed text for the current process
	 * @return	download speed text progress
	 */
	virtual FText GetDownloadSpeedText()  = 0;

	/**
	 * Get the download speed for the current process
	 * @return	download speed progress
	 */
	virtual double GetDownloadSpeed() const  = 0;

	/**
	 * Get the initial download size
	 * @return	the initial download size
	 */
	virtual int64 GetInitialDownloadSize() const  = 0;

	/**
	* Get the total currently downloaded
	* @return	the total currently downloaded
	 */
	virtual int64 GetTotalDownloaded() const = 0;

	/**
	 * Get the text for status of the install process
	 * @param ShortError		The truncated version of the error
	 * @return	status of the install process text
	 */
	virtual FText GetStatusText(bool ShortError = false) = 0;

	/**
	 * Get the update progress
	 * @return	A float describing progress: Between 0 and 1 for known progress, or less than 0 for unknown progress.
	 */
	virtual float GetUpdateProgress() = 0;

	/**
	 * Get the build stats for the process. This should only be called after the install has completed
	 * @return	A struct containing information about the build
	 */
	virtual FBuildInstallStats GetBuildStatistics() = 0;

	/**
	 * Get the display text for the error that occurred. Only valid to call after completion
	 * @return	display error text
	 */
	virtual FText GetErrorText() = 0;

	/**
	 * Cancel the current install
	 */
	virtual void CancelInstall() = 0;

	/**
	 * Toggle the install paused state
	 * @return true if the installer is now paused
	 */
	virtual bool TogglePauseInstall() = 0;
};

