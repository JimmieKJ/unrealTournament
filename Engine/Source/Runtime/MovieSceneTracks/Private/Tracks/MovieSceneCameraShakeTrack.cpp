// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneCameraShakeSection.h"
#include "MovieSceneCameraShakeTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneCameraShakeTrackInstance.h"

#define LOCTEXT_NAMESPACE "MovieSceneCameraShakeTrack"

void UMovieSceneCameraShakeTrack::AddNewCameraShake(float KeyTime, TSubclassOf<UCameraShake> ShakeClass)
{
	UMovieSceneCameraShakeSection* const NewSection = Cast<UMovieSceneCameraShakeSection>(CreateNewSection());
	if (NewSection)
	{
		// #fixme get length
		NewSection->InitialPlacement(CameraShakeSections, KeyTime, KeyTime + 5.f /*AnimSequence->SequenceLength*/, SupportsMultipleRows());
		NewSection->SetCameraShakeClass(ShakeClass);
		
		AddSection(*NewSection);
	}
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneCameraShakeTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Camera Shake");
}
#endif




/* UMovieSceneTrack interface
*****************************************************************************/

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneCameraShakeTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneCameraShakeTrackInstance(*this));
}


const TArray<UMovieSceneSection*>& UMovieSceneCameraShakeTrack::GetAllSections() const
{
	return CameraShakeSections;
}


UMovieSceneSection* UMovieSceneCameraShakeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraShakeSection>(this);
}


void UMovieSceneCameraShakeTrack::RemoveAllAnimationData()
{
	CameraShakeSections.Empty();
}


bool UMovieSceneCameraShakeTrack::HasSection(const UMovieSceneSection& Section) const
{
	return CameraShakeSections.Contains(&Section);
}


void UMovieSceneCameraShakeTrack::AddSection(UMovieSceneSection& Section)
{
	CameraShakeSections.Add(&Section);
}


void UMovieSceneCameraShakeTrack::RemoveSection(UMovieSceneSection& Section)
{
	CameraShakeSections.Remove(&Section);
}


bool UMovieSceneCameraShakeTrack::IsEmpty() const
{
	return CameraShakeSections.Num() == 0;
}


TRange<float> UMovieSceneCameraShakeTrack::GetSectionBoundaries() const
{
	TArray<TRange<float>> Bounds;

	for (auto Section : CameraShakeSections)
	{
		Bounds.Add(Section->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


void UMovieSceneCameraShakeTrack::GetCameraShakeSectionsAtTime(float Time, TArray<UMovieSceneCameraShakeSection*>& OutSections)
{
	OutSections.Empty();

	for (auto Section : CameraShakeSections)
	{
		UMovieSceneCameraShakeSection* const CSSection = dynamic_cast<UMovieSceneCameraShakeSection*>(Section);
		if (CSSection && CSSection->IsTimeWithinSection(Time))
		{
			OutSections.Add(CSSection);
		}
	}
}


#undef LOCTEXT_NAMESPACE
