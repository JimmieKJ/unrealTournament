// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneVectorSection.h"
#include "MovieSceneVectorTrack.h"
#include "PropertyTrackEditor.h"


class ISequencer;


/**
 * A property track editor for vectors.
 */
class FVectorPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneVectorTrack, UMovieSceneVectorSection, FVectorKey>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FVectorPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor<UMovieSceneVectorTrack, UMovieSceneVectorSection, FVectorKey>(InSequencer, NAME_Vector, NAME_Vector4, NAME_Vector2D)
	{ }

	/**
	 * Creates an instance of this class (called by a sequence).
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:

	//~ FPropertyTrackEditor interface

	virtual TSharedRef<FPropertySection> MakePropertySectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) override;
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FVectorKey>& NewGeneratedKeys, TArray<FVectorKey>& DefaultGeneratedKeys) override;
	virtual void InitializeNewTrack(UMovieSceneVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams) override;

private:

	static FName XName;
	static FName YName;
	static FName ZName;
	static FName WName;
};
