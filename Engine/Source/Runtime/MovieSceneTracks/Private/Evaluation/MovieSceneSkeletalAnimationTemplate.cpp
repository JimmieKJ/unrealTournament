// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"

#include "Compilation/MovieSceneCompilerRules.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"


bool ShouldUsePreviewPlayback(IMovieScenePlayer& Player, UObject& RuntimeObject)
{
	// we also use PreviewSetAnimPosition in PIE when not playing, as we can preview in PIE
	bool bIsNotInPIEOrNotPlaying = (RuntimeObject.GetWorld() && !RuntimeObject.GetWorld()->HasBegunPlay()) || Player.GetPlaybackStatus() != EMovieScenePlayerStatus::Playing;
	return GIsEditor && bIsNotInPIEOrNotPlaying;
}

bool CanPlayAnimation(USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* AnimAssetBase = nullptr)
{
	return (SkeletalMeshComponent->SkeletalMesh && SkeletalMeshComponent->SkeletalMesh->Skeleton && 
		(!AnimAssetBase || SkeletalMeshComponent->SkeletalMesh->Skeleton->IsCompatible(AnimAssetBase->GetSkeleton())));
}

struct FAnimationTrackTrackState : IPersistentEvaluationData
{
	struct FComponentAndMontage
	{
		FComponentAndMontage(USkeletalMeshComponent* InComponent) : Component(InComponent) {}

		TWeakObjectPtr<UAnimMontage> Montage;
		TWeakObjectPtr<USkeletalMeshComponent> Component;
	};

	TMap<FObjectKey, FComponentAndMontage> CurrentlyPlayingMontages;

	~FAnimationTrackTrackState()
	{
		for (auto& Pair : CurrentlyPlayingMontages)
		{
			USkeletalMeshComponent* Component = Pair.Value.Component.Get();

			if (Component)
			{
				FinishAnimControl(Component);
			}
		}
	}

	void EnsureAnimControl(USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* InAnimSequence, IMovieScenePlayer& Player)
	{
		SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

		if (!CurrentlyPlayingMontages.Contains(SkeletalMeshComponent))
		{
			BeginAnimControl(SkeletalMeshComponent, InAnimSequence);
		}
	}

private:

	void BeginAnimControl(USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* InAnimSequence)
	{
		if (CanPlayAnimation(SkeletalMeshComponent, InAnimSequence))
		{
			UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
			if (!AnimInstance)
			{
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
			}
		}

		CurrentlyPlayingMontages.Add(SkeletalMeshComponent, FComponentAndMontage(SkeletalMeshComponent));
	}

	void FinishAnimControl(USkeletalMeshComponent* SkeletalMeshComponent)
	{
		if (SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationBlueprint)
		{
			UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
			if(AnimInstance)
			{
				AnimInstance->Montage_Stop(0.f);
				AnimInstance->UpdateAnimation(0.f, false);
			}

			// Update space bases to reset it back to ref pose
			SkeletalMeshComponent->RefreshBoneTransforms();
			SkeletalMeshComponent->RefreshSlaveComponents();
			SkeletalMeshComponent->UpdateComponentToWorld();
		}
	}
};

struct FPreAnimatedAnimationTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FPreAnimatedToken : IMovieScenePreAnimatedToken
		{
			EMeshComponentUpdateFlag::Type MeshComponentUpdateFlag;

			virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player)
			{
				CastChecked<USkeletalMeshComponent>(&InObject)->MeshComponentUpdateFlag = MeshComponentUpdateFlag;
			}
		};

		FPreAnimatedToken Token;
		Token.MeshComponentUpdateFlag = CastChecked<USkeletalMeshComponent>(&Object)->MeshComponentUpdateFlag;
		return Token;
	}
};

struct FAnimationTrackTrackToken : IMovieSceneExecutionToken
{
	FAnimationTrackTrackState* TrackState;

	FAnimationTrackTrackToken(const FMovieSceneSkeletalAnimationSectionTemplateParameters& InParams)
		: Params(InParams)
	{
	}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		TrackState = &PersistentData.GetOrAddTrackData<FAnimationTrackTrackState>();

		for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = Object.Get();
			if (!ObjectPtr)
			{
				continue;
			}

