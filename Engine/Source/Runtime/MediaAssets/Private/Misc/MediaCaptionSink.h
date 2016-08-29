// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaStringSink.h"


class FMediaCaptionSink
	: public FTickerObjectBase
	, public IMediaStringSink
{
public:

	/**
	 * Get the current caption/subtitle string, if any.
	 *
	 * @return Caption string.
	 */
	FString GetCaption() const
	{
		return Caption;
	}

public:

	//~ FTickerObjectBase interface

	virtual bool Tick(float DeltaTime) override
	{
		TFunction<void()> Task;

		while (GameThreadTasks.Dequeue(Task))
		{
			Task();
		}

		return true;
	}

public:

	//~ IMediaStringSink interface

	virtual bool InitializeStringSink() override
	{
		return true;
	}

	virtual void DisplayStringSinkString(const FString& String, FTimespan Time) override
	{
		GameThreadTasks.Enqueue([=]() {
			Caption = String;
		});
	}

	virtual void ShutdownStringSink() override
	{
		// do nothing
	}

private:

	/** The current caption string. */
	FString Caption;

	/** Asynchronous tasks for the game thread. */
	TQueue<TFunction<void()>> GameThreadTasks;
};
