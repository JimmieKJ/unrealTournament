// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneLevelVisibilitySection.generated.h"


/**
 * Visibility options for the level visibility section.
 */
UENUM()
enum class ELevelVisibility
{
	/** The streamed levels should be visible. */
	Visible,

	/** The streamed levels should be hidden. */
	Hidden
};


/**
 * A section for use with the movie scene level visibility track, which controls streamed level visibility.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneLevelVisibilitySection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	ELevelVisibility GetVisibility() const;
	void SetVisibility(ELevelVisibility InVisibility);

	TArray<FName>* GetLevelNames();

public:

	//~ UMovieSceneSection interface

	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	/** Whether or not the levels in this section should be visible or hidden. */
	UPROPERTY(EditAnywhere, Category = LevelVisibility)
	TEnumAsByte<ELevelVisibility> Visibility;

	/** The short names of the levels who's visibility is controlled by this section. */
	UPROPERTY(EditAnywhere, Category = LevelVisibility)
	TArray<FName> LevelNames;
};
