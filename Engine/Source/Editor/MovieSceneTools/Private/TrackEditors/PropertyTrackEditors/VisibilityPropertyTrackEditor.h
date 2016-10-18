// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneVisibilitySection.h"
#include "MovieSceneVisibilityTrack.h"
#include "PropertyTrackEditor.h"


class ISequencer;


/**
 * A property track for bool properties which have been set
 * to use a UMovieSceneVisibilityTrack through metadata.
 */
class FVisibilityPropertyTrackEditor
	: public FPropertyTrackEditor<UMovieSceneVisibilityTrack, UMovieSceneVisibilitySection, bool>
{
public:

	/** Constructor. */
	FVisibilityPropertyTrackEditor(TSharedRef<ISequencer> InSequencer)
		// Don't supply any property type names to watch since the FBoolPropertyTrackEditor is already watching for bool property changes.
		: FPropertyTrackEditor<UMovieSceneVisibilityTrack, UMovieSceneVisibilitySection, bool>(InSequencer, NAME_BoolProperty)
	{ }

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:

	//~ FPropertyTrackEditor interface

	virtual TSharedRef<FPropertySection> MakePropertySectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) override;
	virtual void GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<bool>& NewGeneratedKeys, TArray<bool>& DefaultGeneratedKeys) override;
	virtual bool ForCustomizedUseOnly() { return true; }
};
