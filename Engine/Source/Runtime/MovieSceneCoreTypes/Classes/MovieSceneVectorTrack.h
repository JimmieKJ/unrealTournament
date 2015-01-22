// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieScenePropertyTrack.h"

#include "MovieSceneVectorTrack.generated.h"

template<typename VecType>
struct FVectorKey
{
	VecType Value;
	FName CurveName;
	bool bAddKeyEvenIfUnchanged;
};

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
	 * Adds a key to a section.  Will create the section if it doesn't exist
	 *
	 * @param ObjectHandle		Handle to the object(s) being changed
	 * @param InPropertyName	The name of the property being manipulated.  @todo Sequencer - Could be a UFunction name
	 * @param Time				The time relative to the owning movie scene where the section should be
	 * @param InKey				The vector key to add
	 * @param InChannelsUsed	The number of channels used, 2, 3, or 4, determining which type of vector this is
	 * @return True if the key was successfully added.
	 */

	virtual bool AddKeyToSection( float Time, const FVectorKey<FVector4>& Key );
	virtual bool AddKeyToSection( float Time, const FVectorKey<FVector>& Key );
	virtual bool AddKeyToSection( float Time, const FVectorKey<FVector2D>& Key );
	
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

private:
	virtual bool AddKeyToSection( float Time, const FVector4& InKey, int32 InChannelsUsed, FName CurveName, bool bAddKeyEvenIfUnchanged );
private:
	/** The number of channels used by the vector (2,3, or 4) */
	UPROPERTY()
	int32 NumChannelsUsed;
};
