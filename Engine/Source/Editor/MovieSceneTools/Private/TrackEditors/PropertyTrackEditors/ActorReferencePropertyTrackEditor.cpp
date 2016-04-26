// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ActorReferencePropertyTrackEditor.h"
#include "ActorReferencePropertySection.h"
#include "MatineeImportTools.h"


TSharedRef<ISequencerTrackEditor> FActorReferencePropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FActorReferencePropertyTrackEditor(OwningSequencer));
}


TSharedRef<FPropertySection> FActorReferencePropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FActorReferencePropertySection(SectionObject, Track.GetDisplayName()));
}


void FActorReferencePropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FGuid>& NewGeneratedKeys, TArray<FGuid>& DefaultGeneratedKeys )
{

	AActor* NewReferencedActor = PropertyChangedParams.GetPropertyValue<AActor*>();
	if ( NewReferencedActor != nullptr )
	{
		FGuid ActorGuid = GetSequencer()->GetHandleToObject( NewReferencedActor );
		if ( ActorGuid.IsValid() )
		{
			NewGeneratedKeys.Add( ActorGuid );
		}
	}
}
