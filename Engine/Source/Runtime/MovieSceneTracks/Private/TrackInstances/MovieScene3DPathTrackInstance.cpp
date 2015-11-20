// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieScene3DPathTrackInstance.h"
#include "MovieScene3DPathTrack.h"
#include "MovieScene3DPathSection.h"
#include "IMovieScenePlayer.h"
#include "Components/SplineComponent.h"


FMovieScene3DPathTrackInstance::FMovieScene3DPathTrackInstance( UMovieScene3DPathTrack& InPathTrack )
	: FMovieScene3DConstraintTrackInstance( InPathTrack )
{ }


void FMovieScene3DPathTrackInstance::UpdateConstraint( float Position, const TArray<UObject*>& RuntimeObjects, AActor* Actor, UMovieScene3DConstraintSection* ConstraintSection ) 
{
	FVector Translation;
	FRotator Rotation;

	TArray<USplineComponent*> SplineComponents;
	Actor->GetComponents(SplineComponents);

	if (SplineComponents.Num())
	{
		UMovieScene3DPathSection* PathSection = CastChecked<UMovieScene3DPathSection>(ConstraintSection);

		for( int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex )
		{
			USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(RuntimeObjects[ObjIndex]);

			if (SceneComponent != nullptr)
			{
				PathSection->Eval(SceneComponent, Position, SplineComponents[0], Translation, Rotation);

				// Set the relative translation and rotation.  Note they are set once instead of individually to avoid a redundant component transform update.
				SceneComponent->SetRelativeLocationAndRotation(Translation, Rotation);
			}
		}
	}
}
