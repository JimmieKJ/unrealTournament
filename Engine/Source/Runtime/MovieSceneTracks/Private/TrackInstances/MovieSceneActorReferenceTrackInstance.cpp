// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneActorReferenceTrack.h"
#include "MovieSceneActorReferenceTrackInstance.h"


FMovieSceneActorReferenceTrackInstance::FMovieSceneActorReferenceTrackInstance( UMovieSceneActorReferenceTrack& InActorReferenceTrack )
{
	ActorReferenceTrack = &InActorReferenceTrack;
	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( ActorReferenceTrack->GetPropertyName(), ActorReferenceTrack->GetPropertyPath() ) );
}


void FMovieSceneActorReferenceTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (!InitActorReferenceMap.Contains(Object))
		{
			AActor* ActorReferenceValue = PropertyBindings->GetCurrentValue<AActor*>(Object);
			InitActorReferenceMap.Add(FObjectKey(Object), ActorReferenceValue);
		}
	}
}


void FMovieSceneActorReferenceTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (!IsValid(Object))
		{
			continue;
		}

		if (InitActorReferenceMap.Contains(FObjectKey(Object)))
		{
			TWeakObjectPtr<AActor> ActorReferencePtr = *InitActorReferenceMap.Find(FObjectKey(Object));
			if (ActorReferencePtr.IsValid())
			{
				AActor* ActorReferenceValue = ActorReferencePtr.Get();
				PropertyBindings->CallFunction<AActor*>(Object, &ActorReferenceValue);
			}
		}
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );
}


void FMovieSceneActorReferenceTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) 
{
	
	FGuid ActorReferenceGuid;
	if( ActorReferenceTrack->Eval( UpdateData.Position, UpdateData.LastPosition, ActorReferenceGuid ) )
	{
		AActor* CurrentActor = nullptr;
		if (GuidToActorCache.Contains(ActorReferenceGuid))
		{
			TWeakObjectPtr<AActor> CachedActor = *GuidToActorCache.Find(ActorReferenceGuid);
			if (CachedActor.IsValid())
			{
				CurrentActor = CachedActor.Get();
			}
		}

		if (CurrentActor == nullptr)
		{
			TArray<TWeakObjectPtr<UObject>> RuntimeObjectsForGuid;
			Player.GetRuntimeObjects(SequenceInstance.AsShared(), ActorReferenceGuid, RuntimeObjectsForGuid);
	
			if (RuntimeObjectsForGuid.Num() == 1)
			{
				AActor* ActorForGuid = Cast<AActor>(RuntimeObjectsForGuid[0].Get());

				if (ActorForGuid != nullptr)
				{
					CurrentActor = ActorForGuid;
					if (CurrentActor != nullptr)
					{
						GuidToActorCache.Add(ActorReferenceGuid, CurrentActor);
					}
				}
			}
		}

		if (CurrentActor != nullptr)
		{
			for (auto ObjectPtr : RuntimeObjects)
			{
				UObject* Object = ObjectPtr.Get();
				PropertyBindings->CallFunction<AActor*>(Object, &CurrentActor);
			}
		}
	}
}


void FMovieSceneActorReferenceTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	GuidToActorCache.Empty();
	PropertyBindings->UpdateBindings(RuntimeObjects);
}
