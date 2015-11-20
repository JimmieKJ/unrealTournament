// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrackEditor.h"
#include "IKeyframeSection.h"
#include "ISequencerObjectChangeListener.h"
#include "MovieSceneCommonHelpers.h"

class IPropertyHandle;
class FPropertyChangedParams;

/**
 * A base class for track editors that edit tracks which contain sections implementing IKeyframeSection.
  */
template<typename TrackType, typename SectionType, typename KeyDataType>
class FKeyframeTrackEditor : public FMovieSceneTrackEditor
{

public:
	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	*/
	FKeyframeTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FMovieSceneTrackEditor( InSequencer )
	{ }


	~FKeyframeTrackEditor()
	{
	}

public:

	// ISequencerTrackEditor interface

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override
	{
		return Type == TrackType::StaticClass();
	}


protected:

	DECLARE_DELEGATE_OneParam( FOnInitializeNewTrack, TrackType* );
	DECLARE_DELEGATE_OneParam( FOnSetIntermediateValue, UMovieSceneTrack* );
	
	/*
	 * Adds keys to the specified object.  This may also add tracks and sections depending on the options specified. 
	 *
	 * @param ObjectsToKey An array of objects to add keyframes to.
	 * @param KeyTime The time to add keys.
	 * @param NewKeys The new keys to add.
	 * @param KeyParams The parameters to control keyframing behavior.
	 * @param TrackClass The class of track which should be created if specified in the parameters.
	 * @param PropertyName The name of the property to add keys for.
	 * @param OnInitializeNewTrack A delegate which allows for custom initialization for new tracks.  This is called after the 
	 *        track is created, but before any sections or keys have been added.
	 * @param OnSetIntermediateValue A delegate which is called when the key params specify that keying should only happen when
	 * 		  auto-keying is on, but auto-keying is off.  This allows handling the changed but not keyed intermediate value.
	 */
	bool AddKeysToObjects(TArray<UObject*> ObjectsToKey, float KeyTime, const TArray<KeyDataType> NewKeys, FKeyParams KeyParams,
		TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName, FOnInitializeNewTrack OnIntializeNewTrack, FOnSetIntermediateValue OnSetIntermediateValue )
	{
		bool bHandleCreated = false;
		bool bTrackCreatedOrModified = false;

		for ( UObject* Object : ObjectsToKey )
		{
			FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object, KeyParams.bCreateHandleIfMissing );
			FGuid ObjectHandle = HandleResult.Handle;
			bHandleCreated = HandleResult.bWasCreated;

			if ( ObjectHandle.IsValid() )
			{
				bTrackCreatedOrModified |= AddKeysToHandle( ObjectHandle, KeyTime, NewKeys, KeyParams, TrackClass, PropertyName, OnIntializeNewTrack, OnSetIntermediateValue );
			}
		}
		return bHandleCreated || bTrackCreatedOrModified;
	}


private:

	bool AddKeysToHandle( FGuid ObjectHandle, float KeyTime, const TArray<KeyDataType> NewKeys, FKeyParams KeyParams, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName, FOnInitializeNewTrack OnIntializeNewTrack, FOnSetIntermediateValue OnSetIntermediateValue )
	{
		bool bTrackCreated = false;
		bool bTrackModified = false;

		// Try to find an existing Track, and if one doesn't exist check the key params and create one if requested.
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject( ObjectHandle, TrackClass, PropertyName, KeyParams.bCreateTrackIfMissing );
		TrackType* Track = CastChecked<TrackType>( TrackResult.Track, ECastCheckedType::NullAllowed );
		bTrackCreated = TrackResult.bWasCreated;

		if ( bTrackCreated )
		{
			OnIntializeNewTrack.ExecuteIfBound( Track );
		}

		if ( Track )
		{
			bTrackModified |= AddKeysToTrack( Track, KeyTime, NewKeys, KeyParams, OnSetIntermediateValue );
		}

		return bTrackCreated || bTrackModified;
	}

	bool AddKeysToTrack( TrackType* Track, float KeyTime, const TArray<KeyDataType> NewKeys, FKeyParams KeyParams, FOnSetIntermediateValue OnSetIntermediateValue )
	{
		bool bTrackModified = false;

		bool bSettingIntermediateValue = KeyParams.bCreateKeyOnlyWhenAutoKeying && GetSequencer()->GetAutoKeyEnabled() == false;
		if ( bSettingIntermediateValue )
		{
			checkf( OnSetIntermediateValue.IsBound(), TEXT( "A valid OnSetIntermediateValue delegate must be provided for consistent keyframing behavior." ) );
			OnSetIntermediateValue.Execute(Track);
		}
		else
		{
			EMovieSceneKeyInterpolation InterpolationMode = GetSequencer()->GetKeyInterpolation();
			for ( const KeyDataType& NewKey : NewKeys )
			{
				// Only modify the track if the new key will create new data, or if creating keys has been forced.
				if ( NewKeyIsNewData( Track, KeyTime, NewKey ) || KeyParams.bCreateKeyIfUnchanged )
				{
					// Only add a key if the track already has keys, or if creating keys has been forced.  Otherwise just update the track default.
					if ( HasKeys( Track, NewKey ) || KeyParams.bCreateKeyIfEmpty )
					{
						AddKey( Track, KeyTime, NewKey, InterpolationMode );
					}
					else
					{
						SetDefault( Track, KeyTime, NewKey );
					}
					bTrackModified = true;
				}
			}
		}
		return bTrackModified;
	}

	bool NewKeyIsNewData( TrackType* Track, float Time, const KeyDataType& KeyData ) const
	{
		IKeyframeSection<KeyDataType>* NearestKeyframeSection = CastChecked<SectionType>( MovieSceneHelpers::FindNearestSectionAtTime( Track->GetAllSections(), Time ), ECastCheckedType::NullAllowed );
		return NearestKeyframeSection == nullptr || NearestKeyframeSection->NewKeyIsNewData( Time, KeyData );
	}

	bool HasKeys( TrackType* Track, const KeyDataType& KeyData ) const
	{
		for ( UMovieSceneSection* Section : Track->GetAllSections() )
		{
			IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( Section );
			if ( KeyframeSection->HasKeys( KeyData ) )
			{
				return true;
			}
		}
		return false;
	}

	using FMovieSceneTrackEditor::AddKey;

	void AddKey( TrackType* Track, float Time, const KeyDataType& KeyData, EMovieSceneKeyInterpolation KeyInterpolation )
	{
		Track->Modify();
		IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( Track->FindOrAddSection( Time ) );
		KeyframeSection->AddKey( Time, KeyData, KeyInterpolation );
	}

	void SetDefault( TrackType* Track, float Time, const KeyDataType& KeyData )
	{
		Track->Modify();
		const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
		if ( Sections.Num() > 0)
		{
			for ( UMovieSceneSection* Section : Sections )
			{
				IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( Section );
				KeyframeSection->SetDefault( KeyData );
			}
		}
		else
		{
			IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( Track->FindOrAddSection( Time ) );
			KeyframeSection->SetDefault( KeyData );
		}
	}
};


