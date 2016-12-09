// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneTrackIdentifier.h"

/** Keyable struct that represents a particular entity within an evaluation template (either a section/template or a track) */
struct FMovieSceneEvaluationKey
{
	/**
	 * Default construction to an invalid key
	 */
	FMovieSceneEvaluationKey()
		: SequenceID(MovieSceneSequenceID::Invalid)
		, TrackIdentifier(FMovieSceneTrackIdentifier::Invalid())
		, SectionIdentifier(-1)
	{}

	/**
	 * User construction
	 */
	FMovieSceneEvaluationKey(FMovieSceneSequenceIDRef InSequenceID, FMovieSceneTrackIdentifier InTrackIdentifier, uint32 InSectionIdentifier = uint32(-1))
		: SequenceID(InSequenceID)
		, TrackIdentifier(InTrackIdentifier)
		, SectionIdentifier(InSectionIdentifier)
	{}

	/**
	 * Check whether this key is valid
	 */
	bool IsValid() const
	{
		return SequenceID != MovieSceneSequenceID::Invalid && TrackIdentifier != FMovieSceneTrackIdentifier::Invalid();
	}

	/**
	 * Derive a new key from this one using the specified section identifier
	 */
	FMovieSceneEvaluationKey AsSection(uint32 InSectionIdentifier) const
	{
		FMovieSceneEvaluationKey NewKey = *this;
		NewKey.SectionIdentifier = InSectionIdentifier;
		return NewKey;
	}

	/**
	 * Convert this key into a track key
	 */
	FMovieSceneEvaluationKey AsTrack() const
	{
		FMovieSceneEvaluationKey NewKey = *this;
		NewKey.SectionIdentifier = uint32(-1);
		return NewKey;
	}

	friend bool operator==(const FMovieSceneEvaluationKey& A, const FMovieSceneEvaluationKey& B)
	{
		return A.TrackIdentifier == B.TrackIdentifier && A.SequenceID == B.SequenceID && A.SectionIdentifier == B.SectionIdentifier;
	}

	friend uint32 GetTypeHash(const FMovieSceneEvaluationKey& In)
	{
		return GetTypeHash(In.SequenceID) ^ (~GetTypeHash(In.TrackIdentifier)) ^ In.SectionIdentifier;
	}

	/** ID of the sequence that the entity is contained within */
	FMovieSceneSequenceID SequenceID;
	/** ID of the track this key relates to */
	FMovieSceneTrackIdentifier TrackIdentifier;
	/** ID of the section this key relates to (or -1 where this key relates to a track) */
	uint32 SectionIdentifier;
};
