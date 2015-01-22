// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IPropertyHandle;

struct FKeyPropertyParams;

/**
 * Tools for animatable property types such as floats ands vectors
 */
class FPropertyTrackEditor : public FMovieSceneTrackEditor
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer	The sequencer instance to be used by this tool
	 */
	FPropertyTrackEditor( TSharedRef<ISequencer> InSequencer );
	~FPropertyTrackEditor();

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<FMovieSceneTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

	/** FMovieSceneTrackEditor Interface */
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track ) override;
	virtual void AddKey(const FGuid& ObjectGuid, UObject* AdditionalAsset = NULL) override;

private:
	/**
	 * Called by the details panel when an animatable property changes
	 *
	 * @param InObjectsThatChanged	List of objects that changed
	 * @param PropertyValue			Handle to the property value which changed
	 */
	template <typename Type, typename TrackType>
	void OnAnimatedPropertyChanged( const FKeyPropertyParams& KeyPropertyParams );

	/**
	 * Called by the details panel when an animatable vector property changes
	 *
	 * @param InObjectsThatChanged	List of objects that changed
	 * @param PropertyValue			Handle to the property value which changed
	 */
	void OnAnimatedVectorPropertyChanged( const FKeyPropertyParams& KeyPropertyParams );
	
	/**
	 * Called by the details panel when an animatable color property changes
	 *
	 * @param InObjectsThatChanged	List of objects that changed
	 * @param PropertyValue			Handle to the property value which changed
	 */
	void OnAnimatedColorPropertyChanged( const FKeyPropertyParams& KeyPropertyParams );
	
	/** Delegates for animatable property changed functions */
	template <typename Type, typename TrackType>
	void OnKeyProperty( float KeyTime, FKeyPropertyParams KeyPropertyParams, Type Value );
};


