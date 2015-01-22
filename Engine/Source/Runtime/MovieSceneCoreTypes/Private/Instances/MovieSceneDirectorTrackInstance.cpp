// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneDirectorTrackInstance.h"
#include "IMovieScenePlayer.h"

FMovieSceneDirectorTrackInstance::FMovieSceneDirectorTrackInstance( UMovieSceneDirectorTrack& InDirectorTrack )
{
	DirectorTrack = &InDirectorTrack;
}

void FMovieSceneDirectorTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	// possess the 'first' shot section in order of track index
	ACameraActor* PossessedCamera = NULL;
	UMovieSceneShotSection* FirstShotSection = NULL;

	const TArray<UMovieSceneSection*>& ShotSections = DirectorTrack->GetShotSections();

	for (int32 ShotIndex = 0; ShotIndex < ShotSections.Num(); ++ShotIndex)
	{
		UMovieSceneShotSection* ShotSection = CastChecked<UMovieSceneShotSection>(ShotSections[ShotIndex]);

		if (ShotSection->IsTimeWithinSection(Position) &&
			(FirstShotSection == NULL || FirstShotSection->GetRowIndex() > ShotSection->GetRowIndex()))
		{
			TArray<UObject*> CameraObjects;
			// @todo Sequencer - Sub-moviescenes: Get the cameras from the root movie scene instance.  We should support adding cameras for sub-moviescenes as shots
			Player.GetRuntimeObjects( Player.GetRootMovieSceneInstance(), ShotSection->GetCameraGuid(), CameraObjects);

			if (CameraObjects.Num() > 0)
			{
				check(CameraObjects.Num() == 1 && CameraObjects[0]->IsA(ACameraActor::StaticClass()));

				PossessedCamera = Cast<ACameraActor>(CameraObjects[0]);

				FirstShotSection = ShotSection;
			}
		}
	}

	Player.UpdatePreviewViewports(PossessedCamera);
}

