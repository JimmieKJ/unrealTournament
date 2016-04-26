// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene3DConstraintTrackInstance.h"

class UMovieScene3DAttachTrack;
class UMovieScene3DConstraintSection;

/**
 * Instance of a UMovieScene3DAttachTrack
 */
class FMovieScene3DAttachTrackInstance
	: public FMovieScene3DConstraintTrackInstance
{
public:
	FMovieScene3DAttachTrackInstance( UMovieScene3DAttachTrack& InAttachTrack );

	/** FMovieScene3DConstraintTrackInstance interface */
	virtual void UpdateConstraint(float Position, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, AActor* Actor, UMovieScene3DConstraintSection* ConstraintSection) override;
};
