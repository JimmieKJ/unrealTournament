// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScenePropertyTrack.h"
#include "Slate/WidgetTransform.h"
#include "KeyParams.h"
#include "MovieScene2DTransformTrack.generated.h"


/**
 * Handles manipulation of 2D transforms in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieScene2DTransformTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	UMovieScene2DTransformTrack(const FObjectInitializer& ObjectInitializer);
	
	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	
public:

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position	The current playback position
	 * @param LastPosition	The last playback position
	 * @param InOutTransform 	The transform at the playback position
	 * @return true if anything was evaluated. Note: if false is returned InOutTransform remains unchanged
	 */
	bool Eval( float Position, float LastPostion, FWidgetTransform& InOutTransform ) const;
};
