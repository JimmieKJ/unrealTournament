// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoolPropertyTrackEditor.h"


class ISequencer;
class UMovieSceneTrack;


/**
 * A property track editor for controlling the lifetime of a sapwnable object
 */
class FSpawnTrackEditor
	: public FBoolPropertyTrackEditor
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
	FSpawnTrackEditor(TSharedRef<ISequencer> InSequencer);

public:

	// ISequencerTrackEditor interface

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track) override  { return TSharedPtr<SWidget>(); }
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override { return false; }
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;

private:

	/** Callback for executing the "Add Spawn Track" menu entry. */
	void HandleAddSpawnTrackMenuEntryExecute(FGuid ObjectBinding);
	bool CanAddSpawnTrack(FGuid ObjectBinding) const;
};
