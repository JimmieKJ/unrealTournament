// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneSpawnRegister.h"
#include "UnrealClient.h"


class FMovieSceneSequenceInstance;


namespace EMovieScenePlayerStatus
{
	enum Type
	{
		Stopped,
		Playing,
		Recording,
		Scrubbing,
		Jumping,
		Stepping,
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
	virtual void GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectHandle, TArray<TWeakObjectPtr<UObject>>& OutObjects) const = 0;

	/**
	 * Updates the perspective viewports with the actor to view through
	 *
	 * @param CameraObject The object, probably a camera, that the viewports should lock to
	 * @param UnlockIfCameraObject If this is not nullptr, release actor lock only if currently locked to this object.
	 * @param bJumpCut Whether this is a jump cut, ie. the cut jumps from one shot to another shot
	 */
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject = nullptr, bool bJumpCut = false) = 0;

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

	/** 
	* @param PlaybackStatus The playback status to set
	*/
	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) = 0;

	/**
	 * Obtain an object responsible for managing movie scene spawnables
	 */
	virtual IMovieSceneSpawnRegister& GetSpawnRegister() { return NullRegister; }

	/**
	 * Access the playback context for this movie scene player
	 */
	virtual UObject* GetPlaybackContext() const { return nullptr; }

	/**
	 * Access the event contexts for this movie scene player
	 */
	virtual TArray<UObject*> GetEventContexts() const { return TArray<UObject*>(); }

	/**
	 * Test whether this is a preview player or not. As such, playback range becomes insignificant for things like spawnables
	 */
	virtual bool IsPreview() const { return false; }

private:
	/** Null register that asserts on use */
	FNullMovieSceneSpawnRegister NullRegister;
};
