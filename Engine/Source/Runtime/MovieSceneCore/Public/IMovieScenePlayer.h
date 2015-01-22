// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EMovieScenePlayerStatus
{
	enum Type
	{
		Stopped,
		Playing,
		Recording,
		BeginningScrubbing,
		Scrubbing,
		MAX
	};
}

class FMovieSceneInstance;

/**
 * Interface for movie scene players
 * Provides information for playback of a movie scene
 */
class IMovieScenePlayer
{
public:
	
	/**
	 * Gets runtime objects associated with an object handle for manipulation during playback
	 *
	 * @param ObjectHandle The handle to runtime objects
	 * @Param The list of runtime objects that will be populated
	 */
	virtual void GetRuntimeObjects( TSharedRef<FMovieSceneInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject* >& OutObjects ) const = 0;


	/**
	 * Updates the perspective viewports with the actor to view through
	 *
	 * @param ObjectToViewThrough The object, probably a camera, that the viewports should lock to
	 */
	virtual void UpdatePreviewViewports(UObject* ObjectToViewThrough) const = 0;

	/**
	 * Adds a MovieScene instance to the player.  MovieScene players need to know about each instance for actor spawning
	 *
	 * @param MovieSceneSection	The section owning the MovieScene being instanced. 
	 * @param InstanceToAdd		The instance being added
	 */
	virtual void AddMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToAdd ) = 0;

	/**
	 * Removes a MovieScene instance from the player. 
	 *
	 * @param MovieSceneSection	The section owning the MovieScene that was instanced. 
	 * @param InstanceToAdd		The instance being removed
	 */
	virtual void RemoveMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneInstance> InstanceToRemove ) = 0;

	/**
	 * @return The root movie scene instance being played
	 */
	virtual TSharedRef<FMovieSceneInstance> GetRootMovieSceneInstance() const = 0;

	/** @return whether the player is currently playing, scrubbing, etc. */
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const = 0;
};
