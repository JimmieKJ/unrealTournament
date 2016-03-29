// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoReimportUtilities.h"
#include "FileCache.h"
#include "MessageLog.h"

class FReimportFeedbackContext;

/** Class responsible for watching a specific content directory for changes */
class FContentDirectoryMonitor
{
public:

	/**
	 * Constructor.
	 * @param InDirectory			Content directory path to monitor. Assumed to be absolute.
	 * @param InMatchRules			Rules to select what will apply to this folder
	 * @param InMountedContentPath	(optional) Mounted content path (eg /Engine/, /Game/) to which InDirectory maps.
	 */
	FContentDirectoryMonitor(const FString& InDirectory, const DirectoryWatcher::FMatchRules& InMatchRules, const FString& InMountedContentPath = FString());

	/** Tick this monitor's cache to give it a chance to finish scanning for files */
	void Tick();

	/** Start processing any outstanding changes this monitor is aware of */
	int32 StartProcessing();

	/** Extract the files we need to import from our outstanding changes (happens first)*/ 
	void ProcessAdditions(const IAssetRegistry& Registry, const DirectoryWatcher::FTimeLimit& TimeLimit, TArray<UPackage*>& OutPackagesToSave, const TMap<FString, TArray<UFactory*>>& InFactoriesByExtension, FReimportFeedbackContext& Context);

	/** Process the outstanding changes that we have cached */
	void ProcessModifications(const IAssetRegistry& Registry, const DirectoryWatcher::FTimeLimit& TimeLimit, TArray<UPackage*>& OutPackagesToSave, FReimportFeedbackContext& Context);

	/** Extract the assets we need to delete from our outstanding changes (happens last) */ 
	void ExtractAssetsToDelete(const IAssetRegistry& Registry, TArray<FAssetData>& OutAssetsToDelete);

	/** Abort the current processing operation */
	void Abort();

	/** Report an external change to the manager, such that a subsequent equal change reported by the os be ignored */
	void IgnoreNewFile(const FString& Filename);
	void IgnoreFileModification(const FString& Filename);
	void IgnoreMovedFile(const FString& SrcFilename, const FString& DstFilename);
	void IgnoreDeletedFile(const FString& Filename);

	/** Destroy this monitor including its cache */
	void Destroy();

	/** Get the directory that this monitor applies to */
	const FString& GetDirectory() const { return Cache.GetDirectory(); }

	/** Get the content mount point that this monitor applies to */
	const FString& GetMountPoint() const { return MountedContentPath; }

public:

	/** Get the number of outstanding changes that we potentially have to process (when not already processing) */
	int32 GetNumUnprocessedChanges() const { return Cache.GetNumOutstandingChanges(); }

private:

	/** Import a new asset into the specified package path, from the specified file */ 
	void ImportAsset(const FString& PackagePath, const FString& Filename, TArray<UPackage*>& OutPackagesToSave, const TMap<FString, TArray<UFactory*>>& InFactoriesByExtension, FReimportFeedbackContext& Context);

	/** Reimport a specific asset, provided its source content differs from the specified file hash */
	void ReimportAsset(UObject* InAsset, const FString& FullFilename, const FMD5Hash& NewFileHash, TArray<UPackage*>& OutPackagesToSave, FReimportFeedbackContext& Context);

	/** Set the specified asset to import from the specified file, then attempt to reimport it */
	void ReimportAssetWithNewSource(UObject* InAsset, const FString& FullFilename, const FMD5Hash& NewFileHash, TArray<UPackage*>& OutPackagesToSave, FReimportFeedbackContext& Context);

private:

	/** The file cache that monitors and reflects the content directory. */
	DirectoryWatcher::FFileCache Cache;

	/** The mounted content path for this monitor (eg /Game/) */
	FString MountedContentPath;

	/** A list of file system changes that are due to be processed */
	TArray<DirectoryWatcher::FUpdateCacheTransaction> AddedFiles, ModifiedFiles, DeletedFiles;

	/** The last time we attempted to save the cache file */
	double LastSaveTime;

	/** The interval between potential re-saves of the cache file */
	static const int32 ResaveIntervalS = 60;
};