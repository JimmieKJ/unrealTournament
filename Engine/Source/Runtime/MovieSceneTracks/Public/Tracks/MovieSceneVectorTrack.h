// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieScenePropertyTrack.h"

#include "MovieSceneVectorTrack.generated.h"


/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieSceneVectorTrack : public UMovieScenePropertyTrack
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
	 * @param LastPosition	The last plackback position
	 * @param OutVector 	The value at the playback position
	 * @return true if anything was evaluated. Note: if false is returned OutVector remains unchanged
	 */
	virtual bool Eval( float Position, float LastPostion, FVector4& InOutVector ) const;

	/** @return Get the number of channels used by the vector */
	int32 GetNumChannelsUsed() const { return NumChannelsUsed; }

	/** Set the number of channels used by the vector */
	void SetNumChannelsUsed( int32 InNumChannelsUsed ) { NumChannelsUsed = InNumChannelsUsed; }

private:
	/** The number of channels used by the vector (2,3, or 4) */
	UPROPERTY()
	int32 NumChannelsUsed;
};
