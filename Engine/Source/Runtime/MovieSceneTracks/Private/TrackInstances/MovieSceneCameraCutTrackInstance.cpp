// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneCameraCutTrack.h"
#include "MovieSceneCameraCutTrackInstance.h"
#include "MovieSceneCameraCutSection.h"
#include "IMovieScenePlayer.h"


/* FMovieSceneCameraCutTrackInstance structors
 *****************************************************************************/

FMovieSceneCameraCutTrackInstance::FMovieSceneCameraCutTrackInstance(UMovieSceneCameraCutTrack& InCameraCutTrack)
	: CameraCutTrack(&InCameraCutTrack)
{ }


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneCameraCutTrackInstance::ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	Player.UpdateCameraCut(nullptr, nullptr);
}


void FMovieSceneCameraCutTrackInstance::RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	const TArray<UMovieSceneSection*>& CameraCutSections = CameraCutTrack->GetAllSections();

	CachedCameraObjects.Empty();
	CachedCameraObjects.SetNumZeroed(CameraCutSections.Num());

	for (int32 CameraCutIndex = 0; CameraCutIndex < CameraCutSections.Num(); ++CameraCutIndex)
	{
		UMovieSceneCameraCutSection* CameraCutSection = CastChecked<UMovieSceneCameraCutSection>(CameraCutSections[CameraCutIndex]);
		AcquireCameraForCameraCut(CameraCutIndex, CameraCutSection->GetCameraGuid(), SequenceInstance, Player);
	}
}

	
void FMovieSceneCameraCutTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	Player.UpdateCameraCut(nullptr, nullptr);
}
	
void FMovieSceneCameraCutTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	// Don't switch CameraCuts when in preroll
	if (UpdateData.bPreroll || !UpdateData.bUpdateCameras)
	{
		return;
	}

	UObject* CameraObject = nullptr;
	const TArray<UMovieSceneSection*>& CameraCutSections = CameraCutTrack->GetAllSections();

	UMovieSceneCameraCutSection* NearestCameraCutSection = Cast<UMovieSceneCameraCutSection>(MovieSceneHelpers::FindSectionAtTime(CameraCutSections, UpdateData.Position));
	if (NearestCameraCutSection != nullptr)
	{
		for (int32 CameraCutIndex = 0; CameraCutIndex < CameraCutSections.Num(); ++CameraCutIndex)
		{
			UMovieSceneCameraCutSection* CameraCutSection = CastChecked<UMovieSceneCameraCutSection>(CameraCutSections[CameraCutIndex]);

			if (CameraCutSection == NearestCameraCutSection)
			{
				CameraObject = AcquireCameraForCameraCut(CameraCutIndex, CameraCutSection->GetCameraGuid(), SequenceInstance, Player);

				break;
			}
		}
	}

	if (CameraObject != LastCameraObject)
	{
		Player.UpdateCameraCut(CameraObject, LastCameraObject.Get(), UpdateData.bJumpCut);
		LastCameraObject = CameraObject;
	}
	else if (CameraObject != nullptr)
	{
		Player.UpdateCameraCut(CameraObject, nullptr, UpdateData.bJumpCut);
	}
}


/* FMovieSceneCameraCutTrackInstance implementation
 *****************************************************************************/

UObject* FMovieSceneCameraCutTrackInstance::AcquireCameraForCameraCut(int32 CameraCutIndex, const FGuid& CameraGuid, FMovieSceneSequenceInstance& SequenceInstance, const IMovieScenePlayer& Player)
{
	auto& CachedCamera = CachedCameraObjects[CameraCutIndex];

	if (!CachedCamera.IsValid())
	{
		CachedCamera = SequenceInstance.FindObject(CameraGuid, Player);
	}

	return CachedCamera.Get();
}
