// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UAnimSingleNodeInstance.cpp: Single Node Tree Instance 
	Only plays one animation at a time. 
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/VertexAnim/VertexAnimation.h"
#include "AnimationRuntime.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AnimComposite.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"

/////////////////////////////////////////////////////
// UAnimSingleNodeInstance
/////////////////////////////////////////////////////

UAnimSingleNodeInstance::UAnimSingleNodeInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PlayRate(1.f)
	, bLooping(true)
	, bPlaying(true)
	, bReverse(false)
{
}

void UAnimSingleNodeInstance::SetAnimationAsset(class UAnimationAsset* NewAsset,bool bIsLooping,float InPlayRate)
{
	if (NewAsset != CurrentAsset)
	{
		CurrentAsset = NewAsset;
	}

	if (USkeletalMeshComponent * MeshComponent = GetSkelMeshComponent())
	{
		if (MeshComponent->SkeletalMesh == NULL)
		{
			// if it does not have SkeletalMesh, we nullify it
			CurrentAsset = NULL;
		}
		else if (CurrentAsset != NULL)
		{
			// if we have an asset, make sure their skeleton matches, otherwise, null it
			if (CurrentSkeleton != CurrentAsset->GetSkeleton())
			{
				// clear asset since we do not have matching skeleton
				CurrentAsset = NULL;
			}
		}
	}

	bLooping = bIsLooping;
	PlayRate = InPlayRate;
	CurrentTime = 0.f;
#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
#endif

	UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset);
	if ( Montage!=NULL )
	{
		ReinitializeSlotNodes();
		if ( Montage->SlotAnimTracks.Num() > 0 )
		{
			RegisterSlotNodeWithAnimInstance(Montage->SlotAnimTracks[0].SlotName);
		}
		RestartMontage( Montage );
		SetPlaying(bPlaying);
	}
	else
	{
		// otherwise stop all montages
		StopAllMontages(0.25f);
	}
}

void UAnimSingleNodeInstance::SetVertexAnimation(UVertexAnimation * NewVertexAnim, bool bIsLooping, float InPlayRate)
{
	if (NewVertexAnim != CurrentVertexAnim)
	{
		CurrentVertexAnim = NewVertexAnim;
	}

	if (USkeletalMeshComponent * MeshComponent = GetSkelMeshComponent())
	{
		if (MeshComponent->SkeletalMesh == NULL)
		{
			// if it does not have SkeletalMesh, we nullify it
			CurrentVertexAnim = NULL;
		}
		else if (CurrentVertexAnim != NULL)
		{
			// if we have an anim, make sure their mesh matches, otherwise, null it
			if (MeshComponent->SkeletalMesh != CurrentVertexAnim->BaseSkelMesh)
			{
				// clear asset since we do not have matching skeleton
				CurrentVertexAnim = NULL;
			}
		}
	}

	bLooping = bIsLooping;
	PlayRate = InPlayRate;

	// reinitialize
	InitializeAnimation();
}


void UAnimSingleNodeInstance::SetMontageLoop(UAnimMontage* Montage, bool bIsLooping, FName StartingSection)
{
	check (Montage);

	int32 TotalSection = Montage->CompositeSections.Num();
	if( TotalSection > 0 )
	{
		if (StartingSection == NAME_None)
		{
			StartingSection = Montage->CompositeSections[0].SectionName;
		}
		FName FirstSection = StartingSection;
		FName LastSection = StartingSection;

		bool bSucceeded = false;
		// find last section
		int32 CurSection = Montage->GetSectionIndex(FirstSection);

		int32 Count = TotalSection;
		while( Count-- > 0 )
		{
			FName NewLastSection = Montage->CompositeSections[CurSection].NextSectionName;
			CurSection = Montage->GetSectionIndex(NewLastSection);

			if( CurSection != INDEX_NONE )
			{
				// used to rebuild next/prev
				Montage_SetNextSection(LastSection, NewLastSection);
				LastSection = NewLastSection;
			}
			else
			{
				bSucceeded = true;
				break;
			}
		}

		if( bSucceeded )
		{
			if ( bIsLooping )
			{
				Montage_SetNextSection(LastSection, FirstSection);
			}
			else
			{
				Montage_SetNextSection(LastSection, NAME_None);
			}
		}
		// else the default is already looping
	}
}

