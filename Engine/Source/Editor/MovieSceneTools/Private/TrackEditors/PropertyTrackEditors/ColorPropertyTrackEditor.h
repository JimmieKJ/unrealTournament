// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneColorSection.h"
#include "MovieSceneColorTrack.h"
#include "PropertyTrackEditor.h"


class ISequencer;


/**
* A property track editor for colors.
*/
class FColorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneColorTrack, UMovieSceneColorSection, FColorKey>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FColorPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor<UMovieSceneColorTrack, UMovieSceneColorSection, FColorKey>(InSequencer, NAME_Color, NAME_LinearColor, "SlateColor")
	{ }

	/**
	 * Creates an instance of this class (called by a sequencer).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:

	//~ FPropertyTrackEditor interface

	virtual TSharedRef<FPropertySection> MakePropertySectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) override;
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FColorKey>& NewGeneratedKeys, TArray<FColorKey>& DefaultGeneratedKeys) override;

protected:

	//~ ISequencerTrackEditor interface

	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;

private:

	static FName RedName;
	static FName GreenName;
	static FName BlueName;
	static FName AlphaName;
};
