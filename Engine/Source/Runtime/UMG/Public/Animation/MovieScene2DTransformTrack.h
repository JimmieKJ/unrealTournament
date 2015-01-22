// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScenePropertyTrack.h"
#include "Slate/WidgetTransform.h"
#include "MovieScene2DTransformTrack.generated.h"

struct F2DTransformKey
{
	FWidgetTransform Value;
	FName CurveName;
	bool bAddKeyEvenIfUnchanged;
};


/**
 * Handles manipulation of 2D transforms in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieScene2DTransformTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
	

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position	The current playback position
	 * @param LastPosition	The last playback position
	 * @param InOutTransform 	The transform at the playback position
	 * @return true if anything was evaluated. Note: if false is returned InOutTransform remains unchanged
	 */
	bool Eval( float Position, float LastPostion, FWidgetTransform& InOutTransform ) const;


	UMG_API bool AddKeyToSection( float Time, const F2DTransformKey& TransformKey );

};