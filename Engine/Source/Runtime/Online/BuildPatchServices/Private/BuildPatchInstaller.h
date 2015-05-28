// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchInstaller.h: Declares the FBuildPatchInstaller class which
	controls the process of installing a build described by a build manifest.
=============================================================================*/

typedef TSharedPtr< class FBuildPatchInstaller, ESPMode::ThreadSafe > FBuildPatchInstallerPtr;
typedef TSharedRef< class FBuildPatchInstaller, ESPMode::ThreadSafe > FBuildPatchInstallerRef;
typedef TWeakPtr< class FBuildPatchInstaller, ESPMode::ThreadSafe > FBuildPatchInstallerWeakPtr;

/**
 * FBuildPatchInstaller
 * This class controls a thread that wraps the code to install/patch an app from manifests
 */
class FBuildPatchInstaller
	: public IBuildInstaller
	, public FRunnable
{
private:
	// Hold a pointer to my thread for easier deleting
	FRunnableThread* Thread;

	// The delegates that we will be calling
	FBuildPatchBoolManifestDelegate OnCompleteDelegate;

	// The manifest for the build we have installed (if applicable)
	FBuildPatchAppManifestPtr CurrentBuildManifest;

	// The manifest for the build we want to install
	FBuildPatchAppManifestRef NewBuildManifest;

	// The Destination directory that we want to install to
	FString InstallDirectory;

	// The Staging directory used for construction
	FString StagingDirectory;

	// The directory created in staging, to store local patch data
	FString DataStagingDir;

	// The directory created in staging to construct install files to
	FString InstallStagingDir;

	// The filename used to mark a previous install that did not complete but moved staged files into the install directory
	FString PreviousMoveMarker;

	// A critical section to protect variables
	mutable FCriticalSection ThreadLock;

	// A flag to store if we are installing file data
	const bool bIsFileData;

	// A flag to store if we are installing chunk data (to help readability)
	const bool bIsChunkData;

	// A flag to store if we are performing a repair
	bool bIsRepairing;

	// A flag to store if we should only be staging, and not moving to install location
	bool bShouldStageOnly;

	// A flag storing whether the process was a success
	bool bSuccess;

	// A flag marking that we a running
	bool bIsRunning;

	// A flag marking that we initialized correctly
	bool bIsInited;

	// The download speed value
	double DownloadSpeedValue;

	// The number of bytes left to download
	int64 DownloadBytesLeft;

	// Keep track of build stats
	FBuildInstallStats BuildStats;

	// Keep track of install progress
	FBuildPatchProgress BuildProgress;

	// Cache the float value for number of chunk downloads for progress calculations
	float InitialNumChunkDownloads;

	// Cache the float value for number of chunk constructions for progress calculations
	float InitialNumChunkConstructions;

	// Cache the number of bytes required to download
	int64 TotalInitialDownloadSize;

	// Track the time the installer was paused so we can record pause time
	double TimePausedAt;

	// Holds a list of files that have been placed into the install directory
	TArray< FString > FilesInstalled;

	// Referecne to the module's installation info
	FBuildPatchInstallationInfo& InstallationInfo;

public:
	/**
	 * Constructor takes the required arguments, CurrentManifest can be invalid for fresh install.
	 * @param OnCompleteDelegate	Delegate for when the process has completed
	 * @param CurrentManifest		The manifest for the currently installed build. Can be invalid if fresh install.
	 * @param InstallManifest		The manifest for the build to be installed
	 * @param InstallDirectory		The directory where the build will be installed
	 * @param StagingDirectory		The directory for storing the intermediate files
	 * @param InstallationInfoRef	Reference to the module's installation info that keeps record of locally installed apps for use as chunk sources
	 * @param ShouldStageOnly		Whether the installer should only stage the required files, and skip moving them to the install directory
	 */
	FBuildPatchInstaller(FBuildPatchBoolManifestDelegate OnCompleteDelegate, FBuildPatchAppManifestPtr CurrentManifest, FBuildPatchAppManifestRef InstallManifest, const FString& InstallDirectory, const FString& StagingDirectory, FBuildPatchInstallationInfo& InstallationInfoRef, bool ShouldStageOnly);

	/**
	 * Default Destructor, will delete the allocated Thread
	 */
	~FBuildPatchInstaller();

	// Begin FRunnable
	bool Init() override;
	uint32 Run() override;
	// End FRunnable

	// Begin IBuildInstaller
	virtual bool IsComplete() override;
	virtual bool IsCanceled() override;
	virtual bool IsPaused() override;
	virtual bool HasError() override;
	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	virtual FText GetPercentageText() override;
	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	virtual FText GetDownloadSpeedText() override;
	virtual double GetDownloadSpeed() const override;
	virtual int64 GetInitialDownloadSize() const override;
	virtual int64 GetTotalDownloaded() const override;
	virtual FText GetStatusText() override;
	virtual float GetUpdateProgress() override;
	virtual FBuildInstallStats GetBuildStatistics() override;
	virtual FText GetErrorText() override;
	virtual void CancelInstall() override;
	virtual bool TogglePauseInstall() override;
	// End IBuildInstaller interface

	/**
	 * Executes the on complete delegate. This should only be called when completed, and is separated out
	 * to allow control to make this call on the main thread.
	 */
	void ExecuteCompleteDelegate();

	/**
	 * Only returns once the thread has finished running
	 */
	void WaitForThread() const;

private:

	/**
	 * Runs the installation process
	 * @param	CorruptFiles	A list of files that were corrupt, to only install those.
	 * @return    Returns true if there were no errors blocking installation
	 */
	bool RunInstallation(TArray<FString>& CorruptFiles);

	/**
	 * Runs the backup process for locally changed files, and then moves new files into installation directory
	 * @return    Returns true if there were no errors
	 */
	bool RunBackupAndMove();

	/**
	 * Runs the process to setup all file attributes required
	 * @param bForce		Set true if also removing attributes to force the api calls to be made
	 * @return    Returns true if there were no errors
	 */
	bool RunFileAttributes( bool bForce = false );

	/**
	 * Runs the verification process
	 * @param CorruptFiles  OUT     Receives the list of files that failed verification
	 * @return    Returns true if there were no corrupt files
	 */
	bool RunVerification( TArray< FString >& CorruptFiles );

	/**
	 * Checks a particular file in the install directory to see if it needs backing up, and does so if necessary
	 * @param Filename                      The filename to check, which should match a filename in a manifest
	 * @param bDiscoveredByVerification     Optional, whether the file was detected changed already by verification stage. Default: false.
	 * @return    Returns true if there were no errors
	 */
	bool BackupFileIfNecessary( const FString& Filename, bool bDiscoveredByVerification = false );

	/**
	 * Runs any prerequisites associated with the installation
	 * @return    Returns true if the prerequisites installer succeeded, false otherwise
	 */
	bool RunPrereqInstaller();
	
	/**
	 * Sets the bIsRunning flag
	 * @param bRunning	Whether the thread is running
	 */
	void SetRunning( bool bRunning );

	/**
	 * Sets the bIsInited flag
	 * @param bInited	Whether the thread successfully initialized
	 */
	void SetInited( bool bInited );

	/**
	 * Sets the current byte download speed
	 * @param ByteSpeed	The download speed
	 */
	void SetDownloadSpeed( double ByteSpeed );

	/**
	 * Sets the bytes left to download
	 * @param BytesLeft	The number of bytes left
	 */
	void SetDownloadBytesLeft( const int64& BytesLeft );

	/**
	 * Helper to calculate new chunk progress values
	 * @param bReset	Resets internals without updating
	 */
	void UpdateDownloadProgressInfo( bool bReset = false );

	/**
	 * Callback for verification process to set progress
	 * @param Percent	The current process percentage
	 */
	void UpdateVerificationProgress( float Percent );

	/**
	 * Delete empty directories from an installation
	 * @param RootDirectory	 Root Directory for search
	 */
	void CleanupEmptyDirectories( const FString& RootDirectory );
};
