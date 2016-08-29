// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialTrackEditor.h"

class UMovieSceneMaterialTrack;

/**
 * A specialized material track editor for widget materials
 */
class FWidgetMaterialTrackEditor
	: public FMaterialTrackEditor
{
public:

	FWidgetMaterialTrackEditor( TSharedRef<ISequencer> InSequencer );

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;

protected:

	// FMaterialtrackEditor interface

	virtual UMaterialInterface* GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack ) override;
};
