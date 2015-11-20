// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneEventSection.generated.h"


class ALevelScriptActor;


/**
 * Structure for event section keys.
 */
USTRUCT()
struct FMovieSceneEventSectionKey
{
	GENERATED_USTRUCT_BODY()

	/** The names of the events to be triggered. */
	UPROPERTY(EditAnywhere, Category=EventTrackKey)
	TArray<FName> EventNames;

	/** The time at which the event should be triggered. */
	UPROPERTY()
	float Time;

	/** Default constructor. */
	FMovieSceneEventSectionKey()
		: Time(0.0f)
	{ }

	/** Creates and initializes a new instance. */
	FMovieSceneEventSectionKey(const FName& InEventName, float InTime)
		: Time(InTime)
	{
		EventNames.Add(InEventName);
	}

	/** Operator less, used to sort the heap based on time until execution. */
	bool operator<(const FMovieSceneEventSectionKey& Other) const
	{
		return Time < Other.Time;
	}
};


/**
 * Implements a section in movie scene event tracks.
 */
UCLASS( MinimalAPI )
class UMovieSceneEventSection
	: public UMovieSceneSection
{
	GENERATED_BODY()

	/** Default constructor. */
	UMovieSceneEventSection();

public:

	/**
	 * Add a key with the specified event.
	 *
	 * @param Time The location in time where the key should be added.
	 * @param EventName The name of the event to fire at the specified time.
	 * @param KeyParams The keying parameters.
	 */
	void AddKey(float Time, const FName& EventName, FKeyParams KeyParams);

	/**
	 * @return The float curve on this section
	 */
	FNameCurve& GetEventCurve()
	{
		return Events;
	}

	/**
	 * Trigger the events that fall into the given time range.
	 *
	 * @param LevelScriptActor The script actor to trigger the events on.
	 * @param Position The current position in time.
	 * @param LastPosition The time at the last update.
	 */
	void TriggerEvents(ALevelScriptActor* LevelScriptActor, float Position, float LastPosition);

public:

	// UMovieSceneSection interface

	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;

protected:

	/**
	 * Trigger event for the specified key.
	 *
	 * @param Key The key to trigger.
	 * @param LevelScriptActor The script actor to trigger the events on.
	 */
	void TriggerEvent(const FName& Event, ALevelScriptActor* LevelScriptActor);

private:

	/** The section's keys. */
	UPROPERTY(EditAnywhere, Category="Events")
	FNameCurve Events;
};