void UAnimSingleNodeInstance::UpdateMontageWeightForTimeSkip(float TimeDifference)
{
	Montage_UpdateWeight(TimeDifference);
}

void UAnimSingleNodeInstance::UpdateBlendspaceSamples(FVector InBlendInput)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
	{
		float OutCurrentTime = 0.f;
		BlendSpaceAdvanceImmediate(BlendSpace, InBlendInput, BlendSampleData, BlendFilter, false, 1.f, 0.f, OutCurrentTime);
	}
}

void UAnimSingleNodeInstance::RestartMontage(UAnimMontage * Montage, FName FromSection)
{
	if( Montage == CurrentAsset )
	{
		Montage_Play(Montage, PlayRate);
		if( FromSection != NAME_None )
		{
			Montage_JumpToSection(FromSection);
		}
		SetMontageLoop(Montage, bLooping, FromSection);
	}
}

void UAnimSingleNodeInstance::NativeInitializeAnimation()
{
	CurrentAsset = NULL;
	CurrentVertexAnim = NULL;
#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
#endif

	// it's already doing it when evaluate
	BlendSpaceInput = FVector::ZeroVector;
	CurrentTime = 0.f;
	USkeletalMeshComponent* SkelComp = GetSkelMeshComponent();
	SkelComp->AnimationData.Initialize(this);

	if ( CurrentAsset!=NULL )
	{
		UBlendSpace * BlendSpace = Cast<UBlendSpace>(CurrentAsset);
		if (BlendSpace)
		{
			BlendSpace->InitializeFilter(&BlendFilter);
		}
	}
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaTimeX)
{
	float NewPlayRate = PlayRate;
	UAnimSequence* PreviewBasePose = NULL;

	if (bPlaying == false)
	{
		// we still have to tick animation when bPlaying is false because 
		NewPlayRate = 0.f;
	}

	if(CurrentAsset != NULL)
	{
		FAnimGroupInstance* SyncGroup;
		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
		{
			FAnimTickRecord& TickRecord = CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendSpaceInput, BlendSampleData, BlendFilter, bLooping, NewPlayRate, 1.f, /*inout*/ CurrentTime);
#if WITH_EDITORONLY_DATA
			PreviewBasePose = BlendSpace->PreviewBasePose;
#endif
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			FAnimTickRecord& TickRecord = CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			MakeSequenceTickRecord(TickRecord, Sequence, bLooping, NewPlayRate, 1.f, /*inout*/ CurrentTime);
			// if it's not looping, just set play to be false when reached to end
			if (!bLooping)
			{
				const float CombinedPlayRate = NewPlayRate*Sequence->RateScale;
				if ((CombinedPlayRate < 0.f && CurrentTime <= 0.f) || (CombinedPlayRate > 0.f && CurrentTime >= Sequence->SequenceLength))
				{
					SetPlaying(false);
				}
			}
		}
		else if(UAnimComposite* Composite = Cast<UAnimComposite>(CurrentAsset))
		{
			FAnimTickRecord& TickRecord = CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			MakeSequenceTickRecord(TickRecord, Composite, bLooping, NewPlayRate, 1.f, /*inout*/ CurrentTime);
			// if it's not looping, just set play to be false when reached to end
			if (!bLooping)
			{
				const float CombinedPlayRate = NewPlayRate*Composite->RateScale;
				if ((CombinedPlayRate < 0.f && CurrentTime <= 0.f) || (CombinedPlayRate > 0.f && CurrentTime >= Composite->SequenceLength))
				{
					SetPlaying(false);
				}
			}
		}
		else if (UAnimMontage * Montage = Cast<UAnimMontage>(CurrentAsset))
		{
			// Full weight , if you don't have slot track, you won't be able to see animation playing
			if ( Montage->SlotAnimTracks.Num() > 0 )
			{
				UpdateSlotNodeWeight(Montage->SlotAnimTracks[0].SlotName, 1.f);		
			}
			// get the montage position
			// @todo anim: temporarily just choose first slot and show the location
			FAnimMontageInstance * ActiveMontageInstance = GetActiveMontageInstance();
			if (ActiveMontageInstance)
			{
				CurrentTime = ActiveMontageInstance->GetPosition();
			}
			else if (bPlaying)
			{
				SetPlaying(false);
			}
#if WITH_EDITORONLY_DATA
			PreviewBasePose = Montage->PreviewBasePose;
#endif
		}
	}
	else if(CurrentVertexAnim != NULL)
	{
		float MoveDelta = DeltaTimeX * NewPlayRate;
		FAnimationRuntime::AdvanceTime(bLooping, MoveDelta, CurrentTime, CurrentVertexAnim->GetAnimLength());
	}

