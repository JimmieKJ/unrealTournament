// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneBoolTrack.h"
#include "BoolPropertyTrackEditor.h"
#include "BoolPropertySection.h"


TSharedRef<ISequencerTrackEditor> FBoolPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShareable(new FBoolPropertyTrackEditor(OwningSequencer));
}


TSharedRef<FPropertySection> FBoolPropertyTrackEditor::MakePropertySectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track)
{
	return MakeShareable(new FBoolPropertySection(SectionObject, Track.GetDisplayName(), GetSequencer().Get()));
}


void FBoolPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<bool>& NewGeneratedKeys, TArray<bool>& DefaultGeneratedKeys )
{
	NewGeneratedKeys.Add( PropertyChangedParams.GetPropertyValue<bool>() );
}
