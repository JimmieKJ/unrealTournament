// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneVisibilityTrack.h"
#include "VisibilityPropertyTrackEditor.h"
#include "VisibilityPropertySection.h"


TSharedRef<ISequencerTrackEditor> FVisibilityPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FVisibilityPropertyTrackEditor(OwningSequencer));
}


TSharedRef<FPropertySection> FVisibilityPropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FVisibilityPropertySection(SectionObject, Track.GetDisplayName(), GetSequencer().Get()));
}


void FVisibilityPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<bool>& GeneratedKeys )
{
	GeneratedKeys.Add(!PropertyChangedParams.GetPropertyValue<bool>());
}
