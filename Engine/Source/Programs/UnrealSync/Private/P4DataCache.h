// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class that stores P4 label information.
 */
class FP4Label
{
public:
	/**
	 * Constructor
	 *
	 * @param Name Name of the label.
	 * @param Date Date and time of label last modification.
	 */
	FP4Label(const FString& Name, const FDateTime& Date);

	/**
	 * Gets label name.
	 *
	 * @returns Label name.
	 */
	const FString& GetName() const;

	/**
	 * Gets label last modification date.
	 *
	 * @returns Label last modification date.
	 */
	const FDateTime& GetDate() const;

private:
	/* Label name. */
	FString Name;

	/* Label last modification date. */
	FDateTime Date;
};

/**
 * Class for storing cached P4 data.
 */
class FP4DataCache
{
public:
	/**
	 * Loads P4 data from UnrealSyncList log.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	bool LoadFromLog(const FString& UnrealSyncListLog);

	/**
	 * Gets labels stored in this cache object.
	 *
	 * @returns Reference to array of labels.
	 */
	const TArray<FP4Label>& GetLabels();

private:
	/* Array object with labels. */
	TArray<FP4Label> Labels;
};

/**
 * Class to store info about thread running and controling P4 data loading process.
 */
class FP4DataLoader : public FRunnable
{
public:
	/* Delegate for loading finished event. */
	DECLARE_DELEGATE_OneParam(FOnLoadingFinished, TSharedPtr<FP4DataCache>);

	/**
	 * Constructor
	 *
	 * @param OnLoadingFinished Delegate to run when loading finished.
	 */
	FP4DataLoader(const FOnLoadingFinished& OnLoadingFinished);

	virtual ~FP4DataLoader();

	/**
	 * Initializes the runnable object.
	 *
	 * This method is called in the context of the thread object that aggregates this, not the
	 * thread that passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init() override;

	/**
	 * Main function to run for this thread.
	 *
	 * @returns Exit code.
	 */
	uint32 Run() override;

	/**
	 * Exits the runnable object.
	 *
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit();

	/**
	 * Is this loading thread in progress?
	 *
	 * @returns True if this thread is in progress. False otherwise.
	 */
	bool IsInProgress() const;

	/**
	 * Signals that the loading process should be terminated.
	 */
	void Terminate();

private:
	/* Delegate to run when loading is finished. */
	FOnLoadingFinished OnLoadingFinished;

	/* Handle for thread object. */
	FRunnableThread *Thread;

	/* If this thread is still running. */
	bool bInProgress;

	/* Signals if the loading process should be terminated. */
	bool bTerminate;
};

