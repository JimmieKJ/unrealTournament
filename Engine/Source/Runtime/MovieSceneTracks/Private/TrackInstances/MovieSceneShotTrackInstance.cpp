// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneShotTrack.h"
#include "MovieSceneShotTrackInstance.h"
#include "MovieSceneShotSection.h"
#include "IMovieScenePlayer.h"


/* FMovieSceneShotTrackInstance structors
 *****************************************************************************/

FMovieSceneShotTrackInstance::FMovieSceneShotTrackInstance(UMovieSceneShotTrack& InShotTrack)
	: ShotTrack(&InShotTrack)
{ }


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneShotTrackInstance::ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	Player.UpdateCameraCut(nullptr, nullptr);
}


void FMovieSceneShotTrackInstance::RefreshInstance(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	const TArray<UMovieSceneSection*>& ShotSections = ShotTrack->GetAllSections();

	CachedCameraObjects.SetNumZeroed(ShotSections.Num());

	for (int32 ShotIndex = 0; ShotIndex < ShotSections.Num(); ++ShotIndex)
	{
		UMovieSceneShotSection* ShotSection = CastChecked<UMovieSceneShotSection>(ShotSections[ShotIndex]);
		AcquireCameraForShot(ShotIndex, ShotSection->GetCameraGuid(), SequenceInstance, Player);
	}
}

	
void FMovieSceneShotTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	Player.UpdateCameraCut(nullptr, nullptr);
}
	
void FMovieSceneShotTrackInstance::Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass) 
{
	UObject* CameraObject = nullptr;
	const TArray<UMovieSceneSection*>& ShotSections = ShotTrack->GetAllSections();

	UMovieSceneShotSection* NearestShotSection = Cast<UMovieSceneShotSection>(MovieSceneHelpers::FindNearestSectionAtTime(ShotSections, Position));
	if (NearestShotSection != nullptr)
	{
		for (int32 ShotIndex = 0; ShotIndex < ShotSections.Num(); ++ShotIndex)
		{
			UMovieSceneShotSection* ShotSection = CastChecked<UMovieSceneShotSection>(ShotSections[ShotIndex]);

			if (ShotSection == NearestShotSection)
			{
				CameraObject = AcquireCameraForShot(ShotIndex, ShotSection->GetCameraGuid(), SequenceInstance, Player);

				break;
			}
		}
	}

	if (CameraObject != LastCameraObject)
	{
		Player.UpdateCameraCut(CameraObject, LastCameraObject.Get());
		LastCameraObject = CameraObject;
	}
	else if (CameraObject != nullptr)
	{
		Player.UpdateCameraCut(CameraObject, nullptr);
	}
}


/* FMovieSceneShotTrackInstance implementation
 *****************************************************************************/

UObject* FMovieSceneShotTrackInstance::AcquireCameraForShot(int32 ShotIndex, const FGuid& CameraGuid, FMovieSceneSequenceInstance& SequenceInstance, const IMovieScenePlayer& Player)
{
	auto& CachedCamera = CachedCameraObjects[ShotIndex];

	if (!CachedCamera.IsValid())
	{
		CachedCamera = SequenceInstance.FindObject(CameraGuid, Player);
	}

	return CachedCamera.Get();
}
