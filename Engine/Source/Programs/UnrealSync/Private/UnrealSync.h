// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"
#include "P4DataCache.h"

#define LOCTEXT_NAMESPACE "UnrealSync"

/**
 * Class to store date needed for sync monitoring thread.
 */
class FSyncingThread;

/**
* Interface of sync command line provider widget.
*/
class ILabelNameProvider
{
public:
	/**
	 * Gets label name from current options picked by user.
	 *
	 * @returns Label name.
	 */
	virtual FString GetLabelName() const = 0;

	/**
	 * Gets message to display when sync task has started.
	 */
	virtual FString GetStartedMessage() const
	{
		return "Sync started.";
	}

	/**
	 * Gets message to display when sync task has finished.
	 */
	virtual FString GetFinishedMessage() const
	{
		return "Sync finished.";
	}

	/**
	* Gets game name from current options picked by user.
	*
	* @returns Game name.
	*/
	virtual FString GetGameName() const
	{
		return CurrentGameName;
	}

	/**
	 * Reset data.
	 *
	 * @param GameName Current game name.
	 */
	virtual void ResetData(const FString& GameName)
	{
		CurrentGameName = GameName;
	}

	/**
	 * Refresh data.
	 *
	 * @param GameName Current game name.
	 */
	virtual void RefreshData(const FString& GameName)
	{
		CurrentGameName = GameName;
	}

	/**
	 * Tells if this particular widget has data ready for sync.
	 *
	 * @returns True if ready. False otherwise.
	 */
	virtual bool IsReadyForSync() const = 0;

private:
	/* Current game name. */
	FString CurrentGameName;
};

struct FSyncSettings
{
	/* Artist sync? */
	bool bArtist;

	/* Preview sync? */
	bool bPreview;

	/* Auto-clobber sync? */
	bool bAutoClobber;

	/* Override sync step. If set then it overrides collecting sync steps and uses this one instead. */
	FString OverrideSyncStep;

	FSyncSettings(bool bArtist, bool bPreview, bool bAutoClobber, FString OverrideSyncStep = FString())
		: bArtist(bArtist)
		, bPreview(bPreview)
		, bAutoClobber(bAutoClobber)
		, OverrideSyncStep(MoveTemp(OverrideSyncStep))
	{ }

	FSyncSettings(const FSyncSettings&& Other)
		: bArtist(Other.bArtist)
		, bPreview(Other.bPreview)
		, bAutoClobber(Other.bAutoClobber)
		, OverrideSyncStep(MoveTemp(Other.OverrideSyncStep))
	{ }
};

/**
 * Helper class with functions used to sync engine.
 */
class FUnrealSync
{
public:
	/* On data loaded event delegate. */
	DECLARE_DELEGATE(FOnDataLoaded);

	/* On data reset event delegate. */
	DECLARE_DELEGATE(FOnDataReset);

	/* On sync finished event delegate. */
	DECLARE_DELEGATE_OneParam(FOnSyncFinished, bool);
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
	 * Gets latest label for given game name.
	 *
	 * @param GameName Current game name.
	 *
	 * @returns Latest label for current game.
	 */
	static FString GetLatestLabelForGame(const FString& GameName);

	/**
	 * Gets promoted labels for given game.
	 *
	 * @param GameName Current game name.
	 *
	 * @returns Array of promoted labels for given game.
	 */
	static TSharedPtr<TArray<FString> > GetPromotedLabelsForGame(const FString& GameName);

	/**
	 * Gets promotable labels for given game since last promoted.
	 *
	 * @param GameName Current game name.
	 *
	 * @returns Array of promotable labels for given game since last promoted.
	 */
	static TSharedPtr<TArray<FString> > GetPromotableLabelsForGame(const FString& GameName);

	/**
	 * Gets all labels.
	 *
	 * @returns Array of all labels names.
	 */
	static TSharedPtr<TArray<FString> > GetAllLabels();

