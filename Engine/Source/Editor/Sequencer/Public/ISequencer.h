// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
class UMovieSceneSubSection;
class ISequencerKeyCollection;
enum class EMapChangeType : uint8;


/**
 * Defines auto-key modes.
 */
UENUM()
enum class EAutoKeyMode
{
	/** Key all properties that change. */
	KeyAll,

	/** Only key changed properties if they have exiting animations. */
	KeyAnimated,

	/** Don't auto-key */
	KeyNone
};


/**
 * Enumerates types of UI Command bindings.
 */
enum class ESequencerCommandBindings
{
	/** Bindings that are used by Sequencer widgets only. */
	Sequencer,

	/** Bindings that are shared between Sequencer and non-Sequencer widgets (subset of Sequencer commands). */
	Shared
};


/*
 * Allowable snapping modes when setting global time
 */
enum ESnapTimeMode
{
	/** No snapping */
	STM_None = 0x00000000,

	/** Snap to the time interval. */
	STM_Interval = 0x00000001,

	/** Snap to keys. */
	STM_Keys = 0x00000002,

	/** All snapping */
	STM_All = STM_Interval | STM_Keys
};


/**
 * Defines different types of movie scene data changes. 
 */
enum class EMovieSceneDataChangeType
{
	/** Data owned by a track has been modified such as adding or removing keys, or changing their values. */
	TrackValueChanged,
	/** The structure of the movie scene has changed by adding folders, object bindings, tracks, or sections. */
	MovieSceneStructureItemAdded,
	/** The structure of the movie scene has changed by removing folders, object bindings, tracks, or sections. */
	MovieSceneStructureItemRemoved,
	/** The structure of the movie scene has changed by adding and removing folders, object bindings, tracks, or sections. */
	MovieSceneStructureItemsChanged,
	/** The active movie scene has been changed to a new movie scene. */
	ActiveMovieSceneChanged,
	/** It's not known what data has changed. */
	Unknown
};


/**
 * Interface for sequencers.
 */
class ISequencer
	: public IMovieScenePlayer
	, public TSharedFromThis<ISequencer>
{
public:
	
	DECLARE_MULTICAST_DELEGATE(FOnGlobalTimeChanged);
	DECLARE_MULTICAST_DELEGATE(FOnMovieSceneDataChanged);
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChangedObjectGuids, TArray<FGuid> /*Object*/)

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
	 * @param Section The sub-movie scene section containing the sequence instance to get.
	 */
	virtual void FocusSequenceInstance(UMovieSceneSubSection& Section) = 0;
	
	/**
	 * Create a new binding for the specified object
	 */
	virtual FGuid CreateBinding(UObject& InObject, const FString& InName) = 0;

	/**
	 * Attempts to add a new spawnable to the MovieScene for the specified object (asset, class or actor instance)
	 *
	 * @param	Object	The asset, class, or actor to add a spawnable for
	 * @return	The spawnable guid for the spawnable, or an invalid Guid if we were not able to create a spawnable
	 */
	virtual FGuid MakeNewSpawnable(UObject& SourceObject) = 0;

	/**
	 * Given a sub-movie scene section, returns the instance of the movie scene for that section.
	 *
	 * @param Section The sub-movie scene section containing the sequence instance to get.
	 * @return The instance for the sub-movie scene
	 */
	virtual TSharedRef<FMovieSceneSequenceInstance> GetSequenceInstanceForSection(UMovieSceneSection& Section) const = 0;

	/**
	 * @return Whether the section has a sequence instance
	 */
	virtual bool HasSequenceInstanceForSection(UMovieSceneSection& Section) const = 0;

	/**
	 * Add actors as possessable objects to sequencer.
	 * 
	 * @param InActors The actors to add to sequencer.
	 */
	virtual void AddActors(const TArray<TWeakObjectPtr<AActor> >& InActors) = 0;

	/**
	 * Adds a movie scene as a section inside the current movie scene
	 * 
	 * @param Sequence The sequence to add.
	 */
	virtual void AddSubSequence(UMovieSceneSequence* Sequence) = 0;

	/** @return Returns the current auto-key mode. */
	virtual EAutoKeyMode GetAutoKeyMode() const = 0;

	/** Sets whether autokey is enabled in this sequencer. */
	virtual void SetAutoKeyMode(EAutoKeyMode AutoKeyMode) = 0;

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

	/** @return Returns whether key sections are infinite by default when created */
	virtual bool GetInfiniteKeyAreas() const = 0;

	/** Set infinite key area default */
	virtual void SetInfiniteKeyAreas(bool bInfiniteKeyAreas) = 0;

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
	virtual float GetGlobalTime() const = 0;

	/**
	 * Sets the global position to the time.
	 *
	 * @param Time The global time to set.
	 * @param SnapTimeMode The type of time snapping allowed.
	 * @param bRestarted Whether the time has been restarted from the beginning or looped.
	 * @see GetGlobalTime
	 */
	virtual void SetGlobalTime(float Time, ESnapTimeMode SnapTimeMode = ESnapTimeMode::STM_None, bool bRestarted = false) = 0;

	/** Set the global time directly, without performing any auto-scroll */
	virtual void SetGlobalTimeDirectly(float Time, ESnapTimeMode SnapTimeMode = ESnapTimeMode::STM_None, bool bRestarted = false) = 0;

	/** @return The current view range */
	virtual FAnimatedRange GetViewRange() const
	{
		return FAnimatedRange();
	}

	/**
	 * Set the view range, growing the working range to accomodate, if necessary
	 * @param NewViewRange The new view range. Must be a finite range
	 * @param Interpolation How to interpolate to the new view range
	 */
	virtual void SetViewRange(TRange<float> NewViewRange, EViewRangeInterpolation Interpolation = EViewRangeInterpolation::Animated) = 0;

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

	/*
	 * Render movie for a section.
	 * 
	 * @param InSection The given section to render.
	 */
	virtual void RenderMovie(UMovieSceneSection* InSection) const = 0;

	/*
	 * Puts sequencer in a silent state (whereby it will not redraw viewports, or attempt to update external state besides the sequence itself)
	 */
	virtual void EnterSilentMode() = 0;

	/*
	 * Leaves a silent state (see above)
	 */
	virtual void ExitSilentMode() = 0;

	/*
	 * Checks whether we're in silent mode or not
	 */
	virtual bool IsInSilentMode() const = 0;

	DECLARE_EVENT_TwoParams(ISequencer, FOnActorAddedToSequencer, AActor*, const FGuid)
	virtual FOnActorAddedToSequencer& OnActorAddedToSequencer() = 0;

	DECLARE_EVENT_TwoParams(ISequencer, FOnCameraCut, UObject*, bool)
	virtual FOnCameraCut& OnCameraCut() = 0;

	DECLARE_EVENT_OneParam(ISequencer, FOnPreSave, ISequencer&)
	virtual FOnPreSave& OnPreSave() = 0;

	DECLARE_EVENT_OneParam(ISequencer, FOnActivateSequence, FMovieSceneSequenceInstance&)
	virtual FOnActivateSequence& OnActivateSequence() = 0;

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

	/**
	 * Get a set of all properties currently keyed on the specified object
	 */
	virtual void GetAllKeyedProperties(UObject& Object, TSet<UProperty*>& OutProperties) = 0;

	virtual bool CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const = 0;

	virtual void KeyProperty(FKeyPropertyParams KeyPropertyParams) = 0;

	DEPRECATED( 4.13, "NotifyMovieSceneDataChanged() is deprecated, use the version that takes EMovieSceneDataChangeType" )
	void NotifyMovieSceneDataChanged() { NotifyMovieSceneDataChangedInternal(); };

