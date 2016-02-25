// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineTitleFileInterface.h"
#include "OnlineHotfixManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHotfixManager, Verbose, All);

UENUM()
enum class EHotfixResult : uint8
{
	/** Failed to apply the hotfix */
	Failed,
	/** Hotfix succeeded and is ready to go */
	Success,
	/** Hotfix process succeeded but there were no changes applied */
	SuccessNoChange,
	/** Hotfix succeeded and requires the current level to be reloaded to take effect */
	SuccessNeedsReload,
	/** Hotfix succeeded and requires the process restarted to take effect */
	SuccessNeedsRelaunch
};

/**
 * Delegate fired when the list of files has been returned from the network store
 *
 * @param bWasSuccessful whether the file list was successful or not
 *
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHotfixComplete, EHotfixResult);
typedef FOnHotfixComplete::FDelegate FOnHotfixCompleteDelegate;

/**
 * This class manages the downloading and application of hotfix data
 * Hotfix data is a set of non-executable files downloaded and applied to the game.
 * The base implementation knows how to handle INI, PAK, and locres files.
 * NOTE: Each INI/PAK file must be prefixed by the platform name they are targeted at
 */
UCLASS(Config=Engine)
class HOTFIX_API UOnlineHotfixManager :
	public UObject
{
	GENERATED_BODY()

protected:
	/** The online interface to use for downloading the hotfix files */
	IOnlineTitleFilePtr OnlineTitleFile;

	/** Callbacks for when the title file interface is done */
	FOnEnumerateFilesCompleteDelegate OnEnumerateFilesCompleteDelegate;
	FOnReadFileCompleteDelegate OnReadFileCompleteDelegate;
	FDelegateHandle OnEnumerateFilesCompleteDelegateHandle;
	FDelegateHandle OnReadFileCompleteDelegateHandle;

	/**
	 * Delegate fired when the hotfix process has completed
	 *
	 * @param status of the hotfix process
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnHotfixComplete, EHotfixResult);

	/** Holds which files are pending download */
	TSet<FString> PendingHotfixFiles;
	/** The filtered list of files that are part of the hotfix */
	TArray<FCloudFileHeader> HotfixFileList;
	/** The last set of hotfix files that was applied so we can determine whether we are up to date or not */
	TArray<FCloudFileHeader> LastHotfixFileList;
	/** Holds which files have been mounted for unmounting */
	TArray<FString> MountedPakFiles;
	/** Holds the game localization resource name that we should search for */
	FString GameLocName;
	/** Used to match any PAK files for this platform */
	FString PlatformPrefix;

	void Init();
	void Cleanup();
	/** Looks at each file returned via the hotfix and processes them */
	void ApplyHotfix();
	/** Cleans up and fires the delegate indicating it's done */
	void TriggerHotfixComplete(EHotfixResult HotfixResult);
	/** Checks each file listed to see if it is a hotfix file to process */
	void FilterHotfixFiles();
	/** Starts the async reading process for the hotfix files */
	void ReadHotfixFiles();
	/** Unmounts any PAK files so they can be re-mounted after downloading */
	void UnmountHotfixFiles();

	/** Called once the list of hotfix files has been retrieved */
	void OnEnumerateFilesComplete(bool bWasSuccessful);
	/** Called as files are downloaded to determine when to apply the hotfix data */
	void OnReadFileComplete(bool bWasSuccessful, const FString& FileName);

	/** @return the config file entry for the ini file name in question */
	FConfigFile* GetConfigFile(const FString& IniName);

	/** @return the human readable name of the file */
	const FString GetFriendlyNameFromDLName(const FString& DLName) const;

protected:
	/**
	 * Override this method to look at the file information for any game specific hotfix processing
	 * NOTE: Make sure to call Super to get default handling of files
	 *
	 * @param FileHeader - the information about the file to determine if it needs custom processing
	 *
	 * @return true if the file needs some kind of processing, false to have hotfixing ignore the file
	 */
	virtual bool WantsHotfixProcessing(const FCloudFileHeader& FileHeader);
	/**
	 * Called when a file needs custom processing (see above). Override this to provide your own processing methods
	 *
	 * @param FileHeader - the header information for the file in question
	 *
	 * @return whether the file was successfully processed
	 */
	virtual bool ApplyHotfixProcessing(const FCloudFileHeader& FileHeader);
	/**
	 * Override this to change the default INI file handling (merge delta INI changes into the config cache)
	 *
	 * @param FileName - the name of the INI being merged into the config cache
	 * @param IniData - the contents of the INI file (expected to be delta contents not a whole file)
	 *
	 * @return whether the merging was successful or not
	 */
	virtual bool HotfixIniFile(const FString& FileName, const FString& IniData);
	/**
	 * Override this to change the default loc file handling:
	 *
	 * @param FileName - the name of the loc file being merged into the loc manager
	 *
	 * @return whether the mounting of the PAK file was successful or not
	 */
	virtual void HotfixLocFile(const FCloudFileHeader& FileHeader);
	/**
	 * Override this to change the default PAK file handling:
	 *		- mount PAK file immediately
	 *		- Scan for any INI files contained within the PAK file and merge those in
	 *
	 * @param FileName - the name of the PAK file being mounted
	 *
	 * @return whether the mounting of the PAK file was successful or not
	 */
	virtual bool HotfixPakFile(const FCloudFileHeader& FileHeader);
	/**
	 * Override this to change the default INI file handling (merge whole INI files into the config cache)
	 *
	 * @param FileName - the name of the INI being merged into the config cache
	 *
	 * @return whether the merging was successful or not
	 */
	virtual bool HotfixPakIniFile(const FString& FileName);

	/**
	 * Override this to change the default caching directory
	 *
	 * @return the default caching directory
	 */
	virtual FString GetCachedDirectory()
	{
		return FPaths::GamePersistentDownloadDir();
	}

public:
	UOnlineHotfixManager();

	/** Tells the hotfix manager which OSS to use. Uses the default if empty */
	UPROPERTY(Config)
	FString OSSName;

	/** Tells the factory method which class to contruct */
	UPROPERTY(Config)
	FString HotfixManagerClassName;

	/** Starts the fetching of hotfix data from the OnlineTitleFileInterface that is registered for this game */
	UFUNCTION(BlueprintCallable, Category="Hotfix")
	void StartHotfixProcess();

	/** Factory method that returns the configured hotfix manager */
	static UOnlineHotfixManager* Get(UWorld* World);
};
