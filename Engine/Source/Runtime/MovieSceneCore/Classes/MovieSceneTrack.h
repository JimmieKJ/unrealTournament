// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrack.generated.h"

class UMovieSceneSection;

/**
 * Base class for a track in a Movie Scene
 */
UCLASS( abstract, MinimalAPI )
class UMovieSceneTrack : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * @return The name that makes this track unique from other track of the same class.
	 */
	virtual FName GetTrackName() const PURE_VIRTUAL ( UMovieSceneTrack::GetTrackName, return NAME_None; )

	/**
	 * Called when all the sections of the track need to be retrieved
	 * 
	 * @return List of all the sections in the track
	 */
	virtual TArray<UMovieSceneSection*> GetAllSections() const PURE_VIRTUAL( UMovieSceneTrack::GetAllSections, return TArray<UMovieSceneSection*>(); );

	/**
	 * Creates a runtime instance of this class
	 * 
	 * @return The created runtime instance.  This must not be null (note sharedptr not sharedref due to compatibility with uobjects not actually allowing pure virtuals)
	 */
	virtual TSharedPtr<class IMovieSceneTrackInstance> CreateInstance() PURE_VIRTUAL( UMovieSceneTrack::CreateInstance, check(false); return NULL; );

	/**
	 * Removes animation data
	 */
	virtual void RemoveAllAnimationData() {}

	/**
	 * @return Whether or not this track has any data in it
	 */
	virtual bool IsEmpty() const PURE_VIRTUAL( UMovieSceneTrack::IsEmpty, return true; );
	
	/**
	 * @return True if the data of this track show generate display nodes by default
	 */
	virtual bool HasShowableData() const {return true;}

	/**
	 * @return Whether or not this track supports multiple row indices
	 */
	virtual bool SupportsMultipleRows() const { return false; }

	/**
	 * Generates a new section suitable for use with this track
	 *
	 * @return a new section suitable for use with this track
	 */
	virtual class UMovieSceneSection* CreateNewSection() PURE_VIRTUAL( UMovieSceneTrack::CreateNewSection, return NULL; );
	
	/**
	 * Checks to see if the section is in this track
	 *
	 * @param Section	The section to query for
	 * @return			True if the section is in this track
	 */
	virtual bool HasSection( UMovieSceneSection* Section ) const PURE_VIRTUAL( UMovieSceneSection::HasSection, return false; );

	/**
	 * Removes a section from this track
	 *
	 * @param Section	The section to remove
	 */
	virtual void RemoveSection( UMovieSceneSection* Section ) PURE_VIRTUAL( UMovieSceneSection::RemoveSection, );

	/**
	 * Gets the section boundaries of this track.
	 * 
	 * @return The range of time boundaries
	 */
	virtual TRange<float> GetSectionBoundaries() const PURE_VIRTUAL( UMovieSceneTrack::GetSectionBoundaries, return TRange<float>::Empty(); );
};