			// first try to control the component directly
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ObjectPtr);
			if (!SkeletalMeshComponent)
			{
				// then check to see if we are controlling an actor & if so use its first USkeletalMeshComponent 
				if (AActor* Actor = Cast<AActor>(ObjectPtr))
				{
					SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
				}
			}

			UAnimSequenceBase* AnimSequence = Params.Animation;

			if (!SkeletalMeshComponent || !AnimSequence)
			{
				continue;
			}

			const float EvalTime = Params.MapTimeToAnimation(Context.GetTime());

			Player.SavePreAnimatedState(*SkeletalMeshComponent, TMovieSceneAnimTypeID<FAnimationTrackTrackToken>(), FPreAnimatedAnimationTokenProducer());

			TrackState->EnsureAnimControl(SkeletalMeshComponent, AnimSequence, Player);

			static const int32 ChannelIndex = 0;
			static const bool bLooping = false;

			if (ShouldUsePreviewPlayback(Player, *SkeletalMeshComponent))
			{
				const EMovieScenePlayerStatus::Type PlayerStatus = Player.GetPlaybackStatus();

				// If the playback status is jumping, ie. one such occurrence is setting the time for thumbnail generation, disable anim notifies updates because it could fire audio
				const bool bFireNotifies = PlayerStatus != EMovieScenePlayerStatus::Jumping && PlayerStatus != EMovieScenePlayerStatus::Stopped;

				// When jumping from one cut to another cut, the delta time should be 0 so that anim notifies before the current position are not evaluated. Note, anim notifies at the current time should still be evaluated.
				const float DeltaTime = Context.HasJumped() ? 0.f : Context.GetRange().Size<float>();

				const bool bResetDynamics = PlayerStatus == EMovieScenePlayerStatus::Stepping || 
											PlayerStatus == EMovieScenePlayerStatus::Jumping || 
											PlayerStatus == EMovieScenePlayerStatus::Scrubbing || 
											(DeltaTime == 0.0f && PlayerStatus != EMovieScenePlayerStatus::Stopped); 
			
				PreviewSetAnimPosition(SkeletalMeshComponent, Params.SlotName, ChannelIndex, AnimSequence, EvalTime, bLooping, bFireNotifies, DeltaTime, Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing, bResetDynamics);
			}
			else
			{
				// Don't fire notifies at runtime since they will be fired through the standard animation tick path.
				const bool bFireNotifies = false;
				SetAnimPosition(SkeletalMeshComponent, Params.SlotName, ChannelIndex, AnimSequence, EvalTime, bLooping, bFireNotifies);
			}
		}
	}

	void SetAnimPosition(USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, int32 ChannelIndex, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies)
	{
		if (CanPlayAnimation(SkeletalMeshComponent, InAnimSequence))
		{
			UAnimMontage* Montage = FAnimMontageInstance::SetMatineeAnimPositionInner(SlotName, SkeletalMeshComponent, InAnimSequence, InPosition, bLooping);

			// Ensure the sequence is not stopped
			UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
			UAnimSingleNodeInstance* SingleNodeInst = SkeletalMeshComponent->GetSingleNodeInstance();
			if(SingleNodeInst)
			{
				SingleNodeInst->SetPlaying(true);
			}
			else if (AnimInst && Montage)
			{
				AnimInst->Montage_Resume(Montage);
			}
		}
	}

	void PreviewSetAnimPosition(USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, int32 ChannelIndex, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float DeltaTime, bool bPlaying, bool bResetDynamics)
	{
		if(!CanPlayAnimation(SkeletalMeshComponent, InAnimSequence))
		{
			return;
		}

		UAnimMontage* Montage = FAnimMontageInstance::PreviewMatineeSetAnimPositionInner(SlotName, SkeletalMeshComponent, InAnimSequence, InPosition, bLooping, bFireNotifies, DeltaTime);

		// if we are not playing, make sure we dont continue (as skeletal meshes can still tick us onwards)
		UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
		UAnimSingleNodeInstance * SingleNodeInst = SkeletalMeshComponent->GetSingleNodeInstance();
		if(SingleNodeInst)
		{
			SingleNodeInst->SetPlaying(bPlaying);
		}
		else if (AnimInst)
		{
			if(Montage)
			{
				if(bPlaying)
				{
					AnimInst->Montage_Resume(Montage);
				}
				else
				{
					AnimInst->Montage_Pause(Montage);
				}
			}

			if(bResetDynamics)
			{
				// make sure we reset any simulations
				AnimInst->ResetDynamics();
			}
		}
	}

	FMovieSceneSkeletalAnimationSectionTemplateParameters Params;
};


FMovieSceneSkeletalAnimationSectionTemplate::FMovieSceneSkeletalAnimationSectionTemplate(const UMovieSceneSkeletalAnimationSection& InSection)
	: Params(InSection.Params, InSection.GetStartTime())
{
}

void FMovieSceneSkeletalAnimationSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.Add(FAnimationTrackTrackToken(Params));
}

float FMovieSceneSkeletalAnimationSectionTemplateParameters::MapTimeToAnimation(float ThisPosition) const
{
	// @todo: Sequencer: what is this for??
	//ThisPosition -= 1 / 1000.0f;

	const float SectionPlayRate = PlayRate;
	const float AnimPlayRate = FMath::IsNearlyZero(SectionPlayRate) ? 1.0f : SectionPlayRate;

	const float SeqLength = GetSequenceLength() - (StartOffset + EndOffset);

	ThisPosition = (ThisPosition - SectionStartTime) * AnimPlayRate;
	ThisPosition = FMath::Fmod(ThisPosition, SeqLength);
	ThisPosition += StartOffset;
	if (bReverse)
	{
		ThisPosition = (SeqLength - (ThisPosition - StartOffset)) + StartOffset;
	}

	return ThisPosition;
}
