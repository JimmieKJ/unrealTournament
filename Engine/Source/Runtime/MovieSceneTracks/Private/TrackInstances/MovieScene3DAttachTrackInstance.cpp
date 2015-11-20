// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieScene3DAttachTrackInstance.h"
#include "MovieScene3DAttachTrack.h"
#include "MovieScene3DAttachSection.h"
#include "IMovieScenePlayer.h"


FMovieScene3DAttachTrackInstance::FMovieScene3DAttachTrackInstance( UMovieScene3DAttachTrack& InAttachTrack )
	: FMovieScene3DConstraintTrackInstance( InAttachTrack )
{ }


void FMovieScene3DAttachTrackInstance::UpdateConstraint( float Position, const TArray<UObject*>& RuntimeObjects, AActor* Actor, UMovieScene3DConstraintSection* ConstraintSection ) 
{
	FVector Translation;
	FRotator Rotation;

	UMovieScene3DAttachSection* AttachSection = CastChecked<UMovieScene3DAttachSection>(ConstraintSection);

	for( int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex )
	{
		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(RuntimeObjects[ObjIndex]);

		if (SceneComponent != nullptr)
		{
			AttachSection->Eval(SceneComponent, Position, Actor, Translation, Rotation);

			// Set the relative translation and rotation.  Note they are set once instead of individually to avoid a redundant component transform update.
			SceneComponent->SetRelativeLocationAndRotation(Translation, Rotation);
		}
	}
}
