// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EMovieScenePlayerStatus
{
	enum Type
	{
		Stopped,
		Playing,
		Recording,
		Scrubbing,
		MAX
	};
}

struct EMovieSceneViewportParams
{
	EMovieSceneViewportParams()
	{
		FadeAmount = 0.f;
		FadeColor = FLinearColor::Black;
		bEnableColorScaling = false;
	}

	enum SetViewportParam
	{
		SVP_FadeAmount   = 0x00000001,
		SVP_FadeColor    = 0x00000002,
		SVP_ColorScaling = 0x00000004,
		SVP_All          = SVP_FadeAmount | SVP_FadeColor | SVP_ColorScaling
	};

	SetViewportParam SetWhichViewportParam;

	float FadeAmount;
	FLinearColor FadeColor;
	FVector ColorScale; 
	bool bEnableColorScaling;
};

class FMovieSceneSequenceInstance;

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
	virtual void GetRuntimeObjects( TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray< UObject* >& OutObjects ) const = 0;


	/**
	 * Updates the perspective viewports with the actor to view through
	 *
	 * @param CameraObject The object, probably a camera, that the viewports should lock to
	 * @param UnlockIfCameraObject If this is not nullptr, release actor lock only if currently locked to this object.
	 */
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject = nullptr) const = 0;

	/*
	 * Set the perspective viewport settings
	 *
	 * @param ViewportParamMap A map from the viewport client to its settings
	 */
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) = 0;

	/*
	 * Get the current perspective viewport settings
	 *
	 * @param ViewportParamMap A map from the viewport client to its settings
	 */
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const = 0;

	/**
	 * Adds a MovieScene instance to the player.  MovieScene players need to know about each instance for actor spawning
	 *
	 * @param MovieSceneSection	The section owning the MovieScene being instanced. 
	 * @param InstanceToAdd		The instance being added
	 */
	virtual void AddOrUpdateMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd ) = 0;

	/**
	 * Removes a MovieScene instance from the player. 
	 *
	 * @param MovieSceneSection	The section owning the MovieScene that was instanced. 
	 * @param InstanceToAdd		The instance being removed
	 */
	virtual void RemoveMovieSceneInstance( class UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove ) = 0;

	/**
	 * @return The root movie scene instance being played
	 */
	virtual TSharedRef<FMovieSceneSequenceInstance> GetRootMovieSceneSequenceInstance() const = 0;

	/** @return whether the player is currently playing, scrubbing, etc. */
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const = 0;
};
