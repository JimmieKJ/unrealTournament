// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "IMovieScenePlayer.h"

/** Enumeration specifying whether we're playing forwards or backwards */
enum class EPlayDirection
{
	Forwards, Backwards
};

/** MovieScene evaluation context. Should remain bitwise copyable, and contain no external state since this has the potential to be used on a thread */
struct MOVIESCENE_API FMovieSceneEvaluationRange
{
	/**
	 * Construct this range from a single fixed time
	 */
	FMovieSceneEvaluationRange(float InTime);

	/**
	 * Construct this range from a raw range and a direction
	 */
	FMovieSceneEvaluationRange(TRange<float> InRange, EPlayDirection InDirection);

	/**
	 * Construct this range from 2 times, and whether the range should include the previous time or not
	 */
	FMovieSceneEvaluationRange(float InCurrentTime, float InPreviousTime, bool bInclusivePreviousTime = false);

	/**
	 * Get the range that we should be evaluating
	 */
	FORCEINLINE TRange<float> GetRange() const
	{
		return EvaluationRange;
	}

	/**
	 * Get the direction to evaluate our range
	 */
	FORCEINLINE EPlayDirection GetDirection() const 
	{
		return Direction;
	}

	/**
	 * Get the current time of evaluation.
	 */
	FORCEINLINE float GetTime() const
	{
		if (TimeOverride != TNumericLimits<float>::Lowest())
		{
			return TimeOverride;
		}

		return Direction == EPlayDirection::Forwards ? EvaluationRange.GetUpperBoundValue() : EvaluationRange.GetLowerBoundValue();
	}

	/**
	 * Get the absolute amount of time that has passed since the last update (will always be >= 0)
	 */
	FORCEINLINE float GetDelta() const
	{
		return EvaluationRange.Size<float>();
	}

	/**
	 * Get the previous time of evaluation. Should not generally be used. Prefer GetRange instead.
	 */
	FORCEINLINE float GetPreviousTime() const
	{
		return Direction == EPlayDirection::Forwards ? EvaluationRange.GetLowerBoundValue() : EvaluationRange.GetUpperBoundValue();
	}
	
	/**
	 * Override the time that we're actually evaluating at
	 */
	FORCEINLINE void OverrideTime(float InTimeOverride)
	{
		TimeOverride = InTimeOverride;
	}

protected:

	/** The range to evaluate */
	TRange<float> EvaluationRange;

	/** Whether to evaluate the range forwards, or backwards */
	EPlayDirection Direction;

	/** Overridden current time (doesn't manipulate the actual evaluated range) */
	float TimeOverride;
};

/** MovieScene evaluation context. Should remain bitwise copyable, and contain no external state since this has the potential to be used on a thread */
struct FMovieSceneContext : FMovieSceneEvaluationRange
{
	/**
	 * Construction from an evaluation range, and a current status
	 */
	FMovieSceneContext(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type InStatus)
		: FMovieSceneEvaluationRange(InRange)
		, Status(InStatus)
		, bHasJumped(false)
		, bSilent(false)
	{}

	/**
	 * Get the playback status
	 */
	FORCEINLINE EMovieScenePlayerStatus::Type GetStatus() const
	{
		return Status;
	}

	/**
	 * Check whether we've just jumped to a different time
	 */
	FORCEINLINE bool HasJumped() const
	{
		return bHasJumped;
	}

	/**
	 * Check whether we're evaluating in silent mode (no audio or mutating eval)
	 */
	FORCEINLINE bool IsSilent() const
	{
		return bSilent;
	}

	/**
	 * Get the current root to sequence transform for the current sub sequence
	 */
	FORCEINLINE const FMovieSceneSequenceTransform& GetRootToSequenceTransform() const
	{
		return RootToSequenceTransform;
	}

public:

	/**
	 * Indicate that we've just jumped to a different time
	 */
	FMovieSceneContext& SetHasJumped(bool bInHasJumped)
	{
		bHasJumped = bInHasJumped;
		return *this;
	}

	/**
	 * Set the context to silent mode
	 */
	FMovieSceneContext& SetIsSilent(bool bInIsSilent)
	{
		bSilent = bInIsSilent;
		return *this;
	}

	/**
	 * Clamp the current evaluation range to the specified range (in the current transform space)
	 */
	FMovieSceneContext Clamp(TRange<float> NewRange) const
	{
		FMovieSceneContext NewContext = *this;
		NewContext.EvaluationRange = NewRange;
		return NewContext;
	}

	/**
	 * Transform this context to a different sub sequence space
	 */
	FMovieSceneContext Transform(const FMovieSceneSequenceTransform& InTransform) const
	{
		FMovieSceneContext NewContext = *this;
		NewContext.EvaluationRange = EvaluationRange * InTransform;
		NewContext.RootToSequenceTransform = NewContext.RootToSequenceTransform * InTransform;
		return NewContext;
	}

protected:

	/** The transform from the root sequence to the current sequence space */
	FMovieSceneSequenceTransform RootToSequenceTransform;

	/** The current playback status */
	EMovieScenePlayerStatus::Type Status;

	/** Whether this evaluation frame is happening as part of a large jump */
	bool bHasJumped : 1;

	/** Whether this evaluation should happen silently */
	bool bSilent : 1;
};

/** Helper class designed to abstract the complexity of calculating evaluation ranges for previous times and fixed time intervals */
struct MOVIESCENE_API FMovieScenePlaybackPosition
{
	/**
	 * Reset this position to the specified time.
	 * @note Future calls to 'PlayTo' will include this time in its resulting evaluation range
	 */
	void Reset(float StartPos);

	/**
	 * Jump to the specified time.
	 * @note Will reset previous play position. Any subsequent call to 'PlayTo' will include NewPosition.
	 * @return A range encompassing only the specified time.
	 */
	FMovieSceneEvaluationRange JumpTo(float NewPosition, TOptional<float> FixedInterval);

	/**
	 * Play from the previously evaluated play time, to the specified time
	 * @return An evaluation range from the previously evaluated time, to the specified time
	 */
	FMovieSceneEvaluationRange PlayTo(float NewPosition, TOptional<float> FixedInterval);

	/**
	 * Get the last position that was set
	 */
	TOptional<float> GetPreviousPosition() const { return PreviousPosition; }

	/**
	 * Get the last actual time that was evaluated during playback
	 */
	TOptional<float> GetLastPlayEvalPostition() const { return PreviousPlayEvalPosition; }


private:
	/** The previous *actual* time position set. Never rounded to a fixed interval. */
	TOptional<float> PreviousPosition;

	/** The previous evaluated position when playing, potentially rounded to a frame interval. */
	TOptional<float> PreviousPlayEvalPosition;
};
