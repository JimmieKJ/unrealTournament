// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieScene3DConstraintTrackInstance.h"
#include "MovieScene3DConstraintTrack.h"
#include "MovieScene3DConstraintSection.h"
#include "IMovieScenePlayer.h"
#include "Components/SplineComponent.h"


FMovieScene3DConstraintTrackInstance::FMovieScene3DConstraintTrackInstance( UMovieScene3DConstraintTrack& InConstraintTrack )
{
	ConstraintTrack = &InConstraintTrack;
}


void FMovieScene3DConstraintTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex)
	{
		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(RuntimeObjects[ObjIndex]);
		if (SceneComponent != nullptr)
		{
			if (InitTransformMap.Find(RuntimeObjects[ObjIndex]) == nullptr)
			{
				InitTransformMap.Add(RuntimeObjects[ObjIndex], SceneComponent->GetRelativeTransform());
			}
		}
	}
}


void FMovieScene3DConstraintTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (int32 ObjIndex = 0; ObjIndex < RuntimeObjects.Num(); ++ObjIndex)
	{
		if (!IsValid(RuntimeObjects[ObjIndex]))
		{
			continue;
		}

		USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(RuntimeObjects[ObjIndex]);
		if (SceneComponent != nullptr)
		{
			FTransform *Transform = InitTransformMap.Find(RuntimeObjects[ObjIndex]);
			if (Transform != nullptr)
			{
				SceneComponent->SetRelativeTransform(*Transform);
			}
		}
	}
}


void FMovieScene3DConstraintTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
	UMovieScene3DConstraintSection* FirstConstraintSection = nullptr;

	const TArray<UMovieSceneSection*>& ConstraintSections = ConstraintTrack->GetAllSections();

	for (int32 ConstraintIndex = 0; ConstraintIndex < ConstraintSections.Num(); ++ConstraintIndex)
	{
		UMovieScene3DConstraintSection* ConstraintSection = CastChecked<UMovieScene3DConstraintSection>(ConstraintSections[ConstraintIndex]);

		if (ConstraintSection->IsTimeWithinSection(Position) &&
			(FirstConstraintSection == nullptr || FirstConstraintSection->GetRowIndex() > ConstraintSection->GetRowIndex()))
		{
			TArray<UObject*> ConstraintObjects;
			FGuid ConstraintId = ConstraintSection->GetConstraintId();

			if (ConstraintId.IsValid())
			{
				Player.GetRuntimeObjects( Player.GetRootMovieSceneSequenceInstance(), ConstraintId, ConstraintObjects);

				for (int32 ConstraintObjectIndex = 0; ConstraintObjectIndex < ConstraintObjects.Num(); ++ConstraintObjectIndex)
				{
					AActor* Actor = Cast<AActor>(ConstraintObjects[ConstraintObjectIndex]);
					if (Actor)
					{
						UpdateConstraint(Position, RuntimeObjects, Actor, ConstraintSection);
					}	
				}
			}
		}
	}
}
