// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"
#include "P4DataCache.h"
#include "LabelNameProvider.h"

/**
 * Class to store date needed for sync monitoring thread.
 */
class FSyncingThread;

struct FSyncSettings
{
	/* Artist sync? */
	bool bArtist;

	/* Preview sync? */
	bool bPreview;

	/* Auto-clobber sync? */
	bool bAutoClobber;

	/* Automatically delete stale binaries? */
	bool bDeleteStaleBinaries;

	/* Override sync step. If set then it overrides collecting sync steps and uses this one instead. */
	FString OverrideSyncStep;

	FSyncSettings(bool bInArtist, bool bInPreview, bool bInAutoClobber, bool bInDeleteStaleBinaries, FString InOverrideSyncStep = FString())
		: bArtist(bInArtist)
		, bPreview(bInPreview)
		, bAutoClobber(bInAutoClobber)
		, bDeleteStaleBinaries(bInDeleteStaleBinaries)
		, OverrideSyncStep(MoveTemp(InOverrideSyncStep))
	{ }

	FSyncSettings(const FSyncSettings&& Other)
		: bArtist(Other.bArtist)
		, bPreview(Other.bPreview)
		, bAutoClobber(Other.bAutoClobber)
		, bDeleteStaleBinaries(Other.bDeleteStaleBinaries)
		, OverrideSyncStep(MoveTemp(Other.OverrideSyncStep))
	{ }
};

/**
 * Helper class with functions used to sync engine.
 */
class FUnrealSync
{
public:
	/* On sync log chunk read event delegate. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnSyncProgress, const FString&);

	/**
	 * Tells if UnrealSync was run with -Debug param.
	 */
	static bool IsDebugParameterSet();

	/**
	 * This method copies UnrealSync to temp location and run it from there,
	 * and updates (if needed).
	 *
	 * @param CommandLine Command line.
	 *
	 * @returns True if initialization phase is over and UnrealSync can build-up GUI. False otherwise.
	 */
	static bool Initialization(const TCHAR* CommandLine);

	/**
	 * Gets shared promotable display name.
	 *
	 * @returns Shared promotable display name.
	 */
	static const FString& GetSharedPromotableDisplayName();

	/**
	 * Returns P4 folder name for shared promotable.
	 */
	static const FString& GetSharedPromotableP4FolderName();

	/**
	 * Runs detached UnrealSync process and passes given parameters in command line.
	 *
	 * @param USPath UnrealSync executable path.
	 * @param bDoNotRunFromCopy Should UnrealSync be called with -DoNotRunFromCopy flag?
	 * @param bDoNotUpdateOnStartUp Should UnrealSync be called with -DoNotUpdateOnStartUp flag?
	 * @param bPassP4Env Pass P4 environment parameters to UnrealSync?
	 *
	 * @returns True if succeeded. False otherwise. Notice that this says of
	 *			success of launching procedure not launched process, cause
	 *			its detached.
	 */
	static bool RunDetachedUS(const FString& USPath, bool bDoNotRunFromCopy, bool bDoNotUpdateOnStartUp, bool bPassP4Env);

	/**
	 * Unreal sync main function.
	 *
	 * @param CommandLine Command line that the program was called with.
	 */
	static void RunUnrealSync(const TCHAR* CommandLine);

	/**
	 * Tells if loading has finished.
	 *
	 * @returns True if loading has finished. False otherwise.
	 */
	static bool LoadingFinished();

	/**
	 * Gets initialization error.
	 *
	 * @returns Initialization error.
	 */
	static const FString& GetInitializationError() { return InitializationError; }

	/**
	 * Performs the actual sync with given params.
	 *
	 * @param Settings Sync settings.
	 * @param Label Chosen label name.
	 * @param Game Chosen game.
	 * @param OnSyncProgress Delegate to run when syncing has made progress. 
	 */
	static bool Sync(const FSyncSettings& Settings, const FString& Label, const FString& Game, const FOnSyncProgress& OnSyncProgress);

	/**
	 * Save GUI settings to cache, flush it to file and close the app.
	 */
	static void SaveSettingsAndClose();

	/**
	 * Save GUI settings to cache, flush it to file and restart the app.
	 */
	static void SaveSettingsAndRestart();

private:
	/**
	 * Save cached settings to file.
	 */
	static void SaveSettings();

	/**
	 * Save GUI settings to settings' cache.
	 */
	static void SaveGUISettingsToCache();

	/**
	 * Tries to update original UnrealSync at given location.
	 *
	 * @param Location of original UnrealSync.
	 *
	 * @returns False on failure. True otherwise.
	 */
	static bool UpdateOriginalUS(const FString& OriginalUSPath);

	/**
	 * This function deletes To location if exists and copies file (or
	 * directory) from From to To location.
	 *
	 * @param To Location to which copy the file.
	 * @param From Location from which copy the file.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool DeleteIfExistsAndCopy(const FString& To, const FString& From);

	/**
	 * Deletes stale binaries from the workspace.
	 */
	static void DeleteStaleBinaries(const FOnSyncProgress& OnSyncProgress, bool bPreview);

	/* Error that happened during application initialization. */
	static FString InitializationError;
};
