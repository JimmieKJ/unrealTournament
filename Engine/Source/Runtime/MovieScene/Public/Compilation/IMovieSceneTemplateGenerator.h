// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneFwd.h"
#include "Containers/ArrayView.h"

class UMovieSceneTrack;
struct FMovieSceneEvaluationFieldSegmentPtr;
struct FMovieSceneEvaluationTrack;
struct FMovieSceneSequenceTransform;
struct FMovieSceneSharedDataId;
struct FMovieSceneSubSequenceData;

/** Abstract base class used to generate evaluation templates */
struct IMovieSceneTemplateGenerator
{
	/**
	 * Add a new track that is to be owned by this template
	 *
	 * @param InTrackTemplate			The track template to add
	 * @param SourceTrack				The originating track
	 */
	virtual void AddOwnedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) = 0;

	/**
	 * Add a new track that is potentially shared between multiple tracks. Only one instance of SharedId can exist.
	 *
	 * @param InTrackTemplate			The track template to add
	 * @param SharedId					The identifier of the shared track (for cross referencing between tracks)
	 * @param SourceTrack				The originating track
	 */
	virtual void AddSharedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, FMovieSceneSharedDataId SharedId, const UMovieSceneTrack& SourceTrack) = 0;

	/**
	 * Add a legacy track to the template
	 *
	 * @param InTrackTemplate			The legacy track template
	 * @param SourceTrack				The originating track
	 */
	virtual void AddLegacyTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) = 0;

	/**
	 * Add a set of remapped segments from a sub sequence to this template
	 *
	 * @param RootRange					The range in which to add the specified segments (at the root level)
	 * @param SegmentPtrs				The remapped segment ptrs
	 */
	virtual void AddExternalSegments(TRange<float> RootRange, TArrayView<const FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrs) = 0;

	/**
	 * Get a sequence's transform from its ID
	 *
	 * @param SequenceID				The ID of the sequence to get a transform for
	 * @return The sequence's transform
	 */
	virtual FMovieSceneSequenceTransform GetSequenceTransform(FMovieSceneSequenceIDRef InSequenceID) const = 0;

	/**
	 * Generate a new sequence ID for the specified sub sequence
	 *
	 * @param SequenceData				Data pertaining to the sequence to add
	 * @param ParentID					ID of the parent sequence
	 */
	virtual FMovieSceneSequenceID GenerateSequenceID(FMovieSceneSubSequenceData SequenceData, FMovieSceneSequenceIDRef ParentID) = 0;

	/**
	 * Indicate that the specified evaluation group should flush the execution stack immediately
	 *
	 * @param EvaluationGroup			The name of the evaluation group to flush
	 */
	virtual void FlushGroupImmediately(FName EvaluationGroup) = 0;
};
