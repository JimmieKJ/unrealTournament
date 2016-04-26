// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneEventSection.generated.h"


class ALevelScriptActor;


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
	 * @param EventContextObject The object to trigger the events on.
	 * @param Position The current position in time.
	 * @param LastPosition The time at the last update.
	 */
	void TriggerEvents(UObject* EventContextObject, float Position, float LastPosition);

public:

	// UMovieSceneSection interface

	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;

protected:

	/**
	 * Trigger event for the specified name.
	 *
	 * @param Event The Name to trigger.
	 * @param Object The object to trigger the events on.
	 */
	void TriggerEvent(const FName& Event, UObject* EventContextObject);

private:

	/** The section's keys. */
	UPROPERTY()
	FNameCurve Events;
};
