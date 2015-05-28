// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoReimportUtilities.h"
#include "FileCache.h"
#include "MessageLog.h"

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
	FContentDirectoryMonitor(const FString& InDirectory, const FMatchRules& InMatchRules, const FString& InMountedContentPath = FString());

	/** Tick this monitor's cache to give it a chance to finish scanning for files */
	void Tick();

	/** Start processing any outstanding changes this monitor is aware of */
	void StartProcessing();

	/** Extract the files we need to import from our outstanding changes (happens first)*/ 
	void ProcessAdditions(const IAssetRegistry& Registry, const FTimeLimit& TimeLimit, TArray<UPackage*>& OutPackagesToSave, const TMap<FString, TArray<UFactory*>>& InFactoriesByExtension, class FReimportFeedbackContext& Context);

	/** Process the outstanding changes that we have cached */
	void ProcessModifications(const IAssetRegistry& Registry, const FTimeLimit& TimeLimit, TArray<UPackage*>& OutPackagesToSave, class FReimportFeedbackContext& Context);

	/** Extract the assets we need to delete from our outstanding changes (happens last) */ 
	void ExtractAssetsToDelete(const IAssetRegistry& Registry, TArray<FAssetData>& OutAssetsToDelete);

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

	/** Get the total amount of work this monitor has to perform in the current processing operation */
	int32 GetTotalWork() const { return TotalWork; }

	/** Get the total amount of work this monitor has performed in the current processing operation */
	int32 GetWorkProgress() const { return WorkProgress; }

private:

	/** The file cache that monitors and reflects the content directory. */
	FFileCache Cache;

	/** The mounted content path for this monitor (eg /Game/) */
	FString MountedContentPath;

	/** A list of file system changes that are due to be processed */
	TArray<FUpdateCacheTransaction> AddedFiles, ModifiedFiles, DeletedFiles;

	/** The number of changes we've processed out of the OutstandingChanges array */
	int32 TotalWork, WorkProgress;

	/** The last time we attempted to save the cache file */
	double LastSaveTime;

	/** The interval between potential re-saves of the cache file */
	static const int32 ResaveIntervalS = 60;
};