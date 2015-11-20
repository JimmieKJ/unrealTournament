// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneNameableTrack.h"
#include "MovieSceneEventTrack.generated.h"


/**
 * Implements a movie scene track that triggers discrete events during playback.
 */
UCLASS(MinimalAPI)
class UMovieSceneEventTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	UMovieSceneEventTrack()
		: bFireEventsWhenForwards(true)
		, bFireEventsWhenBackwards(true)
	{ }

public:

	/**
	 * Adds an event to the appropriate section.
	 *
	 * This method will create a new section if no appropriate one exists.
	 *
	 * @param Time The time at which the event should be triggered.
	 * @param EventName The name of the event to be triggered.
	 * @param KeyParams The keying parameters
	 */
	bool AddKeyToSection(float Time, FName EventName, FKeyParams KeyParams);

	/**
	 * Trigger the events that fall into the given time range.
	 *
	 * @param Position The current position in time.
	 * @param LastPosition The time at the last update.
	 */
	void TriggerEvents(float Position, float LastPosition);

public:

	// UMovieSceneTrack interface

	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual bool IsEmpty() const override;
	virtual void RemoveAllAnimationData() override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;

private:

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=TrackEvent)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=TrackEvent)
	uint32 bFireEventsWhenBackwards:1;

	/** The track's sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
