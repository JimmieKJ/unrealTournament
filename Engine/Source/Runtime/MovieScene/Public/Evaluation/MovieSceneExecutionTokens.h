// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneEvaluationKey.h"
#include "Misc/InlineValue.h"
#include "MovieSceneExecutionToken.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieScenePlayback.h"


/**
 * Ordered execution token stack that accumulates tokens that will apply animated state to the sequence environment at a later time
 */
struct FMovieSceneExecutionTokens
{
	FMovieSceneExecutionTokens()
		: Context(FMovieSceneEvaluationRange(0.f), EMovieScenePlayerStatus::Stopped)
	{
	}

	FMovieSceneExecutionTokens(const FMovieSceneExecutionTokens&) = delete;
	FMovieSceneExecutionTokens& operator=(const FMovieSceneExecutionTokens&) = delete;
	
	/**
	 * Add a new IMovieSceneExecutionToken derived token to the stack
	 */
	template<typename T>
	FORCEINLINE typename TEnableIf<TPointerIsConvertibleFromTo<typename TRemoveReference<T>::Type, const IMovieSceneExecutionToken>::Value>::Type Add(T&& InToken)
	{
		check(TrackKey.IsValid() && Operand.IsValid());
		Tokens.Add(FEntry(Operand, TrackKey, SectionIdentifier, CompletionMode, Context, Forward<T>(InToken)));
	}

	/**
	 * Internal: Set the operand we're currently operating on
	 */
	FORCEINLINE void SetOperand(const FMovieSceneEvaluationOperand& InOperand)
	{
		Operand = InOperand;
	}

	/**
	 * Internal: Set the track we're currently evaluating
	 */
	FORCEINLINE void SetTrack(const FMovieSceneEvaluationKey& InTrackKey)
	{
		TrackKey = InTrackKey;
	}

	/**
	 * Internal: Set the section we're currently evaluating
	 */
	FORCEINLINE void SetSectionIdentifier(uint32 InSectionIdentifier)
	{
		SectionIdentifier = InSectionIdentifier;
	}

	/**
	 * Internal: Set the current context
	 */
	FORCEINLINE void SetContext(const FMovieSceneContext& InContext)
	{
		Context = InContext;
	}

	/**
	 * Internal: Set what should happen when the section is no longer evaluated
	 */
	FORCEINLINE void SetCompletionMode(EMovieSceneCompletionMode InCompletionMode)
	{
		CompletionMode = InCompletionMode;
	}

	struct FEntry
	{
		FEntry(const FMovieSceneEvaluationOperand& InOperand, const FMovieSceneEvaluationKey& InTrackKey, uint32 InSectionIdentifier, EMovieSceneCompletionMode InCompletionMode, const FMovieSceneContext& InContext, TInlineValue<IMovieSceneExecutionToken, 32>&& InToken)
			: Operand(InOperand)
			, TrackKey(InTrackKey)
			, SectionIdentifier(InSectionIdentifier)
			, CompletionMode(InCompletionMode)
			, Context(InContext)
			, Token(MoveTemp(InToken))
		{
		}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
		FEntry(FEntry&&) = default;
		FEntry& operator=(FEntry&&) = default;
#else
		FEntry(FEntry&& RHS)
			: Operand(MoveTemp(RHS.Operand))
			, TrackKey(MoveTemp(RHS.TrackKey))
			, SectionIdentifier(MoveTemp(RHS.SectionIdentifier))
			, CompletionMode(MoveTemp(RHS.CompletionMode))
			, Context(MoveTemp(RHS.Context))
			, Token(MoveTemp(RHS.Token))
		{
		}
		FEntry& operator=(FEntry&& RHS)
		{
			Operand = MoveTemp(RHS.Operand);
			TrackKey = MoveTemp(RHS.TrackKey);
			SectionIdentifier = MoveTemp(RHS.SectionIdentifier);
			CompletionMode = MoveTemp(RHS.CompletionMode);
			Context = MoveTemp(RHS.Context);
			Token = MoveTemp(RHS.Token);
			return *this;
		}
#endif

		/** The operand we were operating on when this token was added */
		FMovieSceneEvaluationOperand Operand;
		/** The track we were evaluating when this token was added */
		FMovieSceneEvaluationKey TrackKey;
		/** The section identifier we were evaluating when this token was added */
		uint32 SectionIdentifier;
		/** What should happen when the entity is no longer evaluated */
		EMovieSceneCompletionMode CompletionMode;
		/** The context from when this token was added */
		FMovieSceneContext Context;
		/** The user-provided token */
		TInlineValue<IMovieSceneExecutionToken, 32> Token;
	};

	MOVIESCENE_API void Apply(IMovieScenePlayer& Player);

	/** Ordered array of tokens */
	TArray<FEntry> Tokens;

private:

	/** The operand we're currently operating on */
	FMovieSceneEvaluationOperand Operand;
	/** The track we're currently evaluating */
	FMovieSceneEvaluationKey TrackKey;
	/** The section we're currently evaluating */
	uint32 SectionIdentifier;
	/* What should happen when the section is no longer evaluated */
	EMovieSceneCompletionMode CompletionMode;
	/** The current context */
	FMovieSceneContext Context;
};
