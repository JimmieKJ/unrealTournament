// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchServicesModule.h: Declares the FBuildPatchServicesModule class.
=============================================================================*/

#pragma once

/**
 * Constant values and typedefs
 */
enum
{
	// Sizes
	FileBufferSize		= 1024*1024*4,		// When reading from files, how much to buffer
	StreamBufferSize	= FileBufferSize*4,	// When reading from build data stream, how much to buffer.
};

/**
 * Implements a class that stores information about installed builds that are registered with the module
 */
class FBuildPatchInstallationInfo
{
private:
	// Stores a list of installations available on this machine
	TMap<FString, FBuildPatchAppManifestPtr> AvailableInstallations;

	// Stores the information for which chunks can be gathered from which installations
	TMap<FGuid, FBuildPatchAppManifestPtr> RecyclableChunks;

public:
	/**
	 * Registers an installation on this machine. This information is used to gather a list of install locations that can be used as chunk sources.
	 * @param AppManifest			Ref to the manifest for this installation
	 * @param AppInstallDirectory	The install location
	 */
	void RegisterAppInstallation(IBuildManifestRef AppManifest, const FString& AppInstallDirectory);

	/**
	 * Populates an array of chunks that should be producible from the list of installations on the machine.
	 * @param ChunksRequired	IN		A list of chunks that are needed.
	 * @param ChunksAvailable	OUT		A list to receive the chunks that could be constructed locally.
	 */
	void EnumerateProducibleChunks(const TArray< FGuid >& ChunksRequired, TArray< FGuid >& ChunksAvailable);

	/**
	 * Get the manifest for the installation that a given chunk can be recycled from.
	 * @param ChunkGuid		The GUID for the chunk required
	 * @return Shared pointer to the installed manifest that contains this chunk
	 */
	FBuildPatchAppManifestPtr GetManifestContainingChunk(const FGuid& ChunkGuid);

	/**
	 * Get the installation directory for a given manifest
	 * @param AppManifest	The manifest that has been registered with this system
	 * @return The installation directory
	 */
	const FString GetManifestInstallDir(FBuildPatchAppManifestPtr AppManifest);
};

/**
 * Implements the BuildPatchServicesModule.
 */
class FBuildPatchServicesModule
	: public IBuildPatchServicesModule
{
public:

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	//~ Begin IBuildPatchServicesModule Interface
	virtual IBuildManifestPtr LoadManifestFromFile(const FString& Filename) override;
	virtual IBuildManifestPtr MakeManifestFromData(const TArray<uint8>& ManifestData) override;
	virtual bool SaveManifestToFile(const FString& Filename, IBuildManifestRef Manifest, bool bUseBinary = true) override;
	virtual IBuildInstallerPtr StartBuildInstall(IBuildManifestPtr CurrentManifest, IBuildManifestPtr InstallManifest, const FString& InstallDirectory, FBuildPatchBoolManifestDelegate OnCompleteDelegate, bool bIsRepair = false, TSet<FString> InstallTags = TSet<FString>()) override;
	virtual IBuildInstallerPtr StartBuildInstallStageOnly(IBuildManifestPtr CurrentManifest, IBuildManifestPtr InstallManifest, const FString& InstallDirectory, FBuildPatchBoolManifestDelegate OnCompleteDelegate, bool bIsRepair = false, TSet<FString> InstallTags = TSet<FString>()) override;
	virtual void SetStagingDirectory(const FString& StagingDir) override;
	virtual void SetCloudDirectory(FString CloudDir) override;
	virtual void SetCloudDirectories(TArray<FString> CloudDirs) override;
	virtual void SetBackupDirectory(const FString& BackupDir) override;
	virtual void SetAnalyticsProvider(TSharedPtr< IAnalyticsProvider > AnalyticsProvider) override;
	virtual void SetHttpTracker(TSharedPtr< FHttpServiceTracker > HttpTracker) override;
	virtual void RegisterAppInstallation(IBuildManifestRef AppManifest, const FString AppInstallDirectory) override;
	virtual void CancelAllInstallers(bool WaitForThreads) override;
	virtual bool GenerateChunksManifestFromDirectory(const FBuildPatchSettings& Settings) override;
	virtual bool CompactifyCloudDirectory(float DataAgeThreshold, ECompactifyMode::Type Mode) override;
	virtual bool EnumerateManifestData(const FString& ManifestFilePath, const FString& OutputFile, bool bIncludeSizes) override;
	virtual bool MergeManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& ManifestFilePathC, const FString& NewVersionString, const FString& SelectionDetailFilePath) override;
	virtual IBuildManifestPtr MakeManifestFromJSON(const FString& ManifestJSON) override;
	//~ End IBuildPatchServicesModule Interface

	/**
	 * Gets the directory used for staging intermediate files.
	 * @return	The staging directory
	 */
	static const FString& GetStagingDirectory();

	/**
	 * Gets the cloud directory where chunks and manifests will be pulled from.
	 * @param CloudIdx    Optional override for which cloud directory to get. This value will wrap within the range of available cloud directories.
	 * @return	The cloud directory
	 */
	static FString GetCloudDirectory(int32 CloudIdx = 0);

	/**
	 * Gets the cloud directories where chunks and manifests will be pulled from.
	 * @return	The cloud directories
	 */
	static TArray<FString> GetCloudDirectories();

	/**
	 * Gets the backup directory for saving files clobbered by repair/patch.
	 * @return	The backup directory
	 */
	static const FString& GetBackupDirectory();

private:
	/**
	 * Tick function to monitor installers for completion, so that we can call delegates on the main thread
	 * @param		Delta	Time since last tick
	 * @return	Whether to continue ticking
	 */
	bool Tick(float Delta);

	/**
	 * This will get called when core PreExits. Make sure any running installers are canceled out.
	 */
	void PreExit();

	/**
	 * Called during init to perform any fix up required to new configuration.
	 */
	void FixupLegacyConfig();

	/**
	 * Helper function to normalize the provided directory list.
	 */
	void NormalizeCloudPaths(TArray<FString>& InOutCloudPaths);

private:
	// Holds the cloud directories where chunks should belong
	static TArray<FString> CloudDirectories;

	// Holds the staging directory where we can perform any temporary work
	static FString StagingDirectory;

	// Holds the backup directory where we can move files that will be clobbered by repair or patch
	static FString BackupDirectory;

	// Holds the filename for local machine config. This is instead of shipped or user config, to track machine installation config.
	FString LocalMachineConfigFile;

	// A flag specifying whether prerequisites install should be skipped
	bool bForceSkipPrereqs;

	// Save the ptr to the build installer thread that we create
	TArray< FBuildPatchInstallerPtr > BuildPatchInstallers;

	// The system that holds available installations used for recycling install data
	FBuildPatchInstallationInfo InstallationInfo;

	// Handle to the registered Tick delegate
	FDelegateHandle TickDelegateHandle;
};
