// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneColorTrack.h"
#include "MovieSceneColorSection.h"


/**
* A property track editor for colors.
*/
class FColorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneColorTrack, UMovieSceneColorSection, FColorKey>
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FColorPropertyTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FPropertyTrackEditor<UMovieSceneColorTrack, UMovieSceneColorSection, FColorKey>( InSequencer, NAME_Color, NAME_LinearColor, "SlateColor" )
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
	virtual void GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FColorKey>& NewGeneratedKeys, TArray<FColorKey>& DefaultGeneratedKeys ) override;

private:
	static FName RedName;
	static FName GreenName;
	static FName BlueName;
	static FName AlphaName;
};