#if WITH_EDITORONLY_DATA
	if(PreviewBasePose)
	{
		float MoveDelta = DeltaTimeX * NewPlayRate;
		const bool bIsPreviewPoseLooping = true;

		FAnimationRuntime::AdvanceTime(bIsPreviewPoseLooping, MoveDelta, PreviewPoseCurrentTime, PreviewBasePose->SequenceLength);
	}
#endif
}

bool UAnimSingleNodeInstance::NativeEvaluateAnimation(FPoseContext& Output)
{
	if (CurrentAsset != NULL)
	{
		//@TODO: animrefactor: Seems like more code duplication than we need
		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
		{
			InternalBlendSpaceEvaluatePose(BlendSpace, BlendSampleData, Output.Pose);
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			if (Sequence->IsValidAdditive())
			{
				FA2Pose BasePose;
				FA2Pose AdditivePose;

				BasePose.Bones.AddUninitialized(Output.Pose.Bones.Num());
				AdditivePose.Bones.AddUninitialized(Output.Pose.Bones.Num());

				FAnimExtractContext ExtractionContext(CurrentTime, Sequence->bEnableRootMotion);
				Sequence->GetAdditiveBasePose(BasePose.Bones, RequiredBones, ExtractionContext);
				Sequence->GetAnimationPose(AdditivePose.Bones, RequiredBones, ExtractionContext);
				if (Sequence->AdditiveAnimType == AAT_LocalSpaceBase)
				{
					ApplyAdditiveSequence(BasePose, AdditivePose, 1.0f, Output.Pose);
				}
				else
				{
					BlendRotationOffset(BasePose, AdditivePose, 1.0f, Output.Pose);
				}
			}
			else
			{
				// if sekeltalmesh isn't there, we'll need to use skeleton
				FAnimationRuntime::GetPoseFromSequence(Sequence, RequiredBones, Output.Pose.Bones, FAnimExtractContext(CurrentTime, Sequence->bEnableRootMotion));
			}
		}
		else if (UAnimComposite* Composite = Cast<UAnimComposite>(CurrentAsset))
		{
			const FAnimTrack& AnimTrack = Composite->AnimationTrack;

			// find out if this is additive animation
			EAdditiveAnimationType AdditiveAnimType = AAT_None;
			if (AnimTrack.IsAdditive())
			{
				FA2Pose AdditivePose;
				FA2Pose BasePose;

				BasePose.Bones.AddUninitialized(Output.Pose.Bones.Num());
				AdditivePose.Bones.AddUninitialized(Output.Pose.Bones.Num());
				AdditiveAnimType = AnimTrack.IsRotationOffsetAdditive()? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase;
				
				// get base pose - for now we only support ref pose as base
				FAnimationRuntime::FillWithRefPose(BasePose.Bones, RequiredBones);
				
				//get the additive pose
				FAnimationRuntime::GetPoseFromAnimTrack(AnimTrack, RequiredBones, AdditivePose.Bones, FAnimExtractContext(CurrentTime, RootMotionMode == ERootMotionMode::RootMotionFromEverything));

				// if additive, we should blend with source to make it fullbody
				if (AdditiveAnimType == AAT_LocalSpaceBase)
				{
					ApplyAdditiveSequence(BasePose,AdditivePose,1.f,Output.Pose);
				}
				else if (AdditiveAnimType == AAT_RotationOffsetMeshSpace)
				{
					BlendRotationOffset(BasePose,AdditivePose,1.f,Output.Pose);
				}
			}
			else
			{
				//doesn't handle additive yet
				FAnimationRuntime::GetPoseFromAnimTrack(AnimTrack, RequiredBones, Output.Pose.Bones, FAnimExtractContext(CurrentTime, RootMotionMode == ERootMotionMode::RootMotionFromEverything));
			}
		}
		else if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
		{
			// for now only update first slot
			// in the future, add option to see which slot to see
			if (Montage->SlotAnimTracks.Num() > 0)
			{
				FA2Pose SourcePose;
				SourcePose.Bones.AddUninitialized(Output.Pose.Bones.Num());
				if (Montage->IsValidAdditive())
				{
#if WITH_EDITORONLY_DATA
					// if montage is additive, we need to have base pose for the slot pose evaluate
					if (Montage->PreviewBasePose && Montage->SequenceLength > 0.f)
					{
						Montage->PreviewBasePose->GetBonePose(SourcePose.Bones, RequiredBones, FAnimExtractContext(CurrentTime));
					}
					else
#endif // WITH_EDITORONLY_DATA
					{
						FAnimationRuntime::FillWithRefPose(SourcePose.Bones, RequiredBones);				
					}
				}
				else
				{
					FAnimationRuntime::FillWithRefPose(SourcePose.Bones, RequiredBones);				
				}

				SlotEvaluatePose(Montage->SlotAnimTracks[0].SlotName, SourcePose, Output.Pose, 1.f);
			}
		}
	}

	if(CurrentVertexAnim != NULL)
	{
		VertexAnims.Add(FActiveVertexAnim(CurrentVertexAnim, 1.f, CurrentTime));
	}

	return true;
}

