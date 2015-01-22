// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneShotSection.generated.h"

/**
 * Movie shots are sections on the director's track, that show what the viewer "sees"
 */
UCLASS( MinimalAPI )
class UMovieSceneShotSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/** Sets the camera guid for this shot section */
	void SetCameraGuid(const FGuid& InGuid);

	/** Gets the camera guid for this shot section */
	virtual FGuid GetCameraGuid() const;

	/** Sets the shot title */
	virtual void SetTitle(const FText& NewTitle);

	/** Gets the shot title */
	virtual FText GetTitle() const;

private:
	/** The camera possessable or spawnable that this movie shot uses */
	UPROPERTY()
	FGuid CameraGuid;

	/** The shot's title */
	UPROPERTY()
	FText Title;
};
