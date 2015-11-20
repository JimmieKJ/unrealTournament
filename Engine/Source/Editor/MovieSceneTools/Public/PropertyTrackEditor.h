// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "PropertySection.h"
#include "ISequencerObjectChangeListener.h"

class IPropertyHandle;
class FPropertyChangedParams;

/**
* Tools for animatable property types such as floats ands vectors
*/
template<typename TrackType, typename SectionType, typename KeyDataType>
class FPropertyTrackEditor
	: public FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>
{
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams( FOnSetIntermediateValueFromPropertyChange, UMovieSceneTrack*, FPropertyChangedParams )

public:
	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	*/
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{ }

	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	* @param WatchedPropertyTypeName The property type name that this property track editor should watch for changes.
	*/
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, FName WatchedPropertyTypeName )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		AddWatchedPropertyType( WatchedPropertyTypeName );
	}

	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	* @param WatchedPropertyTypeName1 The first property type name that this property track editor should watch for changes.
	* @param WatchedPropertyTypeName2 The second property type name that this property track editor should watch for changes.
	*/
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, FName WatchedPropertyTypeName1, FName WatchedPropertyTypeName2 )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		AddWatchedPropertyType( WatchedPropertyTypeName1 );
		AddWatchedPropertyType( WatchedPropertyTypeName2 );
	}

	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	* @param WatchedPropertyTypeName1 The first property type name that this property track editor should watch for changes.
	* @param WatchedPropertyTypeName2 The second property type name that this property track editor should watch for changes.
	* @param WatchedPropertyTypeName3 The third property type name that this property track editor should watch for changes.
	*/
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, FName WatchedPropertyTypeName1, FName WatchedPropertyTypeName2, FName WatchedPropertyTypeName3 )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		AddWatchedPropertyType( WatchedPropertyTypeName1 );
		AddWatchedPropertyType( WatchedPropertyTypeName2 );
		AddWatchedPropertyType( WatchedPropertyTypeName3 );
	}

	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	* @param WatchedPropertyTypeNameS An array of property type names that this property track editor should watch for changes.
	*/
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer, TArray<FName> InWatchedPropertyTypeNames )
		: FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>( InSequencer )
	{
		for ( FName WatchedPropertyTypeName : InWatchedPropertyTypeNames )
		{
			AddWatchedPropertyType( WatchedPropertyTypeName );
		}
	}

	~FPropertyTrackEditor()
	{
		TSharedPtr<ISequencer> Sequencer = this->GetSequencer();
		if ( Sequencer.IsValid() )
		{
			ISequencerObjectChangeListener& ObjectChangeListener = Sequencer->GetObjectChangeListener();
			for ( FName WatchedPropertyTypeName : WatchedPropertyTypeNames )
			{
				ObjectChangeListener.GetOnAnimatablePropertyChanged( WatchedPropertyTypeName ).RemoveAll( this );
			}
		}
	}

public:

	// ISequencerTrackEditor interface

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override
	{
		return Type == TrackType::StaticClass();
	}

	virtual void AddKey( const FGuid& ObjectGuid, UObject* AdditionalAsset = NULL ) override
	{
		TSharedPtr<ISequencer> Sequencer = this->GetSequencer();
		ISequencerObjectChangeListener& ObjectChangeListener = Sequencer->GetObjectChangeListener();

		TArray<UObject*> OutObjects;
		Sequencer->GetRuntimeObjects( Sequencer->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects );
		for ( int32 i = 0; i < OutObjects.Num(); ++i )
		{
			ObjectChangeListener.TriggerAllPropertiesChanged( OutObjects[i] );
		}
	}

	virtual TSharedRef<ISequencerSection> MakeSectionInterface( class UMovieSceneSection& SectionObject, UMovieSceneTrack& Track ) override
	{
		TSharedRef<FPropertySection> PropertySection = MakePropertySectionInterface( SectionObject, Track );
		OnSetIntermediateValueFromPropertyChange.AddSP( PropertySection, &FPropertySection::SetIntermediateValueForTrack );
		this->GetSequencer()->OnGlobalTimeChanged().AddSP( PropertySection, &FPropertySection::ClearIntermediateValue );
		return PropertySection;
	}

