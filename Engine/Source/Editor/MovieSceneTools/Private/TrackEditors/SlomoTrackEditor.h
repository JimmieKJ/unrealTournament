// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FloatPropertyTrackEditor.h"


class ISequencer;
class UMovieSceneTrack;


/**
* A property track editor for slow motion control.
*/
class FSlomoTrackEditor
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
	FSlomoTrackEditor(TSharedRef<ISequencer> InSequencer);	

public:

	// ISequencerTrackEditor interface

	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;

private:

	/** Callback for executing the "Add Slomo Track" menu entry. */
	void HandleAddSlomoTrackMenuEntryExecute();
};
