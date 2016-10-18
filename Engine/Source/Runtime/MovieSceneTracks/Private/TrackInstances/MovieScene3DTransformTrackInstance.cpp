// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieScene3DTransformTrackInstance.h"
#include "MovieScene3DTransformTrack.h"

FMovieScene3DTransformTrackInstance::FMovieScene3DTransformTrackInstance( UMovieScene3DTransformTrack& InTransformTrack )
{
	TransformTrack = &InTransformTrack;
}


void FMovieScene3DTransformTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex)
	{
		UObject* Object = RuntimeObjects[ObjIndex].Get();
		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Object);

		if (SceneComponent != nullptr)
		{
			if (InitTransformMap.Find(Object) == nullptr)
			{
				InitTransformMap.Add(Object, SceneComponent->GetRelativeTransform());
			}
			if (InitMobilityMap.Find(Object) == nullptr)
			{
				InitMobilityMap.Add(Object, SceneComponent->Mobility);
			}
		}
	}
}


void FMovieScene3DTransformTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex)
	{
		UObject* Object = RuntimeObjects[ObjIndex].Get();

		if (!IsValid(Object))
		{
			continue;
		}

		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Object);
		if (SceneComponent != nullptr)
		{
			FTransform *Transform = InitTransformMap.Find(Object);
			if (Transform != nullptr)
			{
				SceneComponent->SetRelativeTransform(*Transform);
			}

			EComponentMobility::Type* ComponentMobility = InitMobilityMap.Find(Object);
			if (ComponentMobility != nullptr)
			{
				SceneComponent->SetMobility(*ComponentMobility);
			}
		}
	}
}

void FMovieScene3DTransformTrackInstance::UpdateRuntimeMobility(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects)
{
	for( int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex )
	{
		UObject* Object = RuntimeObjects[ObjIndex].Get();
		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Object);

		if (SceneComponent != nullptr)
		{
			if (SceneComponent->Mobility != EComponentMobility::Movable)
			{
				if (InitMobilityMap.Find(Object) == nullptr)
				{
					InitMobilityMap.Add(Object, SceneComponent->Mobility);
				}

				SceneComponent->SetMobility(EComponentMobility::Movable);
			}
		}
	}
}

void FMovieScene3DTransformTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	if (UpdateData.UpdatePass == MSUP_PreUpdate)
	{
		UpdateRuntimeMobility(RuntimeObjects);
	}

	FVector Translation;
	FRotator Rotation;
	FVector Scale;

	if( TransformTrack->Eval( UpdateData.Position, UpdateData.LastPosition, Translation, Rotation, Scale ) )
	{
		for( int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex )
		{
			UObject* Object = RuntimeObjects[ObjIndex].Get();
			USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Object);

			if (SceneComponent != nullptr)
			{
				if (UpdateData.UpdatePass == MSUP_PreUpdate)
				{		
					// Set the transforms to identity explicitly instead of ResetRelativeTransform so that overlaps aren't evaluated at the origin
					SceneComponent->RelativeLocation = FVector(FVector::ZeroVector);
					SceneComponent->RelativeRotation = FRotator(FRotator::ZeroRotator);
					SceneComponent->RelativeScale3D = FVector(1.f);
					SceneComponent->UpdateComponentToWorld();
				}
				else if (UpdateData.UpdatePass == MSUP_Update)
				{
					SceneComponent->AddRelativeLocation(Translation);
					SceneComponent->AddRelativeRotation(Rotation);
					SceneComponent->SetRelativeScale3D(Scale);
				}
			}
		}
	}
}
 
void FMovieScene3DTransformTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	UpdateRuntimeMobility(RuntimeObjects);
}
