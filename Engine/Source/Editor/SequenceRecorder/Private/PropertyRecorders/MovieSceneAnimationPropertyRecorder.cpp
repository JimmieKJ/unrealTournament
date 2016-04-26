// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "MovieSceneAnimationPropertyRecorder.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "MovieScene.h"
#include "SequenceRecorderUtils.h"
#include "SequenceRecorderSettings.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"

FMovieSceneAnimationPropertyRecorder::FMovieSceneAnimationPropertyRecorder(FAnimationRecordingSettings& InAnimationSettings, UAnimSequence* InSpecifiedSequence)
	: Sequence(InSpecifiedSequence)
	, AnimationSettings(InAnimationSettings)
{
}

void FMovieSceneAnimationPropertyRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord)
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

		// turn off URO and make sure we always update even if out of view
		bEnableUpdateRateOptimizations = SkeletalMeshComponent->bEnableUpdateRateOptimizations;
		MeshComponentUpdateFlag = SkeletalMeshComponent->MeshComponentUpdateFlag;

		SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
		SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

		ComponentTransform = SkeletalMeshComponent->GetComponentToWorld().GetRelativeTransform(SkeletalMeshComponent->GetOwner()->GetTransform());

		if(!Sequence.IsValid())
		{
			// build an asset path
			const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

			FString AssetPath(TEXT("/Game"));
			AssetPath /= Settings->SequenceRecordingBasePath.Path;
			if(Settings->AnimationSubDirectory.Len() > 0)
			{
				AssetPath /= Settings->AnimationSubDirectory;
			}

			FString AssetName = Settings->SequenceName.Len() > 0 ? Settings->SequenceName : TEXT("RecordedSequence");
			AssetName += TEXT("_");
			AssetName += Actor->GetActorLabel();

			Sequence = SequenceRecorderUtils::MakeNewAsset<UAnimSequence>(AssetPath, AssetName);
			if(Sequence.IsValid())
			{
				FAssetRegistryModule::AssetCreated(Sequence.Get());

				// set skeleton
				Sequence->SetSkeleton(SkeletalMeshComponent->SkeletalMesh->Skeleton);
			}
		}

		if(Sequence.IsValid())
		{
			FAnimationRecorderManager::Get().RecordAnimation(SkeletalMeshComponent.Get(), Sequence.Get(), AnimationSettings);

			if(MovieScene)
			{
				UMovieSceneSkeletalAnimationTrack* AnimTrack = MovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(Guid);
				if(AnimTrack)
				{
					AnimTrack->AddNewAnimation(Time, Sequence.Get());
					MovieSceneSection = Cast<UMovieSceneSkeletalAnimationSection>(AnimTrack->GetAllSections()[0]);
				}
			}
		}
	}

	bRecording = bRecord;
}

void FMovieSceneAnimationPropertyRecorder::FinalizeSection()
{
	bRecording = false;

	if(Sequence.IsValid())
	{
		// enable root motion on the animation
		Sequence->bEnableRootMotion = true;
		Sequence->RootMotionRootLock = ERootMotionRootLock::Zero;
	}

	if(SkeletalMeshComponent.IsValid())
	{
		// restore update flags
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = bEnableUpdateRateOptimizations;
		SkeletalMeshComponent->MeshComponentUpdateFlag = MeshComponentUpdateFlag;

		// only show a message if we dont have a valid movie section
		const bool bShowMessage = !MovieSceneSection.IsValid();
		FAnimationRecorderManager::Get().StopRecordingAnimation(SkeletalMeshComponent.Get(), bShowMessage);
	}

	if(MovieSceneSection.IsValid() && Sequence.IsValid())
	{
		MovieSceneSection->SetEndTime(MovieSceneSection->GetStartTime() + Sequence->GetPlayLength());
	}
}

void FMovieSceneAnimationPropertyRecorder::Record(float CurrentTime)
{
	// The animation recorder does most of the work here

	if(SkeletalMeshComponent.IsValid())
	{
		// re-force updates on as gameplay can sometimes turn these back off!
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;
		SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	}
}