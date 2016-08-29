// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

//@todo Sequencer - these have to be here for now because this class contains a small amount of implementation inside this header to avoid exporting this class
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "ScopedTransaction.h"
#include "MovieSceneSequence.h"


class ISequencerSection;
class UMovieSceneTrack;


/** Delegate for adding keys for a property
 * float - The time at which to add the key.
 * return - True if any data was changed as a result of the call, otherwise false.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnKeyProperty, float)

DECLARE_DELEGATE_RetVal_OneParam(bool, FCanKeyProperty, float)


/**
 * Base class for handling key and section drawing and manipulation
 * of a UMovieSceneTrack class.
 *
 * @todo Sequencer Interface needs cleanup
 */
class FMovieSceneTrackEditor
	: public TSharedFromThis<FMovieSceneTrackEditor>
	, public ISequencerTrackEditor
{
public:

	/** Constructor */
	FMovieSceneTrackEditor(TSharedRef<ISequencer> InSequencer)
		: Sequencer(InSequencer)
	{ }

	/** Destructor */
	virtual ~FMovieSceneTrackEditor() { };

public:

	/** @return The current movie scene */
	UMovieSceneSequence* GetMovieSceneSequence() const
	{
		return Sequencer.Pin()->GetFocusedMovieSceneSequence();
	}

	/**
	 * Gets the movie scene that should be used for adding new keys/sections to and the time where new keys should be added
	 *
	 * @param InMovieScene	The movie scene to be used for keying
	 * @param OutTime		The current time of the sequencer which should be used for adding keys during auto-key
	 * @return true	if we can auto-key
	 */
	float GetTimeForKey( UMovieSceneSequence* InMovieSceneSequence )
	{ 
		return Sequencer.Pin()->GetCurrentLocalTime( *InMovieSceneSequence );
	}

	void UpdatePlaybackRange()
	{
		TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
		if( SequencerPin.IsValid()  )
		{
			SequencerPin->UpdatePlaybackRange();
		}
	}

	void AnimatablePropertyChanged( FOnKeyProperty OnKeyProperty )
	{
		check(OnKeyProperty.IsBound());

		// Get the movie scene we want to autokey
		UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
		if (MovieSceneSequence)
		{
			float KeyTime = GetTimeForKey( MovieSceneSequence );

			if( !Sequencer.Pin()->IsRecordingLive() )
			{
				// @todo Sequencer - The sequencer probably should have taken care of this
				MovieSceneSequence->SetFlags(RF_Transactional);
		
				// Create a transaction record because we are about to add keys
				const bool bShouldActuallyTransact = !Sequencer.Pin()->IsRecordingLive();		// Don't transact if we're recording in a PIE world.  That type of keyframe capture cannot be undone.
				FScopedTransaction AutoKeyTransaction( NSLOCTEXT("AnimatablePropertyTool", "PropertyChanged", "Animatable Property Changed"), bShouldActuallyTransact );

				if( OnKeyProperty.Execute( KeyTime ) )
				{
					// TODO: This should pass an appropriate change type here instead of always passing structure changed since most
					// changes will be value changes.
					Sequencer.Pin()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
				}

				UpdatePlaybackRange();
			}
		}
	}

	struct FFindOrCreateHandleResult
	{
		FGuid Handle;
		bool bWasCreated;
	};
	
	/**
	 * Finds or creates a binding to an object
	 *
	 * @param Object	The object to create a binding for
	 * @return A handle to the binding or an invalid handle if the object could not be bound
	 */
	FFindOrCreateHandleResult FindOrCreateHandleToObject( UObject* Object, bool bCreateHandleIfMissing = true )
	{
		FFindOrCreateHandleResult Result;
		bool bHandleWasValid = GetSequencer()->GetHandleToObject( Object, false ).IsValid();
		Result.Handle = GetSequencer()->GetHandleToObject( Object, bCreateHandleIfMissing );
		Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
		return Result;
	}

	struct FFindOrCreateTrackResult
	{
		UMovieSceneTrack* Track;
		bool bWasCreated;
	};

	FFindOrCreateTrackResult FindOrCreateTrackForObject( const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName = NAME_None, bool bCreateTrackIfMissing = true )
	{
		FFindOrCreateTrackResult Result;
		bool bTrackExisted;

		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		Result.Track = MovieScene->FindTrack( TrackClass, ObjectHandle, PropertyName );
		bTrackExisted = Result.Track != nullptr;

		if (!Result.Track && bCreateTrackIfMissing)
		{
			Result.Track = AddTrack(MovieScene, ObjectHandle, TrackClass, PropertyName);
		}

		Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;

		return Result;
	}

	template<typename TrackClass>
	struct FFindOrCreateMasterTrackResult
	{
		TrackClass* Track;
		bool bWasCreated;
	};

	/**
	 * Find or add a master track of the specified type in the focused movie scene.
	 *
	 * @param TrackClass The class of the track to find or add.
	 * @return The track results.
	 */
	template<typename TrackClass>
	FFindOrCreateMasterTrackResult<TrackClass> FindOrCreateMasterTrack()
	{
		FFindOrCreateMasterTrackResult<TrackClass> Result;
		bool bTrackExisted;

		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		Result.Track = MovieScene->FindMasterTrack<TrackClass>();
		bTrackExisted = Result.Track != nullptr;

		if (Result.Track == nullptr)
		{
			Result.Track = MovieScene->AddMasterTrack<TrackClass>();
		}

		Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;
		return Result;
	}


	/** @return The sequencer bound to this handler */
	const TSharedPtr<ISequencer> GetSequencer() const
	{
		return Sequencer.Pin();
	}

public:

	// ISequencerTrackEditor interface

	virtual void AddKey( const FGuid& ObjectGuid ) override {}

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override
	{
		return FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
	}

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) override { }
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override { }
	virtual void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) override { }
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override { }
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override  { return TSharedPtr<SWidget>(); }
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override { }
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override { return false; }

	virtual bool IsAllowedKeyAll() const
	{
		return Sequencer.Pin()->GetKeyAllEnabled();
	}

	virtual bool IsAllowedToAutoKey() const
	{
		// @todo sequencer livecapture: This turns on "auto key" for the purpose of capture keys for actor state
		// during PIE sessions when record mode is active.
		return Sequencer.Pin()->IsRecordingLive() || Sequencer.Pin()->GetAutoKeyMode() != EAutoKeyMode::KeyNone;
	}

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(class UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) = 0;
	virtual void OnInitialize() override { };
	virtual void OnRelease() override { };

	virtual int32 PaintTrackArea(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle) override
	{
		return LayerId;
	}

	virtual bool SupportsType( TSubclassOf<class UMovieSceneTrack> TrackClass ) const = 0;
	virtual void Tick(float DeltaTime) override { }

protected:

	/**
	 * Gets the currently focused movie scene, if any.
	 *
	 * @return Focused movie scene, or nullptr if no movie scene is focused.
	 */
	UMovieScene* GetFocusedMovieScene() const
	{
		UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
		return FocusedSequence->GetMovieScene();
	}

private:

	/** The sequencer bound to this handler.  Used to access movie scene and time info during auto-key */
	TWeakPtr<ISequencer> Sequencer;
};
