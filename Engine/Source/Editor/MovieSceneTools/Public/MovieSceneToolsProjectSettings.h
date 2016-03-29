// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneToolsProjectSettings.generated.h"

// Settings for the level sequences
UCLASS(config=EditorPerProjectUserSettings)
class MOVIESCENETOOLS_API UMovieSceneToolsProjectSettings : public UObject
{
	GENERATED_BODY()

public:
	UMovieSceneToolsProjectSettings();

	/** The default start time for new level sequences, in seconds. */
	UPROPERTY(config, EditAnywhere, Category=Timeline, meta=(Units=s))
	float DefaultStartTime;

	/** The default duration for new level sequences in seconds. */
	UPROPERTY(config, EditAnywhere, Category=Timeline, meta=(ClampMin=0.00001f, Units=s))
	float DefaultDuration;

	/** The default directory for the shots. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	FString ShotDirectory;

	/** The default prefix for shot names. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	FString ShotPrefix;

	/** The first shot number. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	uint32 FirstShotNumber;

	/** The default shot increment. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	uint32 ShotIncrement;

	/** The number of digits for the shot number. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	uint32 ShotNumDigits;

	/** The number of digits for the take number. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	uint32 TakeNumDigits;

	/** The first take number. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	uint32 FirstTakeNumber;

	/** The separator between the shot number and the take number. */
	UPROPERTY(config, EditAnywhere, Category=Shots)
	FString TakeSeparator;
};
