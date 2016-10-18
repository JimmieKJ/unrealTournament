// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FloatPropertyTrackEditor.h"


class ISequencer;
class UMovieSceneTrack;


/**
* A property track editor for fade control.
*/
class FFadeTrackEditor
	: public FFloatPropertyTrackEditor
{
public:

	/**
	 * Factory function to create an instance of this class (called by a sequencer).
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FFadeTrackEditor(TSharedRef<ISequencer> InSequencer);	

public:

	//~ FPropertyTrackEditor interface

	virtual TSharedRef<FPropertySection> MakePropertySectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track) override;

	// ISequencerTrackEditor interface

	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual const FSlateBrush* GetIconBrush() const override;

private:

	/** Callback for executing the "Add Fade Track" menu entry. */
	void HandleAddFadeTrackMenuEntryExecute();
};
