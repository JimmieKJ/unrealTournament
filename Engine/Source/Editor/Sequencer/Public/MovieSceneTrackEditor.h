// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//@todo Sequencer - these have to be here for now because this class contains a small amount of implementation inside this header to avoid exporting this class
#include "ISequencer.h"
#include "MovieScene.h"
#include "ScopedTransaction.h"

class ISequencerSection;

DECLARE_DELEGATE_OneParam(FOnKeyProperty, float)

/**
 * Base class for handling key and section drawing and manipulation for a UMovieSceneTrack class
 * @todo Sequencer - Interface needs cleanup
 */
class FMovieSceneTrackEditor
{
public:
	/** Constructor */
	FMovieSceneTrackEditor( TSharedRef<ISequencer> InSequencer )
		: Sequencer( InSequencer )
	{}

	/** Destructor */
	virtual ~FMovieSceneTrackEditor(){};

	/** @return The current movie scene */
	UMovieScene* GetMovieScene() const
	{
		return Sequencer.Pin()->GetFocusedMovieScene();
	}

	/**
	 * Gets the movie scene that should be used for adding new keys/sections to and the time where new keys should be added
	 *
	 * @param InMovieScene	The movie scene to be used for keying
	 * @param OutTime		The current time of the sequencer which should be used for adding keys during auto-key
	 * @return true	if we can auto-key
	 */
	float GetTimeForKey( UMovieScene* InMovieScene )
	{ 
		return Sequencer.Pin()->GetCurrentLocalTime( *InMovieScene );
	}

	/** Gets whether the tool can legally autokey */
	virtual bool IsAllowedToAutoKey() const
	{
		// @todo sequencer livecapture: This turns on "auto key" for the purpose of capture keys for actor state
		// during PIE sessions when record mode is active.
		return Sequencer.Pin()->IsRecordingLive() || Sequencer.Pin()->IsAutoKeyEnabled();
	}

	void NotifyMovieSceneDataChanged()
	{
		TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
		if( SequencerPin.IsValid()  )
		{
			SequencerPin->NotifyMovieSceneDataChanged();
		}
	}
	
	void AnimatablePropertyChanged( TSubclassOf<UMovieSceneTrack> TrackClass, bool bMustBeAutokeying, FOnKeyProperty OnKeyProperty )
	{
		check(OnKeyProperty.IsBound());

		// Get the movie scene we want to autokey
		UMovieScene* MovieScene = GetMovieScene();
		float KeyTime = GetTimeForKey( MovieScene );
		bool bIsKeyingValid = !bMustBeAutokeying || IsAllowedToAutoKey();

		if( bIsKeyingValid )
		{
			check( MovieScene );

			// @todo Sequencer - The sequencer probably should have taken care of this
			MovieScene->SetFlags(RF_Transactional);
		
			// Create a transaction record because we are about to add keys
			const bool bShouldActuallyTransact = !Sequencer.Pin()->IsRecordingLive();		// Don't transact if we're recording in a PIE world.  That type of keyframe capture cannot be undone.
			FScopedTransaction AutoKeyTransaction( NSLOCTEXT("AnimatablePropertyTool", "PropertyChanged", "Animatable Property Changed"), bShouldActuallyTransact );

			OnKeyProperty.ExecuteIfBound( KeyTime );

			// Movie scene data has changed
			NotifyMovieSceneDataChanged();
		}
	}
	
	/**
	 * Finds or creates a binding to an object
	 *
	 * @param Object	The object to create a binding for
	 * @return A handle to the binding or an invalid handle if the object could not be bound
	 */
	FGuid FindOrCreateHandleToObject( UObject* Object )
	{
		return GetSequencer()->GetHandleToObject( Object );
	}

	UMovieSceneTrack* GetTrackForObject( const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName )
	{
		check( UniqueTypeName != NAME_None );

		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieScene();

		UMovieSceneTrack* Type = MovieScene->FindTrack( TrackClass, ObjectHandle, UniqueTypeName );

		if( !Type )
		{
			Type = MovieScene->AddTrack( TrackClass, ObjectHandle );
		}

		return Type;
	}

	UMovieSceneTrack* GetMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
	{
		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieScene();
		UMovieSceneTrack* Type = MovieScene->FindMasterTrack( TrackClass );

		if( !Type )
		{
			Type = MovieScene->AddMasterTrack( TrackClass );
		}

		return Type;
	}


	/** @return The sequencer bound to this handler */
	const TSharedPtr<ISequencer> GetSequencer() const { return Sequencer.Pin(); }
	
	/**
	 * Returns whether a track class is supported by this tool
	 *
	 * @param TrackClass	The track class that could be supported
	 * @return true if the type is supported
	 */
	virtual bool SupportsType( TSubclassOf<class UMovieSceneTrack> TrackClass ) const = 0;

	/**
	 * Called to generate a section layout for a particular section
	 *
	 * @param SectionObject	The section to make UI for
	 * @param SectionName	The name of the section
	 */
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( class UMovieSceneSection& SectionObject, UMovieSceneTrack* Track ) = 0;

	/**
	 * Manually adds a key

	 * @param ObjectGuid		The Guid of the object that we are adding a key to
	 * @param AdditionalAsset	An optional asset that can be added with the key
	 */
	virtual void AddKey( const FGuid& ObjectGuid, UObject* AdditionalAsset = NULL ) {}

	/** Ticks this tool */
	virtual void Tick(float DeltaTime) {}
	
	/**
	 * Called when an asset is dropped into Sequencer. Can potentially consume the asset
	 * so it doesn't get added as a spawnable
	 *
	 * @param Asset				The asset that is dropped in
	 * @param TargetObjectGuid	The object guid this asset is dropped onto, if applicable
	 * @return					True, if we want to consume this asset
	 */
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) {return false;}
	
	/**
	 * Builds up the object binding context menu for the outliner
	 *
	 * @param MenuBuilder	The menu builder to change
	 * @param ObjectBinding	The object binding this is for
	 * @param ObjectClass	The class of the object this is for
	 */
	virtual void BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) {}

private:
	/** The sequencer bound to this handler.  Used to access movie scene and time info during auto-key */
	TWeakPtr<ISequencer> Sequencer;
};

