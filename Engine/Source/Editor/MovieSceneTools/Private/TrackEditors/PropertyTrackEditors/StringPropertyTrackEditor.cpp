// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "StringPropertyTrackEditor.h"
#include "StringPropertySection.h"


TSharedRef<ISequencerTrackEditor> FStringPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FStringPropertyTrackEditor(OwningSequencer));
}


TSharedRef<FPropertySection> FStringPropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FStringPropertySection(SectionObject, Track.GetDisplayName()));
}


void FStringPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FString>& NewGeneratedKeys, TArray<FString>& DefaultGeneratedKeys )
{
	void* CurrentObject = PropertyChangedParams.ObjectsThatChanged[0];
	void* PropertyValue = nullptr;
	for (int32 i = 0; i < PropertyChangedParams.PropertyPath.Num(); i++)
	{
		CurrentObject = PropertyChangedParams.PropertyPath[i]->ContainerPtrToValuePtr<FString>(CurrentObject, 0);
	}

	const UStrProperty* StrProperty = Cast<const UStrProperty>( PropertyChangedParams.PropertyPath.Last() );
	if ( StrProperty )
	{
		FString StrPropertyValue = StrProperty->GetPropertyValue(CurrentObject);
		NewGeneratedKeys.Add(StrPropertyValue);
	}
}
