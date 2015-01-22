// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneAnimationTrackInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneAnimationTrack.h"
#include "Matinee/MatineeAnimInterface.h"

FMovieSceneAnimationTrackInstance::FMovieSceneAnimationTrackInstance( UMovieSceneAnimationTrack& InAnimationTrack )
{
	AnimationTrack = &InAnimationTrack;
}

FMovieSceneAnimationTrackInstance::~FMovieSceneAnimationTrackInstance()
{
	// @todo Sequencer Need to find some way to call PreviewFinishAnimControl (needs the runtime objects)
}

void FMovieSceneAnimationTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	// @todo Sequencer gameplay update has a different code path than editor update for animation

	for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
	{
		IMatineeAnimInterface* AnimInterface = Cast<IMatineeAnimInterface>(RuntimeObjects[i]);
		if (AnimInterface)
		{
			UMovieSceneAnimationSection* AnimSection = Cast<UMovieSceneAnimationSection>(AnimationTrack->GetAnimSectionAtTime(Position));
			if (AnimSection)
			{
				int32 ChannelIndex = 0;
				FName SlotName = FName("AnimationSlot");
				UAnimSequence* AnimSequence = AnimSection->GetAnimSequence();
				float AnimPosition = FMath::Fmod(Position - AnimSection->GetAnimationStartTime(), AnimSection->GetAnimationDuration()) / AnimSection->GetAnimationDilationFactor();

				AnimInterface->PreviewBeginAnimControl(NULL);
				AnimInterface->PreviewSetAnimPosition(SlotName, ChannelIndex, AnimSequence, AnimPosition, true, false, 0.f);
			}
		}
	}
}
