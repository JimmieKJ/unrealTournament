// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneSkeletalAnimationTrackInstance.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "IMovieScenePlayer.h"
#include "Matinee/MatineeAnimInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "MovieSceneCommonHelpers.h"

float MapTimeToAnimation(float ThisPosition, UMovieSceneSkeletalAnimationSection* AnimSection)
{
	ThisPosition = FMath::Clamp(ThisPosition, AnimSection->GetStartTime(), AnimSection->GetEndTime());

	float AnimPlayRate = FMath::IsNearlyZero(AnimSection->GetPlayRate()) ? 1.0f : AnimSection->GetPlayRate();
	float SeqLength = AnimSection->GetSequenceLength() - (AnimSection->GetStartOffset() + AnimSection->GetEndOffset());

	ThisPosition = (ThisPosition - AnimSection->GetStartTime()) * AnimPlayRate;
	ThisPosition = FMath::Fmod(ThisPosition, SeqLength);
	ThisPosition += AnimSection->GetStartOffset();
	if (AnimSection->GetReverse())
	{
		ThisPosition = (SeqLength - (ThisPosition - AnimSection->GetStartOffset())) + AnimSection->GetStartOffset();
	}

	return ThisPosition;
};

FMovieSceneSkeletalAnimationTrackInstance::FMovieSceneSkeletalAnimationTrackInstance( UMovieSceneSkeletalAnimationTrack& InAnimationTrack )
{
	AnimationTrack = &InAnimationTrack;
}


FMovieSceneSkeletalAnimationTrackInstance::~FMovieSceneSkeletalAnimationTrackInstance()
{
}

void FMovieSceneSkeletalAnimationTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
	{
		// get the skeletal mesh component we want to control
		USkeletalMeshComponent* Component = nullptr;
		UObject* RuntimeObject = RuntimeObjects[i].Get();

		if (RuntimeObject != nullptr)
		{
			// first try to control the component directly
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RuntimeObject);
			if (SkeletalMeshComponent == nullptr)
			{
				// then check to see if we are controlling an actor & if so use its first USkeletalMeshComponent 
				AActor* Actor = Cast<AActor>(RuntimeObject);
				if(Actor != nullptr)
				{
					SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
				}
			}

			if (SkeletalMeshComponent)
			{
				if (ShouldUsePreviewPlayback(Player, RuntimeObject))
				{
					PreviewBeginAnimControl(SkeletalMeshComponent);
				}
				else
				{
					BeginAnimControl(SkeletalMeshComponent);
				}
			}
		}
	}
}

void FMovieSceneSkeletalAnimationTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (int32 i = 0; i < RuntimeObjects.Num(); ++i)
	{
		// get the skeletal mesh component we want to control
		USkeletalMeshComponent* Component = nullptr;
		UObject* RuntimeObject = RuntimeObjects[i].Get();

		if (RuntimeObject != nullptr)
		{
			// first try to control the component directly
			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(RuntimeObject);
			if (SkeletalMeshComponent == nullptr)
			{
				// then check to see if we are controlling an actor & if so use its first USkeletalMeshComponent 
				AActor* Actor = Cast<AActor>(RuntimeObject);
				if(Actor != nullptr)
				{
					SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
				}
			}

			if (SkeletalMeshComponent)
			{
				if (ShouldUsePreviewPlayback(Player, RuntimeObject))
				{
					PreviewFinishAnimControl(SkeletalMeshComponent);
				}
				else
				{
					FinishAnimControl(SkeletalMeshComponent);
				}
			}
		}
	}
}

USkeletalMeshComponent* GetSkeletalMeshComponentFromRuntimeObjectPtr( TWeakObjectPtr<UObject> RuntimeObjectPtr )
{
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	UObject* RuntimeObject = RuntimeObjectPtr.Get();
	if ( RuntimeObject != nullptr )
	{
		// first try to control the component directly
		SkeletalMeshComponent = Cast<USkeletalMeshComponent>( RuntimeObject );
		if ( SkeletalMeshComponent == nullptr )
		{
			// then check to see if we are controlling an actor & if so use its first USkeletalMeshComponent 
			AActor* Actor = Cast<AActor>( RuntimeObject );
			if ( Actor != nullptr )
			{
				SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
			}
		}
	}
	return SkeletalMeshComponent;
}

