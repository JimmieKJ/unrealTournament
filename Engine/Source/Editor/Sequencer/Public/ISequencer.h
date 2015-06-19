// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Runtime/MovieSceneCore/Public/IMovieScenePlayer.h"

class UMovieScene;
class FSequencerSelection;

/**
 * Sequencer public interface
 */
class ISequencer : public IMovieScenePlayer, public TSharedFromThis<ISequencer>
{

public:
	/** @return Widget used to display the sequencer */
	virtual TSharedRef<SWidget> GetSequencerWidget() const = 0;

	/** @return The root movie scene being used */
	virtual UMovieScene* GetRootMovieScene() const = 0;

	/** @return Returns the MovieScene that is currently focused for editing by the sequencer.  This can change at any time. */
	virtual UMovieScene* GetFocusedMovieScene() const = 0;

	/**
	 * @return the currently focused movie scene instance
	 */
	virtual TSharedRef<FMovieSceneInstance> GetFocusedMovieSceneInstance() const = 0;

	/** Resets sequencer with a new RootMovieScene */
	virtual void ResetToNewRootMovieScene( UMovieScene& NewRoot, TSharedRef<class ISequencerObjectBindingManager> NewObjectBindingManager ) = 0;

	/**
	 * Focuses a sub-movie scene (MovieScene within a MovieScene) in the sequencer
	 * 
	 * @param SubMovieSceneInstance	Sub-MovieScene instance to focus
	 */
	virtual void FocusSubMovieScene( TSharedRef<FMovieSceneInstance> SubMovieSceneInstance ) = 0;
	
	/**
	 * Given a sub-movie scene section, returns the instance of the movie scene for that section
	 *
	 * @param SubMovieSceneSection	The sub-movie scene section containing a movie scene to get the instance for
	 * @return The instance for the sub-moviescene
	 */
	virtual TSharedRef<FMovieSceneInstance> GetInstanceForSubMovieSceneSection( UMovieSceneSection& SubMovieSceneSection ) const = 0;

	/**
	 * Adds a movie scene as a section inside the current movie scene
	 * 
	 * @param SubMovieScene The moviescene to add
	 */
	virtual void AddSubMovieScene( UMovieScene* SubMovieScene ) = 0;

	/** @return Returns whether auto-key is enabled in this sequencer */
	virtual bool IsAutoKeyEnabled() const = 0;

	/** @return Returns whether sequencer is currently recording live data from simulated actors */
	virtual bool IsRecordingLive() const = 0;

	/**
	 * Sets which shot sections need to be filtered to, or none to disable shot filtering
	 *
	 * @param ShotSections					The shot sections to filter to
	 * @param bZoomToShotBounds				Whether or not to zoom time to the bounds of the shots 
	 */
	virtual void FilterToShotSections(const TArray< TWeakObjectPtr<class UMovieSceneSection> >& ShotSections, bool bZoomToShotBounds = true ) = 0;
	
	/**
	 * Sets shot filtering to current selection
	 *
	 * @param bZoomToShotBounds				Whether or not to zoom time to the bounds of the shots 
	 */
	virtual void FilterToSelectedShotSections(bool bZoomToShotBounds = true ) = 0;
	
	/** Adds a new shot key with the specified camera guid */
	virtual void AddNewShot( FGuid CameraGuid ) = 0;

	/** Adds a new animation to the specified skeletal mesh object guid */
	virtual void AddAnimation( FGuid ObjectGuid, class UAnimSequence* AnimSequence ) = 0;

	/**
	 * Gets the current time of the time slider relative to the passed in movie scene
	 *
	 * @param MovieScene	The movie scene to get the local time for.  The movie scene must exist in the sequencer
	 */
	virtual float GetCurrentLocalTime( UMovieScene& MovieScene ) = 0;
	
	/**
	 * Gets the global time.
	 */
	virtual float GetGlobalTime() = 0;

	/**
	 * Sets the global position to the time.
	 *
	 * @param Time The global time to set to.
	 */
	virtual void SetGlobalTime(float Time) = 0;

	/**
	 * Sets whether perspective viewport hijacking is enabled.
	 */
	virtual void SetPerspectiveViewportPossessionEnabled(bool bEnabled) = 0;

	/**
	 * Gets a handle to runtime information about the object being manipulated by a movie scene
	 * 
	 * @param Object	The object to get a handle for
	 * @return The handle to the object
	 */
	virtual FGuid GetHandleToObject( UObject* Object ) = 0;

	/**
	 * @return Returns the object change listener for sequencer instance
	 */
	virtual class ISequencerObjectChangeListener& GetObjectChangeListener() = 0;

	virtual bool CanKeyProperty(const UClass& ObjectClass, const class IPropertyHandle& PropertyHandle) const = 0;

	virtual void KeyProperty( const TArray<UObject*>& ObjectsToKey, const class IPropertyHandle& PropertyHandle ) = 0;

	virtual void NotifyMovieSceneDataChanged() = 0;

	virtual void UpdateRuntimeInstances() = 0;

	virtual TSharedRef<ISequencerObjectBindingManager> GetObjectBindingManager() const = 0;

	virtual FSequencerSelection* GetSelection() = 0;
};