protected:

	/** Creates a property section for a section object. */
	virtual TSharedRef<FPropertySection> MakePropertySectionInterface( class UMovieSceneSection& SectionObject, UMovieSceneTrack& Track ) = 0;

	/**
	* Generates a new key who's value is the result of the supplied property change.
	*
	* @param PropertyChangedParams Parameters associated with the property change.
	* @return The new key.
	*/
	virtual void GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<KeyDataType>& GeneratedKeys ) = 0;

	/** When true, this track editor will only be used on properties which have specified it as a custom track class. This is necessary to prevent duplicate
		property change handling in cases where a custom track editor handles the same type of data as one of the standard track editors. */
	virtual bool ForCustomizedUseOnly() { return false; }

	/** 
	 * Initialized values on a track after it's been created, but before any sections or keys have been added.
	 * @param NewTrack The newly created track.
	 * @param PropertyChangedParams The property change parameters which caused this track to be created.
	 */
	virtual void InitializeNewTrack( TrackType* NewTrack, FPropertyChangedParams PropertyChangedParams )
	{
		NewTrack->SetPropertyNameAndPath( PropertyChangedParams.PropertyPath.Last()->GetFName(), PropertyChangedParams.GetPropertyPathString() );
	}

private:
	/** Adds a callback for property changes for the supplied property type name. */
	void AddWatchedPropertyType( FName WatchedPropertyTypeName )
	{
		this->GetSequencer()->GetObjectChangeListener().GetOnAnimatablePropertyChanged( WatchedPropertyTypeName ).AddRaw( this, &FPropertyTrackEditor<TrackType, SectionType, KeyDataType>::OnAnimatedPropertyChanged );
		WatchedPropertyTypeNames.Add( WatchedPropertyTypeName );
	}

	/** Get a customized track class from the property if there is one, otherwise return nullptr. */
	TSubclassOf<UMovieSceneTrack> GetCustomizedTrackClass( const UProperty* Property )
	{
		// Look for a customized track class for this property on the meta data
		const FString& MetaSequencerTrackClass = Property->GetMetaData( TEXT( "SequencerTrackClass" ) );
		if ( !MetaSequencerTrackClass.IsEmpty() )
		{
			UClass* MetaClass = FindObject<UClass>( ANY_PACKAGE, *MetaSequencerTrackClass );
			if ( !MetaClass )
			{
				MetaClass = LoadObject<UClass>( nullptr, *MetaSequencerTrackClass );
			}
			return MetaClass;
		}
		return nullptr;
	}

	/**
	* Called by the details panel when an animatable property changes
	*
	* @param InObjectsThatChanged	List of objects that changed
	* @param KeyPropertyParams		Parameters for the property change.
	*/
	virtual void OnAnimatedPropertyChanged( const FPropertyChangedParams& PropertyChangedParams )
	{
		this->AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FPropertyTrackEditor::OnKeyProperty, PropertyChangedParams ) );
	}

	/** Adds a key based on a property change. */
	bool OnKeyProperty( float KeyTime, FPropertyChangedParams PropertyChangedParams )
	{
		TArray<KeyDataType> KeysForPropertyChange;
		GenerateKeysFromPropertyChanged( PropertyChangedParams, KeysForPropertyChange );

		TSubclassOf<UMovieSceneTrack> CustomizedClass = GetCustomizedTrackClass( PropertyChangedParams.PropertyPath.Last() );
		TSubclassOf<UMovieSceneTrack> TrackClass = (*CustomizedClass != nullptr ? *CustomizedClass : TrackType::StaticClass());

		// If the track class has been customized for this property then it's possible this track editor doesn't support it, 
		// also check for track editors which should only be used for customization.
		if ( SupportsType( TrackClass ) && ( ForCustomizedUseOnly() == false || *CustomizedClass != nullptr) )
		{
			typename FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>::FOnInitializeNewTrack OnInitializeNewTrack;
			OnInitializeNewTrack.BindLambda( [&](TrackType* NewTrack) { InitializeNewTrack( NewTrack, PropertyChangedParams ); } );

			typename FKeyframeTrackEditor<TrackType, SectionType, KeyDataType>::FOnSetIntermediateValue OnSetIntermediateValue;
			OnSetIntermediateValue.BindSP( this, &FPropertyTrackEditor::SetIntermediateValueFromPropertyChange, PropertyChangedParams );

			return this->AddKeysToObjects( PropertyChangedParams.ObjectsThatChanged, KeyTime, KeysForPropertyChange, PropertyChangedParams.KeyParams,
				TrackClass, PropertyChangedParams.PropertyPath.Last()->GetFName(), OnInitializeNewTrack, OnSetIntermediateValue );
		}
		else
		{
			return false;
		}
	}

private:

	void SetIntermediateValueFromPropertyChange(  UMovieSceneTrack* Track, FPropertyChangedParams PropertyChangedParams )
	{
		OnSetIntermediateValueFromPropertyChange.Broadcast( Track, PropertyChangedParams );
	}

private:

	/** An array of property type names which are being watched for changes. */
	TArray<FName> WatchedPropertyTypeNames;

	FOnSetIntermediateValueFromPropertyChange OnSetIntermediateValueFromPropertyChange;
};