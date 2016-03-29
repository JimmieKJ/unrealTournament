// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyTrackEditor.h"
#include "MovieScene2DTransformTrack.h"
#include "MovieScene2DTransformSection.h"


class F2DTransformTrackEditor
	: public FPropertyTrackEditor<UMovieScene2DTransformTrack, UMovieScene2DTransformSection, F2DTransformKey>
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	F2DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FPropertyTrackEditor<UMovieScene2DTransformTrack, UMovieScene2DTransformSection, F2DTransformKey>( InSequencer, "WidgetTransform" )
	{ }

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

protected:

	// FPropertyTrackEditor interface

	virtual TSharedRef<FPropertySection> MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track ) override;

	virtual void GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<F2DTransformKey>& NewGeneratedKeys, TArray<F2DTransformKey>& DefaultGeneratedKeys ) override;

private:
	static FName TranslationName;
	static FName ScaleName;
	static FName ShearName;
	static FName AngleName;
};