void UAnimSingleNodeInstance::PostAnimEvaluation()
{
	PostEvaluateAnimEvent.ExecuteIfBound();

	Super::PostAnimEvaluation();
}

void UAnimSingleNodeInstance::Montage_Advance(float DeltaTime)
{
	Super::Montage_Advance(DeltaTime);
		
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		CurrentTime = CurMontageInstance->GetPosition();
	}
}

void UAnimSingleNodeInstance::PlayAnim(bool bIsLooping, float InPlayRate, float InStartPosition)
{
	SetPlaying(true);
	SetLooping(bIsLooping);
	SetPlayRate(InPlayRate);
	SetPosition(InStartPosition);
}

void UAnimSingleNodeInstance::StopAnim()
{
	SetPlaying(false);
}

void UAnimSingleNodeInstance::SetLooping(bool bIsLooping)
{
	bLooping = bIsLooping;

	if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
	{
		SetMontageLoop(Montage, bLooping, Montage_GetCurrentSection());
	}
}

void UAnimSingleNodeInstance::SetPlaying(bool bIsPlaying)
{
	bPlaying = bIsPlaying;

	if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
	{
		CurMontageInstance->bPlaying = bIsPlaying;
	}
	else if (bPlaying)
	{
		UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset);
		if (Montage)
		{
			RestartMontage(Montage);
		}
	}
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
	PlayRate = InPlayRate;

	if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
	{
		CurMontageInstance->SetPlayRate(InPlayRate);
	}
}

void UAnimSingleNodeInstance::SetReverse(bool bInReverse)
{
	bReverse = bInReverse;
	if (bInReverse)
	{
		PlayRate = -FMath::Abs(PlayRate);
	}
	else
	{
		PlayRate = FMath::Abs(PlayRate);
	}

// reverse support is a bit tricky for montage
// since we don't have delegate when it reached to the beginning
// for now I comment this out and do not support
// I'd like the buttons to be customizable per asset types -
// 	TTP 233456	ANIM: support different scrub controls per asset type
/*
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		if ( bReverse == (CurMontageInstance->PlayRate > 0.f) )
		{
			CurMontageInstance->PlayRate *= -1.f;
		}
	}*/
}

