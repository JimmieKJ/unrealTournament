// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneActorReferenceTrack.h"
#include "MovieSceneActorReferenceSection.h"
#include "PropertyTrackEditor.h"


class ISequencer;


/**
 * A property track editor for actor references.
 */
class FActorReferencePropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneActorReferenceTrack, UMovieSceneActorReferenceSection, FGuid>
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FActorReferencePropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FPropertyTrackEditor(InSequencer, NAME_ObjectProperty)
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
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FGuid>& NewGeneratedKeys, TArray<FGuid>& DefaultGeneratedKeys) override;
};
