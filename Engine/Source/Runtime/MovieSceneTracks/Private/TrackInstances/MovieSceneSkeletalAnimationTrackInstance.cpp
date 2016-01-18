// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneSkeletalAnimationTrackInstance.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "IMovieScenePlayer.h"
#include "Matinee/MatineeAnimInterface.h"
#include "MovieSceneCommonHelpers.h"

struct FAnimEvalTimes
{
	float Current, Previous;

	/** Map the specified times onto the specified anim section */
	static FAnimEvalTimes MapTimesToAnimation(float ThisPosition, float LastPosition, UMovieSceneSkeletalAnimationSection* AnimSection)
	{
		ThisPosition = FMath::Clamp(ThisPosition, AnimSection->GetStartTime(), AnimSection->GetEndTime());
		LastPosition = FMath::Clamp(LastPosition, AnimSection->GetStartTime(), AnimSection->GetEndTime());

		ThisPosition -= 1 / 1000.0f;

		float AnimPlayRate = FMath::IsNearlyZero(AnimSection->GetPlayRate()) ? 1.0f : AnimSection->GetPlayRate();
		float SeqLength = AnimSection->GetSequenceLength() - (AnimSection->GetStartOffset() + AnimSection->GetEndOffset());

		ThisPosition = (ThisPosition - AnimSection->GetStartTime()) * AnimPlayRate;
		ThisPosition = FMath::Fmod(ThisPosition, SeqLength);
		ThisPosition += AnimSection->GetStartOffset();
		if (AnimSection->GetReverse())
		{
			ThisPosition = (SeqLength - (ThisPosition - AnimSection->GetStartOffset())) + AnimSection->GetStartOffset();
		}

		LastPosition = (LastPosition - AnimSection->GetStartTime()) * AnimPlayRate;
		LastPosition = FMath::Fmod(LastPosition, SeqLength);
		LastPosition += AnimSection->GetStartOffset();
		if (AnimSection->GetReverse())
		{
			LastPosition = (SeqLength - (LastPosition - AnimSection->GetStartOffset())) + AnimSection->GetStartOffset();
		}

		FAnimEvalTimes EvalTimes = { ThisPosition, LastPosition };
		return EvalTimes;
	}
};

FMovieSceneSkeletalAnimationTrackInstance::FMovieSceneSkeletalAnimationTrackInstance( UMovieSceneSkeletalAnimationTrack& InAnimationTrack )
{
	AnimationTrack = &InAnimationTrack;
}


FMovieSceneSkeletalAnimationTrackInstance::~FMovieSceneSkeletalAnimationTrackInstance()
{
	// @todo Sequencer Need to find some way to call PreviewFinishAnimControl (needs the runtime objects)
}

void FMovieSceneSkeletalAnimationTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
	// @todo Sequencer gameplay update has a different code path than editor update for animation

	for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
	{
		IMatineeAnimInterface* AnimInterface = Cast<IMatineeAnimInterface>(RuntimeObjects[i]);
		if (AnimInterface) 
		{
			UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimationTrack->GetAnimSectionAtTime(Position));
			
			// cbb: If there is no overlapping section, evaluate the closest section only if the current time is before it.
			if (AnimSection == nullptr)
			{
				AnimSection = Cast<UMovieSceneSkeletalAnimationSection>(MovieSceneHelpers::FindNearestSectionAtTime(AnimationTrack->GetAllSections(), Position));
				if (AnimSection != nullptr && (Position >= AnimSection->GetStartTime() || Position <= AnimSection->GetEndTime()))
				{
					AnimSection = nullptr;
				}
			}

			if (AnimSection && AnimSection->IsActive())
			{
				const FAnimEvalTimes EvalTimes = FAnimEvalTimes::MapTimesToAnimation(Position, LastPosition, AnimSection);

				int32 ChannelIndex = 0;
				UAnimSequence* AnimSequence = AnimSection->GetAnimSequence();

				// NOTE: The order of the bLooping and bFireNotifiers parameters are swapped in the preview and non-preview SetAnimPosition functions.
				const bool bLooping = false;
				if (GIsEditor && !GWorld->HasBegunPlay())
				{
					// Pass an absolute delta time to the preview anim evaluation
					float Delta = FMath::Abs(EvalTimes.Current - EvalTimes.Previous);
					const bool bFireNotifies = true;
					AnimInterface->PreviewBeginAnimControl(nullptr);
					AnimInterface->PreviewSetAnimPosition(AnimSection->GetSlotName(), ChannelIndex, AnimSequence, EvalTimes.Current, bLooping, bFireNotifies, Delta);
				}
				else
				{
					// Don't fire notifies at runtime since they will be fired through the standard animation tick path.
					const bool bFireNotifies = false;
					AnimInterface->BeginAnimControl(nullptr);
					AnimInterface->SetAnimPosition(AnimSection->GetSlotName(), ChannelIndex, AnimSequence, EvalTimes.Current, bFireNotifies, bLooping);
				}
			}
		}
	}
}
