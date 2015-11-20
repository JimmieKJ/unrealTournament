// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Runtime/MovieScene/Public/IMovieScenePlayer.h"
#include "Editor/SequencerWidgets/Public/ITimeSlider.h"
#include "KeyPropertyParams.h"


class FSequencerSelection;
class FSequencerSelectionPreview;
class UMovieSceneSequence;
class UAnimSequence;
class UMovieScene;
class UMovieSceneSection;
enum class EMapChangeType : uint8;


/**
 * Interface for sequencers.
 */
class ISequencer
	: public IMovieScenePlayer
	, public TSharedFromThis<ISequencer>
{
public:
	
	DECLARE_MULTICAST_DELEGATE(FOnGlobalTimeChanged);

public:

	/** Close the sequencer. */
	virtual void Close() = 0;

	/** @return Widget used to display the sequencer */
	virtual TSharedRef<SWidget> GetSequencerWidget() const = 0;

	/** @return The root movie scene being used */
	virtual UMovieSceneSequence* GetRootMovieSceneSequence() const = 0;

	/** @return Returns the MovieScene that is currently focused for editing by the sequencer.  This can change at any time. */
	virtual UMovieSceneSequence* GetFocusedMovieSceneSequence() const = 0;

	/**
	 * @return the currently focused movie scene instance
	 */
	virtual TSharedRef<FMovieSceneSequenceInstance> GetFocusedMovieSceneSequenceInstance() const = 0;

	/** Resets sequencer with a new animation */
	virtual void ResetToNewRootSequence(UMovieSceneSequence& NewAnimation) = 0;

	/**
	 * Focuses a sub-movie scene (MovieScene within a MovieScene) in the sequencer.
	 * 
	 * @param SequenceInstance	Sub-sequence instance to focus.
	 */
	virtual void FocusSequenceInstance(TSharedRef<FMovieSceneSequenceInstance> SequenceInstance) = 0;
	
	/**
	 * Given a sub-movie scene section, returns the instance of the movie scene for that section.
	 *
	 * @param Section The sub-movie scene section containing the sequence instance to get.
	 * @return The instance for the sub-movie scene
	 */
	virtual TSharedRef<FMovieSceneSequenceInstance> GetSequenceInstanceForSection(UMovieSceneSection& Section) const = 0;

	/**
	 * Adds a movie scene as a section inside the current movie scene
	 * 
	 * @param Sequence The sequence to add.
	 */
	virtual void AddSubSequence(UMovieSceneSequence* Sequence) = 0;

	/** @return Returns whether auto-key is enabled in this sequencer */
	virtual bool GetAutoKeyEnabled() const = 0;

	/** Sets whether autokey is enabled in this sequencer. */
	virtual void SetAutoKeyEnabled(bool bAutoKeyEnabled) = 0;

	/** @return Returns whether key all is enabled in this sequencer */
	virtual bool GetKeyAllEnabled() const = 0;

	/** Sets whether key all is enabled in this sequencer. */
	virtual void SetKeyAllEnabled(bool bKeyAllEnabled) = 0;

	/** @return Returns whether or not to key only interp properties in this sequencer */
	virtual bool GetKeyInterpPropertiesOnly() const = 0;

	/** Sets whether or not to key only interp properties in this sequencer. */
	virtual void SetKeyInterpPropertiesOnly(bool bKeyInterpPropertiesOnly) = 0;

	/** @return Returns default key interpolation */
	virtual EMovieSceneKeyInterpolation GetKeyInterpolation() const = 0;

	/** Set default key interpolation */
	virtual void SetKeyInterpolation(EMovieSceneKeyInterpolation) = 0;

	/** @return Returns whether sequencer is currently recording live data from simulated actors */
	virtual bool IsRecordingLive() const = 0;

	/**
	 * Gets the current time of the time slider relative to the passed in movie scene
	 *
	 * @param MovieScene The movie scene to get the local time for (must exist in this sequencer).
	 */
	virtual float GetCurrentLocalTime(UMovieSceneSequence& InMovieSceneSequence) = 0;
	
	/**
	 * Gets the global time.
	 *
	 * @return Global time in seconds.
	 * @see SetGlobalTime
	 */
	virtual float GetGlobalTime() = 0;

	/**
	 * Sets the global position to the time.
	 *
	 * @param Time The global time to set.
	 * @see GetGlobalTime
	 */
	virtual void SetGlobalTime(float Time) = 0;

	/** @return The current view range */
	virtual FAnimatedRange GetViewRange() const
	{
		return FAnimatedRange();
	}

	/**
	 * Sets whether perspective viewport hijacking is enabled.
	 *
	 * @param bEnabled true if the viewport should be enabled, false if it should be disabled.
	 */
	virtual void SetPerspectiveViewportPossessionEnabled(bool bEnabled) = 0;

	/*
	 * Gets whether perspective viewport hijacking is enabled.
	 */ 
	virtual bool IsPerspectiveViewportPossessionEnabled() const { return true; }

	/**
	 * Sets whether perspective viewport camera cutting is enabled.
	 *
	 * @param bEnabled true if the viewport should be enabled, false if it should be disabled.
	 */
	virtual void SetPerspectiveViewportCameraCutEnabled(bool bEnabled) = 0;

	/*
	 * Gets whether perspective viewport hijacking is enabled.
	 */ 
	virtual bool IsPerspectiveViewportCameraCutEnabled() const { return true; }

	/**
	 * Gets a handle to runtime information about the object being manipulated by a movie scene
	 * 
	 * @param Object The object to get a handle for.
	 * @param bCreateHandleIfMissing Create a handle if it doesn't exist.
	 * @return The handle to the object.
	 */
	virtual FGuid GetHandleToObject(UObject* Object, bool bCreateHandleIfMissing = true) = 0;

	/**
	 * @return Returns the object change listener for sequencer instance
	 */
	virtual class ISequencerObjectChangeListener& GetObjectChangeListener() = 0;

	virtual bool CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const = 0;

	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) = 0;

	virtual void NotifyMovieSceneDataChanged() = 0;

	virtual void UpdateRuntimeInstances() = 0;

	virtual FSequencerSelection& GetSelection() = 0;
	virtual FSequencerSelectionPreview& GetSelectionPreview() = 0;

	virtual void NotifyMapChanged(class UWorld* NewWorld, EMapChangeType MapChangeType) = 0;

	/** Gets a multicast delegate which is executed whenever the global time changes. */
	virtual FOnGlobalTimeChanged& OnGlobalTimeChanged() = 0;
};
