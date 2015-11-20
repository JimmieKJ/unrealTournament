// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class to store info of syncing thread.
 */
class FSyncingThread : public FRunnable
{
public:
	/* On sync finished event delegate. */
	DECLARE_DELEGATE_OneParam(FOnSyncFinished, bool);
	/* On sync log chunk read event delegate. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnSyncProgress, const FString&);

	/**
	 * Constructor
	 *
	 * @param InSettings Sync settings.
	 * @param InLabelNameProvider Label name provider.
	 * @param InOnSyncFinished Delegate to run when syncing process has finished.
	 * @param InOnSyncProgress Delegate to run when syncing process has made progress.
	 */
	FSyncingThread(FSyncSettings InSettings, ILabelNameProvider& InLabelNameProvider, const FOnSyncFinished& InOnSyncFinished, const FOnSyncProgress& InOnSyncProgress);

	/**
	 * Main thread function.
	 */
	uint32 Run() override;

	/**
	 * Stops process running in the background and terminates wait for the
	 * watcher thread to finish.
	 */
	void Terminate();

private:
	/* Tells the thread to terminate the process. */
	bool bTerminate;

	/* Handle for thread object. */
	FRunnableThread* Thread;

	/* Sync settings. */
	FSyncSettings Settings;

	/* Label name provider. */
	ILabelNameProvider& LabelNameProvider;

	/* Delegate that will be run when syncing process has finished. */
	FOnSyncFinished OnSyncFinished;

	/* Delegate that will be run when syncing process has finished. */
	FOnSyncProgress OnSyncProgress;
};