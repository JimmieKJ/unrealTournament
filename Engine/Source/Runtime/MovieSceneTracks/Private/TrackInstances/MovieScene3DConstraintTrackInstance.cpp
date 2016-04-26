// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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


void FMovieScene3DConstraintTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
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
		}
	}
}


void FMovieScene3DConstraintTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
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
		}
	}
}


void FMovieScene3DConstraintTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	UMovieScene3DConstraintSection* FirstConstraintSection = nullptr;

	const TArray<UMovieSceneSection*>& ConstraintSections = ConstraintTrack->GetAllSections();

	for (int32 ConstraintIndex = 0; ConstraintIndex < ConstraintSections.Num(); ++ConstraintIndex)
	{
		UMovieScene3DConstraintSection* ConstraintSection = CastChecked<UMovieScene3DConstraintSection>(ConstraintSections[ConstraintIndex]);

		if (ConstraintSection->IsTimeWithinSection(UpdateData.Position) &&
			(FirstConstraintSection == nullptr || FirstConstraintSection->GetRowIndex() > ConstraintSection->GetRowIndex()))
		{
			TArray<TWeakObjectPtr<UObject>> ConstraintObjects;
			FGuid ConstraintId = ConstraintSection->GetConstraintId();

			if (ConstraintId.IsValid())
			{
				Player.GetRuntimeObjects( Player.GetRootMovieSceneSequenceInstance(), ConstraintId, ConstraintObjects);

				// Also look in this current sequence instance for the constrained object
				UObject* FoundObject = SequenceInstance.FindObject(ConstraintId, Player);
				if (FoundObject && ConstraintObjects.Find(FoundObject) == INDEX_NONE)
				{
					ConstraintObjects.Add(FoundObject);
				}

				for (int32 ConstraintObjectIndex = 0; ConstraintObjectIndex < ConstraintObjects.Num(); ++ConstraintObjectIndex)
				{
					AActor* Actor = Cast<AActor>(ConstraintObjects[ConstraintObjectIndex].Get());
					if (Actor)
					{
						UpdateConstraint(UpdateData.Position, RuntimeObjects, Actor, ConstraintSection);
					}	
				}
			}
		}
	}
}
