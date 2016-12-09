// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneTrackIdentifier.generated.h"

USTRUCT()
struct FMovieSceneTrackIdentifier
{
	GENERATED_BODY()

	FMovieSceneTrackIdentifier()
		: Value(-1)
	{}

	static FMovieSceneTrackIdentifier Invalid() { return FMovieSceneTrackIdentifier(); }

	FMovieSceneTrackIdentifier& operator++()
	{
		++Value;
		return *this;
	}

	friend bool operator==(FMovieSceneTrackIdentifier A, FMovieSceneTrackIdentifier B)
	{
		return A.Value == B.Value;
	}

	friend bool operator!=(FMovieSceneTrackIdentifier A, FMovieSceneTrackIdentifier B)
	{
		return A.Value != B.Value;
	}

	friend uint32 GetTypeHash(FMovieSceneTrackIdentifier In)
	{
		return In.Value;
	}

private:

	friend struct FMovieSceneEvaluationTemplate;
	
	UPROPERTY()
	uint32 Value;
};
