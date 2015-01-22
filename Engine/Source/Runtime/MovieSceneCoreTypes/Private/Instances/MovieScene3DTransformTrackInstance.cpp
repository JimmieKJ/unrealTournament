// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieScene3DTransformTrackInstance.h"


FMovieScene3DTransformTrackInstance::FMovieScene3DTransformTrackInstance( UMovieScene3DTransformTrack& InTransformTrack )
{
	TransformTrack = &InTransformTrack;
}

void FMovieScene3DTransformTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	FVector Translation;
	FRotator Rotation;
	FVector Scale;
	bool bHasTranslationKeys = false;
	bool bHasRotationKeys = false;
	bool bHasScaleKeys = false;

	if( TransformTrack->Eval( Position, LastPosition, Translation, Rotation, Scale, bHasTranslationKeys, bHasRotationKeys, bHasScaleKeys ) )
	{
		for( int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex )
		{
			UObject* Object = RuntimeObjects[ObjIndex];

			AActor* Actor = Cast<AActor>( Object );

			USceneComponent* SceneComponent = NULL;
			if( Actor && Actor->GetRootComponent() )
			{
				// If there is an actor, modify its root component
				SceneComponent = Actor->GetRootComponent();
			}
			else
			{
				// No actor was found.  Attempt to get the object as a component in the case that we are editing them directly.
				SceneComponent = Cast<USceneComponent>( Object );
			}

			// Set the relative translation and rotation.  Note they are set once instead of individually to avoid a redundant component transform update.
			SceneComponent->SetRelativeLocationAndRotation(
				bHasTranslationKeys ? Translation : SceneComponent->RelativeLocation,
				bHasRotationKeys ? Rotation : SceneComponent->RelativeRotation );

			if( bHasScaleKeys )
			{
				SceneComponent->SetRelativeScale3D( Scale );
			}
		}
	}
}