void UAnimSingleNodeInstance::SetPosition(float InPosition, bool bFireNotifies)
{
	float PreviousTime = CurrentTime;
	CurrentTime = FMath::Clamp<float>(InPosition, 0.f, GetLength());

	if (FAnimMontageInstance* CurMontageInstance = GetActiveMontageInstance())
	{
		CurMontageInstance->SetPosition(CurrentTime);
	}

	// Handle notifies
	// the way AnimInstance handles notifies doesn't work for single node because this does not tick or anything
	// this will need to handle manually, emptying, it and collect it, and trigger them at once. 
	if (bFireNotifies)
	{
		UAnimSequenceBase * SequenceBase = Cast<UAnimSequenceBase> (CurrentAsset);
		if (SequenceBase)
		{
			AnimNotifies.Empty();

			TArray<const FAnimNotifyEvent*> Notifies;
			SequenceBase->GetAnimNotifiesFromDeltaPositions(PreviousTime, CurrentTime, Notifies);
			if ( Notifies.Num() > 0 )
			{
				// single node instance only has 1 asset at a time
				AddAnimNotifies(Notifies, 1.0f);
			}

			TriggerAnimNotifies(0.f);

			// since this is singlenode instance, if position changes, we can't keep old morphtarget curves
			// we clear it and evaluate curve here with new asset. 
			MorphTargetCurves.Empty();
			MaterialParameterCurves.Empty();
			// Evaluate Curve data now - even if time did not move, we still need to return curve if it exists
			SequenceBase->EvaluateCurveData(this, CurrentTime, 1.0);
		}
	}
}

void UAnimSingleNodeInstance::SetBlendSpaceInput(const FVector& InBlendInput)
{
	BlendSpaceInput = InBlendInput;
}

void UAnimSingleNodeInstance::InternalBlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, struct FA2Pose& Pose)
{
	USkeletalMeshComponent* Component = GetSkelMeshComponent();

	if (BlendSpace->IsValidAdditive())
	{
		FA2Pose BasePose;
		FA2Pose AdditivePose;
		BasePose.Bones.AddUninitialized(Pose.Bones.Num());
		AdditivePose.Bones.AddUninitialized(Pose.Bones.Num());

#if WITH_EDITORONLY_DATA
		if (BlendSpace->PreviewBasePose)
		{
			BlendSpace->PreviewBasePose->GetBonePose(/*out*/ BasePose.Bones, RequiredBones, FAnimExtractContext(PreviewPoseCurrentTime));
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			// otherwise, get ref pose
			FAnimationRuntime::FillWithRefPose(BasePose.Bones, RequiredBones);
		}

		FAnimationRuntime::GetPoseFromBlendSpace(
			BlendSpace,
			BlendSampleDataCache, 
			RequiredBones,
			/*out*/ AdditivePose.Bones);

		if (BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) ||
			BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass()) )
		{
			BlendRotationOffset(BasePose, AdditivePose, 1.f, Pose);
		}
		else
		{
			ApplyAdditiveSequence(BasePose, AdditivePose, 1.f, Pose);
		}
	}
	else
	{
		FAnimationRuntime::GetPoseFromBlendSpace(
			BlendSpace,
			BlendSampleDataCache, 
			RequiredBones,
			/*out*/ Pose.Bones);
	}
}

float UAnimSingleNodeInstance::GetLength()
{
	if ((CurrentAsset != NULL))
	{
		if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(CurrentAsset))
		{
			return BlendSpace->AnimLength;
		}
		else if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(CurrentAsset))
		{
			return SequenceBase->SequenceLength;
		}
	}	

	return 0.f;
}

void UAnimSingleNodeInstance::StepForward()
{
	if ((CurrentAsset != NULL))
	{
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			float KeyLength = Sequence->SequenceLength/Sequence->NumFrames+SMALL_NUMBER;
			float Fraction = (CurrentTime+KeyLength)/Sequence->SequenceLength;
			int32 Frames = FMath::Clamp<int32>((float)(Sequence->NumFrames*Fraction), 0, Sequence->NumFrames);
			SetPosition(Frames*KeyLength);
		}
	}	
}

void UAnimSingleNodeInstance::StepBackward()
{
	if ((CurrentAsset != NULL))
	{
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			float KeyLength = Sequence->SequenceLength/Sequence->NumFrames+SMALL_NUMBER;
			float Fraction = (CurrentTime-KeyLength)/Sequence->SequenceLength;
			int32 Frames = FMath::Clamp<int32>((float)(Sequence->NumFrames*Fraction), 0, Sequence->NumFrames);
			SetPosition(Frames*KeyLength);
		}
	}	
}
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