protected:
	virtual void NotifyMovieSceneDataChangedInternal() = 0;

public:
	virtual void NotifyMovieSceneDataChanged( EMovieSceneDataChangeType DataChangeType ) = 0;

	virtual void UpdateRuntimeInstances() = 0;

	virtual void UpdatePlaybackRange() = 0;

	/** Get all the keys for the current sequencer selection */
	virtual void GetKeysFromSelection(TUniquePtr<ISequencerKeyCollection>& KeyCollection) = 0;

	virtual float FindNearestKey(float NewScrubPosition) = 0;

	virtual FSequencerSelection& GetSelection() = 0;
	virtual FSequencerSelectionPreview& GetSelectionPreview() = 0;

	virtual void NotifyMapChanged(class UWorld* NewWorld, EMapChangeType MapChangeType) = 0;

	/** Gets a multicast delegate which is executed whenever the global time changes. */
	virtual FOnGlobalTimeChanged& OnGlobalTimeChanged() = 0;

	/** Gets a multicast delegate which is executed whenever the movie scene data is changed. */
	virtual FOnMovieSceneDataChanged& OnMovieSceneDataChanged() = 0;

	/** Gets a multicast delegate with an array of FGuid of bound objects which is called when the outliner node selection changes. */
	virtual FOnSelectionChangedObjectGuids& GetSelectionChangedObjectGuids() = 0;

	/** @return a numeric type interface that will parse and display numbers as frames and times correctly */
	virtual TSharedRef<INumericTypeInterface<float>> GetNumericTypeInterface() = 0;

	/** @return a numeric type interface that will parse and display numbers as frames and times correctly (including zero-pad, if applicable) */
	virtual TSharedRef<INumericTypeInterface<float>> GetZeroPadNumericTypeInterface() = 0;

	/** @return the command bindings for this sequencer */
	virtual TSharedPtr<FUICommandList> GetCommandBindings(ESequencerCommandBindings Type = ESequencerCommandBindings::Sequencer) const = 0;

	/** @return Returns a widget containing the sequencer's playback controls */
	virtual TSharedRef<SWidget> MakeTransportControls(bool bExtended) = 0;

	/** Turn viewport transport controls on or off */
	virtual void SetViewportTransportControlsVisibility(bool bVisible) = 0;

	/** Attempt to find a spawned object in the currently focused movie scene, or the template object for the specified binding ID, if possible */
	virtual UObject* FindSpawnedObjectOrTemplate(const FGuid& BindingId) const = 0;

	/**
	 * Create a widget containing the spinboxes for setting the working and playback range
	 * 
	 * @param InnerContent		Inner content to be inserted to the middle of the widget (inbetween the playback range spinboxes)
	 * @return the widget
	 */
	virtual TSharedRef<SWidget> MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange) = 0;
};
