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
	 * @param InName Name of the label.
	 * @param InDate Date and time of label last modification.
	 */
	FP4Label(const FString& InName, const FDateTime& InDate);

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

class FP4DataProxy;

/**
 * Class to store info about thread running and controling P4 data loading process.
 */
class FP4DataLoader : public FRunnable
{
public:
	/* Delegate for loading finished event. */
	DECLARE_DELEGATE(FOnLoadingFinished);

	/**
	 * Constructor
	 *
	 * @param InOnLoadingFinished Delegate to run when loading finished.
	 */
	FP4DataLoader(const FOnLoadingFinished& InOnLoadingFinished, FP4DataProxy& InDataProxy);

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

	/* Reference to data proxy. */
	FP4DataProxy& DataProxy;

	/* If this thread is still running. */
	bool bInProgress;

	/* Signals if the loading process should be terminated. */
	bool bTerminate;
};

class FP4DataProxy
{
public:
	/**
	 * Gets latest label for given game name.
	 *
	 * @param GameName Current game name.
	 *
	 * @returns Latest label for current game.
	 */
	FString GetLatestLabelForGame(const FString& GameName);

	/**
	 * Gets promoted labels for given game.
	 *
	 * @param GameName Current game name.
	 *
	 * @returns Array of promoted labels for given game.
	 */
	TSharedPtr<TArray<FString> > GetPromotedLabelsForGame(const FString& GameName);

	/**
	 * Gets promotable labels for given game since last promoted.
	 *
	 * @param GameName Current game name.
	 *
	 * @returns Array of promotable labels for given game since last promoted.
	 */
	TSharedPtr<TArray<FString> > GetPromotableLabelsForGame(const FString& GameName);

	/**
	 * Gets all labels.
	 *
	 * @returns Array of all labels names.
	 */
	TSharedPtr<TArray<FString> > GetAllLabels();

	/**
	 * Gets possible game names.
	 *
	 * @returns Array of possible game names.
	 */
	TSharedPtr<TArray<FString> > GetPossibleGameNames();

	/**
	 * Tells if has valid P4 data loaded.
	 *
	 * @returns If P4 data was loaded.
	 */
	bool HasValidData() const;

	/**
	 * Clears data in proxy.
	 */
	void Reset()
	{
		Data.Reset();
	}

	/**
	 * Method to receive p4 data loading finished event.
	 *
	 * @param InData Loaded data.
	 */
	void OnP4DataLoadingFinished(TSharedPtr<FP4DataCache> InData);

private:
	/**
	 * Helper function to construct and get array of labels if data is fetched from the P4
	 * using given policy.
	 *
	 * @template_param TFillLabelsPolicy Policy class that has to implement:
	 *     static void Fill(TArray<FString>& OutLabelNames, const FString& GameName, const TArray<FP4Label>& Labels)
	 *     that will fill the array on request.
	 * @param GameName Game name to pass to the Fill method from policy.
	 * @returns Filled out labels.
	 */
	template <class TFillLabelsPolicy>
	TSharedPtr<TArray<FString> > GetLabelsForGame(const FString& GameName)
	{
		TSharedPtr<TArray<FString> > OutputLabels = MakeShareable(new TArray<FString>());

		if (Data.IsValid())
		{
			TFillLabelsPolicy::Fill(*OutputLabels, GameName, Data->GetLabels());
		}

		return OutputLabels;
	}

	/* Cached data ptr. */
	TSharedPtr<FP4DataCache> Data;
};