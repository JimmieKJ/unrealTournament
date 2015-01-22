// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/Sequencer/Public/MovieSceneTrackEditor.h"

class IPropertyHandle;

class F2DTransformTrackEditor : public FMovieSceneTrackEditor
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer	The sequencer instance to be used by this tool
	 */
	F2DTransformTrackEditor( TSharedRef<ISequencer> InSequencer );
	~F2DTransformTrackEditor();

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

private:
	/**
	 * Called by the details panel when an animatable property changes
	 *
	 * @param InObjectsThatChanged	List of objects that changed
	 * @param PropertyValue			Handle to the property value which changed
	 */
	void OnTransformChanged( const struct FKeyPropertyParams& PropertyKeyParams );

	/** Called After OnMarginChanged if we actually can key the margin */
	void OnKeyTransform( float KeyTime, const struct FKeyPropertyParams* PropertyKeyParams );
};


