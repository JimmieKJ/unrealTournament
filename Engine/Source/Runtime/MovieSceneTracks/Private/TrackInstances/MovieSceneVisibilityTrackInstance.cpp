// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneVisibilityTrackInstance.h"
#include "MovieSceneVisibilityTrack.h"


FMovieSceneVisibilityTrackInstance::FMovieSceneVisibilityTrackInstance(UMovieSceneVisibilityTrack& InVisibilityTrack)
{
	VisibilityTrack = &InVisibilityTrack;
	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( VisibilityTrack->GetPropertyName(), VisibilityTrack->GetPropertyPath() ) );
}


void FMovieSceneVisibilityTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
 	for( UObject* Object : RuntimeObjects )
 	{
		AActor* Actor = Cast<AActor>(Object);

		if (Actor != nullptr)
		{
#if WITH_EDITOR
			if (GIsEditor && !Actor->GetWorld()->IsPlayInEditor())
			{
				if (InitHiddenInEditorMap.Find(Actor) == nullptr)
				{
					InitHiddenInEditorMap.Add(Actor, Actor->IsTemporarilyHiddenInEditor());
				}
			}
			USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Actor);
			if (SceneComponent != nullptr)
			{
				if (InitSceneComponentVisibleMap.Find(Actor) == nullptr)
				{
					InitSceneComponentVisibleMap.Add(Actor, SceneComponent->IsVisibleInEditor());
				}
			}
#endif // WITH_EDITOR

			if (InitHiddenInGameMap.Find(Actor) == nullptr)
			{
				InitHiddenInGameMap.Add(Actor, Actor->bHidden);
			}
		}
 	}
}


void FMovieSceneVisibilityTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
 	for( UObject* Object : RuntimeObjects )
 	{
		if (!IsValid(Object))
		{
			continue;
		}

		AActor* Actor = Cast<AActor>(Object);

		if (Actor != nullptr)
		{
			bool *HiddenInGameValue = InitHiddenInGameMap.Find(Object);
			if (HiddenInGameValue != nullptr)
			{
				Actor->SetActorHiddenInGame(*HiddenInGameValue);
			}

#if WITH_EDITOR
			if (GIsEditor && !Actor->GetWorld()->IsPlayInEditor())
			{
				bool *HiddenInEditorValue = InitHiddenInEditorMap.Find(Object);
				if (HiddenInEditorValue != nullptr)
				{
					if (Actor->IsTemporarilyHiddenInEditor() != *HiddenInEditorValue)
					{
						Actor->SetIsTemporarilyHiddenInEditor(*HiddenInEditorValue);
					}
				}

				bool *SceneComponentVisibleValue = InitSceneComponentVisibleMap.Find(Object);
				if (SceneComponentVisibleValue != nullptr)
				{
					USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Actor);
					if (SceneComponent != nullptr)
					{
						if (SceneComponent->IsVisibleInEditor() != *SceneComponentVisibleValue)
						{
							SceneComponent->SetVisibility(*SceneComponentVisibleValue);
						}
					}
				}
			}
#endif // WITH_EDITOR
		}
 	}
}


void FMovieSceneVisibilityTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
 	bool Visible = false;
 	if( VisibilityTrack->Eval( Position, LastPosition, Visible ) )
 	{
		// Invert this evaluation since the property is "bHiddenInGame" and we want the visualization to be the inverse of that. Green means visible.
		Visible = !Visible;

 		for( UObject* Object : RuntimeObjects )
 		{
			AActor* Actor = Cast<AActor>(Object);

			if (Actor != nullptr)
			{
				Actor->SetActorHiddenInGame(Visible);

#if WITH_EDITOR
				if (GIsEditor && !Actor->GetWorld()->IsPlayInEditor())
				{
					// In editor HiddenGame flag is not respected so set bHiddenEdTemporary too.
					// It will be restored just like HiddenGame flag when Matinee is closed.
					if (Actor->IsTemporarilyHiddenInEditor() != Visible)
					{
						Actor->SetIsTemporarilyHiddenInEditor(Visible);
					}

					USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Actor);
					if (SceneComponent != nullptr)
					{
						if (SceneComponent->IsVisibleInEditor() != !Visible)
						{
							SceneComponent->SetVisibility(!Visible);
						}
					}
				}
#endif // WITH_EDITOR
			}
 		}
	}
}


void FMovieSceneVisibilityTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
