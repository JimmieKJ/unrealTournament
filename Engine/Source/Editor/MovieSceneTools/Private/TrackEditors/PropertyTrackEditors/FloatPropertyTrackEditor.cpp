// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "FloatPropertyTrackEditor.h"
#include "FloatPropertySection.h"


TSharedRef<ISequencerTrackEditor> FFloatPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FFloatPropertyTrackEditor(OwningSequencer));
}


TSharedRef<FPropertySection> FFloatPropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FFloatPropertySection(SectionObject, Track.GetDisplayName()));
}


void FFloatPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<float>& GeneratedKeys )
{
	GeneratedKeys.Add( PropertyChangedParams.GetPropertyValue<float>() );
}
