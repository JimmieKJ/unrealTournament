// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "MovieSceneAnimationSectionRecorder.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "MovieScene.h"
#include "SequenceRecorderUtils.h"
#include "SequenceRecorderSettings.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "ActorRecording.h"
#include "Animation/AnimSequence.h"

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneAnimationSectionRecorderFactory::CreateSectionRecorder(const FActorRecordingSettings& InActorRecordingSettings) const
{
	return nullptr;
}

TSharedPtr<FMovieSceneAnimationSectionRecorder> FMovieSceneAnimationSectionRecorderFactory::CreateSectionRecorder(UActorRecording* InActorRecording, const FAnimationRecordingSettings& InAnimationSettings) const
{
	return MakeShareable(new FMovieSceneAnimationSectionRecorder(InAnimationSettings, InActorRecording->TargetAnimation.Get()));
}

bool FMovieSceneAnimationSectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	if (InObjectToRecord->IsA<USkeletalMeshComponent>())
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObjectToRecord);
		if (SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh)
		{
			return true;
		}
	}
	return false;
}

FMovieSceneAnimationSectionRecorder::FMovieSceneAnimationSectionRecorder(const FAnimationRecordingSettings& InAnimationSettings, UAnimSequence* InSpecifiedSequence)
	: AnimSequence(InSpecifiedSequence)
	, bRemoveRootTransform(true)
	, AnimationSettings(InAnimationSettings)
{
}

void FMovieSceneAnimationSectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time)
{
	ObjectToRecord = InObjectToRecord;

	AActor* Actor = nullptr;
	SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InObjectToRecord);
	if(!SkeletalMeshComponent.IsValid())
	{
		Actor = Cast<AActor>(InObjectToRecord);
		if(Actor != nullptr)
		{
			SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
		}
	}
	else
	{
		Actor = SkeletalMeshComponent->GetOwner();
	}

	if(SkeletalMeshComponent.IsValid())
	{
		SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
		if (SkeletalMesh != nullptr)
		{
			ComponentTransform = SkeletalMeshComponent->GetComponentToWorld().GetRelativeTransform(SkeletalMeshComponent->GetOwner()->GetTransform());

			if (!AnimSequence.IsValid())
			{
				// build an asset path
				const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

				FString AssetPath = Settings->SequenceRecordingBasePath.Path;
				if (Settings->AnimationSubDirectory.Len() > 0)
				{
					AssetPath /= Settings->AnimationSubDirectory;
				}

				FString AssetName = Settings->SequenceName.Len() > 0 ? Settings->SequenceName : TEXT("RecordedSequence");
				AssetName += TEXT("_");
				AssetName += Actor->GetActorLabel();

				AnimSequence = SequenceRecorderUtils::MakeNewAsset<UAnimSequence>(AssetPath, AssetName);
				if (AnimSequence.IsValid())
				{
					FAssetRegistryModule::AssetCreated(AnimSequence.Get());

					// set skeleton
					AnimSequence->SetSkeleton(SkeletalMeshComponent->SkeletalMesh->Skeleton);
				}
			}

			if (AnimSequence.IsValid())
			{
				FAnimationRecorderManager::Get().RecordAnimation(SkeletalMeshComponent.Get(), AnimSequence.Get(), AnimationSettings);

				if (MovieScene)
				{
					UMovieSceneSkeletalAnimationTrack* AnimTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(Guid);
					if (AnimTrack)
					{
						AnimTrack->AddNewAnimation(Time, AnimSequence.Get());
						MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
					}
				}
			}
		}
	}
}

void FMovieSceneAnimationSectionRecorder::FinalizeSection()
{
	if(AnimSequence.IsValid())
	{
		if (AnimationSettings.bRemoveRootAnimation)
		{
			// enable root motion on the animation
			AnimSequence->bEnableRootMotion = true;
			AnimSequence->RootMotionRootLock = ERootMotionRootLock::Zero;
		}
		else
		{
			// enable root motion on the animation
			AnimSequence->bEnableRootMotion = false;
			AnimSequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
		}
	}

	if(SkeletalMeshComponent.IsValid())
	{
		// only show a message if we dont have a valid movie section
		const bool bShowMessage = !MovieSceneSection.IsValid();
		FAnimationRecorderManager::Get().StopRecordingAnimation(SkeletalMeshComponent.Get(), bShowMessage);
	}

	if(MovieSceneSection.IsValid() && AnimSequence.IsValid())
	{
		MovieSceneSection->SetEndTime(MovieSceneSection->GetStartTime() + AnimSequence->GetPlayLength());
	}
}

void FMovieSceneAnimationSectionRecorder::Record(float CurrentTime)
{
	// The animation recorder does most of the work here

	if(SkeletalMeshComponent.IsValid())
	{
		// re-force updates on as gameplay can sometimes turn these back off!
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
		SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	}
}