// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealString.h"

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