void FMovieSceneSkeletalAnimationTrackInstance::Update( EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	// @todo Sequencer gameplay update has a different code path than editor update for animation
	for ( TWeakObjectPtr<UObject> RuntimeObjectPtr : RuntimeObjects )
	{
		USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponentFromRuntimeObjectPtr( RuntimeObjectPtr );
		if ( SkeletalMeshComponent )
		{
			TArray<UMovieSceneSection*> AnimSections = AnimationTrack->GetAnimSectionsAtTime( UpdateData.Position );

			// cbb: If there is no overlapping section, evaluate the closest section only if the current time is before it.
			if (AnimSections.Num() == 0)
			{
				UMovieSceneSection* NearestSection = MovieSceneHelpers::FindNearestSectionAtTime( AnimationTrack->GetAllSections(), UpdateData.Position );
				if (NearestSection)
				{
					AnimSections.Add(NearestSection);
				}
			}

			for (int32 AnimSectionIndex = 0; AnimSectionIndex < AnimSections.Num(); ++AnimSectionIndex)
			{
				UMovieSceneSkeletalAnimationSection* AnimSection = Cast<UMovieSceneSkeletalAnimationSection>( AnimSections[AnimSectionIndex] );

				if ( AnimSection && AnimSection->IsActive() )
				{
					UAnimSequenceBase* AnimSequence = AnimSection->GetAnimSequence();

					if ( AnimSequence )
					{
						float EvalTime = MapTimeToAnimation( UpdateData.Position, AnimSection );

						int32 ChannelIndex = 0;

						const bool bLooping = false;

						if ( ShouldUsePreviewPlayback( Player, SkeletalMeshComponent ) )
						{
							// If the playback status is jumping, ie. one such occurrence is setting the time for thumbnail generation, disable anim notifies updates because it could fire audio
							const bool bFireNotifies = Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Jumping || Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Stopped ? false : true;

							float DeltaTime = UpdateData.Position > UpdateData.LastPosition ? UpdateData.Position - UpdateData.LastPosition : 0.f;

							// When jumping from one cut to another cut, the delta time should be 0 so that anim notifies before the current position are not evaluated. Note, anim notifies at the current time should still be evaluated.
							if ( UpdateData.bJumpCut )
							{
								DeltaTime = 0.f;
							}

							const bool bResetDynamics = Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Stepping ||
								Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Jumping ||
								Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Scrubbing ||
								( DeltaTime == 0.0f && Player.GetPlaybackStatus() != EMovieScenePlayerStatus::Stopped );

							PreviewSetAnimPosition( SkeletalMeshComponent, AnimSection->GetSlotName(), ChannelIndex, AnimSequence, EvalTime, bLooping, bFireNotifies, DeltaTime, Player.GetPlaybackStatus() == EMovieScenePlayerStatus::Playing, bResetDynamics );
						}
						else
						{
							// Don't fire notifies at runtime since they will be fired through the standard animation tick path.
							const bool bFireNotifies = false;
							SetAnimPosition( SkeletalMeshComponent, AnimSection->GetSlotName(), ChannelIndex, AnimSequence, EvalTime, bLooping, bFireNotifies );
						}
					}
				}
			}
		}
	}
}

void FMovieSceneSkeletalAnimationTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	// When not in preview playback we need to stop any running animations to prevent the animations from being advanced a frame when
	// they are ticked by the engine.
	for ( TWeakObjectPtr<UObject> RuntimeObjectPtr : RuntimeObjects )
	{
		if ( ShouldUsePreviewPlayback( Player, RuntimeObjectPtr.Get() ) == false )
		{
			USkeletalMeshComponent* SkeletalMeshComponent = GetSkeletalMeshComponentFromRuntimeObjectPtr( RuntimeObjectPtr );
			if ( SkeletalMeshComponent != nullptr )
			{
				UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
				if ( AnimInstance )
				{
					UAnimSingleNodeInstance * SingleNodeInstance = SkeletalMeshComponent->GetSingleNodeInstance();
					if ( SingleNodeInstance )
					{
						SingleNodeInstance->SetPlaying( false );
					}
					// TODO: Anim montage?
				}
			}
		}
	}
}

void FMovieSceneSkeletalAnimationTrackInstance::BeginAnimControl(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (CanPlayAnimation(SkeletalMeshComponent))
	{
		UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
		if (!AnimInstance)
		{
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		}
	}

	CurrentlyPlayingMontage = nullptr;
}

bool FMovieSceneSkeletalAnimationTrackInstance::CanPlayAnimation(USkeletalMeshComponent* SkeletalMeshComponent, class UAnimSequenceBase* AnimAssetBase/*=nullptr*/) const
{
	return (SkeletalMeshComponent->SkeletalMesh && SkeletalMeshComponent->SkeletalMesh->Skeleton && 
		(!AnimAssetBase || SkeletalMeshComponent->SkeletalMesh->Skeleton->IsCompatible(AnimAssetBase->GetSkeleton())));
}

void FMovieSceneSkeletalAnimationTrackInstance::SetAnimPosition(USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, int32 ChannelIndex, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies)
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

void FMovieSceneSkeletalAnimationTrackInstance::FinishAnimControl(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if(SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationBlueprint)
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

	CurrentlyPlayingMontage = nullptr;
}


void FMovieSceneSkeletalAnimationTrackInstance::PreviewBeginAnimControl(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (CanPlayAnimation(SkeletalMeshComponent))
	{
		UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
		if (!AnimInstance)
		{
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::Type::AnimationSingleNode);
		}
	}

	CurrentlyPlayingMontage = nullptr;
}

void FMovieSceneSkeletalAnimationTrackInstance::PreviewFinishAnimControl(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (CanPlayAnimation(SkeletalMeshComponent))
	{
		// if in editor, reset the Animations, makes easier for artist to see them visually and align them
		// in game, we keep the last pose that matinee kept. If you'd like it to have animation, you'll need to have AnimTree or AnimGraph to handle correctly
		if (SkeletalMeshComponent->GetAnimationMode() == EAnimationMode::Type::AnimationBlueprint)
		{
			UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
			if(AnimInstance)
			{
				AnimInstance->Montage_Stop(0.f);
				AnimInstance->UpdateAnimation(0.f, false);
			}
		}
		// Update space bases to reset it back to ref pose
		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->RefreshSlaveComponents();
		SkeletalMeshComponent->UpdateComponentToWorld();
	}

	CurrentlyPlayingMontage = nullptr;
}

void FMovieSceneSkeletalAnimationTrackInstance::PreviewSetAnimPosition(USkeletalMeshComponent* SkeletalMeshComponent, FName SlotName, int32 ChannelIndex, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float DeltaTime, bool bPlaying, bool bResetDynamics)
{
	if(CanPlayAnimation(SkeletalMeshComponent, InAnimSequence))
	{
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
}

bool FMovieSceneSkeletalAnimationTrackInstance::ShouldUsePreviewPlayback(class IMovieScenePlayer& Player, UObject* RuntimeObject) const
{
	// we also use PreviewSetAnimPosition in PIE when not playing, as we can preview in PIE
	bool bIsNotInPIEOrNotPlaying = (RuntimeObject && RuntimeObject->GetWorld() && !RuntimeObject->GetWorld()->HasBegunPlay()) || Player.GetPlaybackStatus() != EMovieScenePlayerStatus::Playing;
	return GIsEditor && bIsNotInPIEOrNotPlaying;
}