	/**
	 * Gets possible game names.
	 *
	 * @returns Array of possible game names.
	 */
	static TSharedPtr<TArray<FString> > GetPossibleGameNames();

	/**
	 * Gets shared promotable display name.
	 *
	 * @returns Shared promotable display name.
	 */
	static const FString& GetSharedPromotableDisplayName();

	/**
	 * Registers event that will be trigger when data is loaded.
	 *
	 * @param OnDataLoaded Delegate to call when event happens.
	 */
	static void RegisterOnDataLoaded(const FOnDataLoaded& OnDataLoaded);

	/**
	 * Registers event that will be trigger when data is reset.
	 *
	 * @param OnDataReset Delegate to call when event happens.
	 */
	static void RegisterOnDataReset(const FOnDataReset& OnDataReset);

	/**
	 * Start async loading of the P4 label data in case user wants it.
	 */
	static void StartLoadingData();

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
	 * Tells that labels names are currently being loaded.
	 *
	 * @returns True if labels names are currently being loaded. False otherwise.
	 */
	static bool IsLoadingInProgress()
	{
		return LoaderThread.IsValid() && LoaderThread->IsInProgress();
	}

	/**
	 * Terminates background P4 data loading process.
	 */
	static void TerminateLoadingProcess();

	/**
	 * Terminates P4 syncing process.
	 */
	static void TerminateSyncingProcess();

	/**
	 * Method to receive p4 data loading finished event.
	 *
	 * @param Data Loaded data.
	 */
	static void OnP4DataLoadingFinished(TSharedPtr<FP4DataCache> Data);

	/**
	 * Tells if has valid P4 data loaded.
	 *
	 * @returns If P4 data was loaded.
	 */
	static bool HasValidData();

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
	 * Gets labels from the loaded cache.
	 *
	 * @returns Array of loaded labels.
	 */
	static const TArray<FP4Label>& GetLabels() { return Data->GetLabels(); }

	/**
	 * Launches UAT UnrealSync command with given command line and options.
	 *
	 * @param Settings Sync settings.
	 * @param LabelNameProvider Object that will provide label name to syncing thread.
	 * @param OnSyncFinished Delegate to run when syncing is finished.
	 * @param OnSyncProgress Delegate to run when syncing has made progress.
	 */
	static void LaunchSync(FSyncSettings Settings, ILabelNameProvider& LabelNameProvider, const FOnSyncFinished& OnSyncFinished, const FOnSyncProgress& OnSyncProgress);

	/**
	 * Performs the actual sync with given params.
	 *
	 * @param Settings Sync settings.
	 * @param Label Chosen label name.
	 * @param Game Chosen game.
	 * @param OnSyncProgress Delegate to run when syncing has made progress. 
	 */
	static bool Sync(const FSyncSettings& Settings, const FString& Label, const FString& Game, const FOnSyncProgress& OnSyncProgress);
private:
	/**
	 * Tries to update original UnrealSync at given location.
	 *
	 * @param Location of original UnrealSync.
	 *
	 * @returns False on failure. True otherwise.
	 */
	static bool UpdateOriginalUS(const FString& OriginalUSPath);

	/**
	 * This function copies file from From to To location and deletes
	 * if To exists.
	 *
	 * @param To Location to which copy the file.
	 * @param From Location from which copy the file.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	static bool DeleteIfExistsAndCopyFile(const FString& To, const FString& From);

	/* Tells if loading has finished. */
	static bool bLoadingFinished;

	/* Data loaded event. */
	static FOnDataLoaded OnDataLoaded;

	/* Data reset event. */
	static FOnDataReset OnDataReset;

	/* Cached data ptr. */
	static TSharedPtr<FP4DataCache> Data;

	/* Background loading process monitoring thread. */
	static TSharedPtr<FP4DataLoader> LoaderThread;

	/* Background syncing process monitoring thread. */
	static TSharedPtr<FSyncingThread> SyncingThread;

	/* Error that happened during application initialization. */
	static FString InitializationError;
};
