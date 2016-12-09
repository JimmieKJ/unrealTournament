// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchInstaller.h: Declares the FBuildPatchInstaller class which
	controls the process of installing a build described by a build manifest.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "BuildPatchManifest.h"
#include "Interfaces/IBuildInstaller.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "HAL/ThreadSafeBool.h"
#include "BuildPatchProgress.h"

class FBuildPatchInstallationInfo;
class FBuildPatchInstaller;

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

	// The installer configuration
	FInstallerConfiguration Configuration;

	// The manifest for the build we have installed (if applicable)
	FBuildPatchAppManifestPtr CurrentBuildManifest;

	// The manifest for the build we want to install
	FBuildPatchAppManifestRef NewBuildManifest;

	// The directory created in staging, to store local patch data
	FString DataStagingDir;

	// The directory created in staging to construct install files to
	FString InstallStagingDir;

	// The filename used to mark a previous install that did not complete but moved staged files into the install directory
	FString PreviousMoveMarker;

	// The filename for the local machine config. This is used for per-machine values rather than per-user or shipped config.
	const FString LocalMachineConfigFile;

	// A critical section to protect variables
	mutable FCriticalSection ThreadLock;

	// A flag to store if we are installing file data
	const bool bIsFileData;

	// A flag to store if we are installing chunk data (to help readability)
	const bool bIsChunkData;

	// A flag storing whether the process was a success
	FThreadSafeBool bSuccess;

	// A flag marking that we a running
	FThreadSafeBool bIsRunning;

	// A flag marking that we initialized correctly
	FThreadSafeBool bIsInited;

	// A flag that stores whether we are on the first install iteration
	FThreadSafeBool bFirstInstallIteration;

	// The download speed value
	double DownloadSpeedValue;

	// The current download health value
	EBuildPatchDownloadHealth DownloadHealthValue;

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
	TArray<FString> FilesInstalled;

	// Holds the files which are all required
	TSet<FString> TaggedFiles;

	// The files that the installation process required to construct
	TSet<FString> FilesToConstruct;

	// The list of prerequisites that have already been installed. Will also be updated on successful installation
	TSet<FString> InstalledPrereqs;

	// Reference to the module's installation info
	FBuildPatchInstallationInfo& InstallationInfo;

public:
	/**
	 * Constructor takes configuration and dependencies.
	 * @param Configuration             The installer configuration structure.
	 * @param InstallationInfoRef       Reference to the module's installation info that keeps record of locally installed apps for use as chunk sources.
	 * @param InLocalMachineConfigFile  Filename for the local machine's config. This is used for per-machine configuration rather than shipped or user config.
	 * @param OnCompleteDelegate        Delegate for when the process has completed.
	 */
	FBuildPatchInstaller(FInstallerConfiguration Configuration, FBuildPatchInstallationInfo& InstallationInfoRef, const FString& InLocalMachineConfigFile, FBuildPatchBoolManifestDelegate OnCompleteDelegate);

	/**
	 * Default Destructor, will delete the allocated Thread
	 */
	~FBuildPatchInstaller();

	// Begin FRunnable
	uint32 Run() override;
	// End FRunnable

	// Begin IBuildInstaller
	virtual bool IsComplete() const override;
	virtual bool IsCanceled() const override;
	virtual bool IsPaused() const override;
	virtual bool IsResumable() const override;
	virtual bool HasError() const override;
	virtual EBuildPatchInstallError GetErrorType() const override;
	virtual FString GetErrorCode() const override;
	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	virtual FText GetPercentageText() const override;
	//@todo this is deprecated and shouldn't be used anymore [6/4/2014 justin.sargent]
	virtual FText GetDownloadSpeedText() const override;
	virtual double GetDownloadSpeed() const override;
	virtual int64 GetInitialDownloadSize() const override;
	virtual int64 GetTotalDownloaded() const override;
	virtual EBuildPatchState GetState() const override;
	virtual FText GetStatusText() const override;
	virtual float GetUpdateProgress() const override;
	virtual FBuildInstallStats GetBuildStatistics() const override;
	virtual EBuildPatchDownloadHealth GetDownloadHealth() const override;
	virtual FText GetErrorText() const override;
	virtual void CancelInstall() override;
	virtual bool TogglePauseInstall() override;
	// End IBuildInstaller interface

	/**
	 * Begin the installation process
	 * @return true if the installation started successfully, or is already running
	 */
	bool StartInstallation();

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
	 * Initialise the installer.
	 * @return Whether initialization was successful.
	 */
	bool Initialize();

	/**
	 * Checks the installation directory for any already existing files of the correct size, with may account for manual
	 * installation. Should be used for new installation detecting existing files.
	 * NB: Not useful for patches, where we'd expect existing files anyway.
	 * @return    Returns true if there were potentially already installed files
	 */
	bool CheckForExternallyInstalledFiles();

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
	 * Sets the current download health
	 * @param DownloadHealth	The download health
	 */
	void SetDownloadHealth(EBuildPatchDownloadHealth DownloadHealth);

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

	/**
	 * Loads the configuration values for this computer, call from main thread.
	 */
	void LoadLocalMachineConfig();

	/**
	 * Saves updated configuration values for this computer, call from main thread.
	 */
	void SaveLocalMachineConfig();
};
