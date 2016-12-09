// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "MovieSceneExecutionToken.h"

class USceneComponent;

struct FMobilityTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	MOVIESCENETRACKS_API static FMovieSceneAnimTypeID GetAnimTypeID();

private:
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override;
};

/** A token that sets a component's relative transform */
struct F3DTransformTrackToken
{
	FVector Translation;
	FRotator Rotation;
	FVector Scale;

	void Apply(USceneComponent& SceneComponent, float DeltaTime = 0.f) const;
};

struct F3DTransformTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	MOVIESCENETRACKS_API static FMovieSceneAnimTypeID GetAnimTypeID();

private:
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override;
};
