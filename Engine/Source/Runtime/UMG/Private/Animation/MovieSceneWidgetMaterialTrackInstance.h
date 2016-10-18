// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneMaterialTrackInstance.h"

class UMovieSceneWidgetMaterialTrack;

/** A material track instance specialized for widget materials. */
class FMovieSceneWidgetMaterialTrackInstance : public FMovieSceneMaterialTrackInstance
{
public:
	/** Creates a new track instance with a specific widget material track. */
	FMovieSceneWidgetMaterialTrackInstance( UMovieSceneWidgetMaterialTrack& InMaterialTrack );

protected:
	/** FMovieSceneMaterialTrackInstance interface. */
	virtual UMaterialInterface* GetMaterialForObject( UObject* Object ) const override;
	virtual void SetMaterialForObject( UObject* Object, UMaterialInterface* Material ) override;

private:
	TArray<FName> BrushPropertyNamePath;
};
