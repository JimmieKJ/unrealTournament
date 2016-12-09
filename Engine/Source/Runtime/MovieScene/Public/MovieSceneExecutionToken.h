// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneFwd.h"
#include "Misc/InlineValue.h"
#include "Evaluation/MovieSceneAnimTypeID.h"

class IMovieScenePlayer;
struct FMovieSceneContext;
struct FMovieSceneEvaluationOperand;
struct FPersistentEvaluationData;

struct IMovieScenePreAnimatedToken
{
	virtual ~IMovieScenePreAnimatedToken() {}
	/** Restore state for the specified object, only called when this token was created with a bound object */
	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) = 0;
};
typedef TInlineValue<IMovieScenePreAnimatedToken, 32> IMovieScenePreAnimatedTokenPtr;

/** Type required for production of pre-animated state tokens. Implemented as an interface rather than a callback to ensure efficient construction (these types are often constructed, but rarely called) */
struct IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const = 0;
};

struct IMovieScenePreAnimatedGlobalToken
{
	virtual ~IMovieScenePreAnimatedGlobalToken() {}
	/** Restore global state for a master track only called when this token was created with no bound object */
	virtual void RestoreState(IMovieScenePlayer& Player) = 0;
};
typedef TInlineValue<IMovieScenePreAnimatedGlobalToken, 32> IMovieScenePreAnimatedGlobalTokenPtr;

/** Type required for production of pre-animated state tokens. Implemented as an interface rather than a callback to ensure efficient construction (these types are often constructed, but rarely called) */
struct IMovieScenePreAnimatedGlobalTokenProducer
{
	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const = 0;
};

struct IMovieSceneExecutionToken
{
	virtual ~IMovieSceneExecutionToken() {}
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) = 0;
};
