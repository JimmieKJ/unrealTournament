// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

UENUM()
enum class EMovieSceneKeyInterpolation
{
	/** Auto. */
	Auto UMETA(DisplayName="Auto"),

	/** User. */
	User UMETA(DisplayName="User"),

	/** Break. */
	Break UMETA(DisplayName="Break"),

	/** Linear. */
	Linear UMETA(DisplayName="Linear"),

	/** Constant. */
	Constant UMETA(DisplayName="Constant"),
};

/**
 * Parameters for determining keying behavior
 */
struct MOVIESCENE_API FKeyParams
{
	FKeyParams()
	{
		bCreateHandleIfMissing = false;
		bCreateTrackIfMissing = false;
		bCreateKeyOnlyWhenAutoKeying = false;
		bCreateKeyIfUnchanged = false;
		bCreateKeyIfEmpty = false;
	}

	FKeyParams(const FKeyParams& InKeyParams)
	{
		bCreateHandleIfMissing = InKeyParams.bCreateHandleIfMissing;
		bCreateTrackIfMissing = InKeyParams.bCreateTrackIfMissing;
		bCreateKeyOnlyWhenAutoKeying = InKeyParams.bCreateKeyOnlyWhenAutoKeying;
		bCreateKeyIfUnchanged = InKeyParams.bCreateKeyIfUnchanged;
		bCreateKeyIfEmpty = InKeyParams.bCreateKeyIfEmpty;
	}

	/** Create handle if it doesn't exist. */
	bool bCreateHandleIfMissing;
	/** Create track if it doesn't exist. */
	bool bCreateTrackIfMissing;
	/** Create a new key only when currently autokeying.*/
	bool bCreateKeyOnlyWhenAutoKeying;
	/** Create a key even if it's unchanged. */
	bool bCreateKeyIfUnchanged;
	/** Create a key even if the track is empty */
	bool bCreateKeyIfEmpty;
};
