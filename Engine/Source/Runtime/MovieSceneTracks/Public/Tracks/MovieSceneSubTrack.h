// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneNameableTrack.h"
#include "MovieSceneSubTrack.generated.h"


class UMovieSceneSection;
class UMovieSceneSequence;


/**
 * A track that holds sub-sequences within a larger sequence.
 */
UCLASS(MinimalAPI)
class UMovieSceneSubTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_BODY()

public:

	/**
	 * Adds a movie scene section
	 *
	 * @param Sequence The sequence to add
	 * @param Time The time to add the section at
	 */
	MOVIESCENETRACKS_API void AddSequence(class UMovieSceneSequence& Sequence, float Time);

	/**
	 * Check whether this track contains the given sequence.
	 *
	 * @param Sequence The sequence to find.
	 * @param Recursively Whether to search for the sequence in sub-sequences.
	 * @return true if the sequence is in this track, false otherwise.
	 */
	MOVIESCENETRACKS_API bool ContainsSequence(const UMovieSceneSequence& Sequence, bool Recursively = false) const;

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
	virtual bool SupportsMultipleRows() const override;

private:

	/** All movie scene sections. */
	UPROPERTY()
	TArray<UMovieSceneSection*> Sections;
};
