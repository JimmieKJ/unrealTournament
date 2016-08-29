// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneStringTrack.h"
#include "MovieSceneStringSection.h"
#include "PropertyTrackEditor.h"


class ISequencer;


/**
 * A property track editor for strings.
 */
class FStringPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneStringTrack, UMovieSceneStringSection, FString>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FStringPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor(InSequencer, NAME_StrProperty)
	{ }

	/**
	 * Creates an instance of this class (called by a sequencer).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:

	//~ FPropertyTrackEditor interface

	virtual TSharedRef<FPropertySection> MakePropertySectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) override;
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FString>& NewGeneratedKeys, TArray<FString>& DefaultGeneratedKeys) override;
};
