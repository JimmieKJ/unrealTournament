// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/VertexAnim/VertexAnimation.h"
#include "Animation/BlendSpace.h"

void FAnimSingleNodeInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	CurrentAsset = NULL;
	CurrentVertexAnim = NULL;
#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
#endif

	// it's already doing it when evaluate
	BlendSpaceInput = FVector::ZeroVector;
	CurrentTime = 0.f;
}

bool FAnimSingleNodeInstanceProxy::Evaluate(FPoseContext& Output)
{
	const bool bCanProcessAdditiveAnimationsLocal
#if WITH_EDITOR
		= bCanProcessAdditiveAnimations;
#else
		= false;
#endif

	if (CurrentAsset != NULL)
	{
		//@TODO: animrefactor: Seems like more code duplication than we need
		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
		{
			InternalBlendSpaceEvaluatePose(BlendSpace, BlendSampleData, Output);
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			if (Sequence->IsValidAdditive())
			{
				FAnimExtractContext ExtractionContext(CurrentTime, Sequence->bEnableRootMotion);

				if (bCanProcessAdditiveAnimationsLocal)
				{
					Sequence->GetAdditiveBasePose(Output.Pose, Output.Curve, ExtractionContext);
				}
				else
				{
					Output.ResetToRefPose();
				}

				FCompactPose AdditivePose;
				FBlendedCurve AdditiveCurve;
				AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				AdditiveCurve.InitFrom(Output.Curve);
				Sequence->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractionContext);

				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, Sequence->AdditiveAnimType);
				Output.Pose.NormalizeRotations();
			}
			else
			{
				// if SkeletalMesh isn't there, we'll need to use skeleton
				Sequence->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(CurrentTime, Sequence->bEnableRootMotion));
			}
		}
		else if (UAnimComposite* Composite = Cast<UAnimComposite>(CurrentAsset))
		{
			FAnimExtractContext ExtractionContext(CurrentTime, ShouldExtractRootMotion());
			const FAnimTrack& AnimTrack = Composite->AnimationTrack;

			// find out if this is additive animation
			if (AnimTrack.IsAdditive())
			{
#if WITH_EDITORONLY_DATA
				if (bCanProcessAdditiveAnimationsLocal && Composite->PreviewBasePose)
				{
					Composite->PreviewBasePose->GetAdditiveBasePose(Output.Pose, Output.Curve, ExtractionContext);
				}
				else
#endif
				{
					// get base pose - for now we only support ref pose as base
					Output.Pose.ResetToRefPose();
				}

				EAdditiveAnimationType AdditiveAnimType = AnimTrack.IsRotationOffsetAdditive()? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase;

				FCompactPose AdditivePose;
				FBlendedCurve AdditiveCurve;
				AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				AdditiveCurve.InitFrom(Output.Curve);
				Composite->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractionContext);

				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, AdditiveAnimType);
			}
			else
			{
				//doesn't handle additive yet
				Composite->GetAnimationPose(Output.Pose, Output.Curve, ExtractionContext);
			}
		}
		else if (UAnimMontage* Montage = Cast<UAnimMontage>(CurrentAsset))
		{
			// for now only update first slot
			// in the future, add option to see which slot to see
			if (Montage->SlotAnimTracks.Num() > 0)
			{
				FCompactPose SourcePose;
				FBlendedCurve SourceCurve;
				SourcePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				SourceCurve.InitFrom(Output.Curve);
			
				if (Montage->IsValidAdditive())
				{
#if WITH_EDITORONLY_DATA
					// if montage is additive, we need to have base pose for the slot pose evaluate
					if (bCanProcessAdditiveAnimationsLocal && Montage->PreviewBasePose && Montage->SequenceLength > 0.f)
					{
						Montage->PreviewBasePose->GetBonePose(SourcePose, SourceCurve, FAnimExtractContext(CurrentTime));
					}
					else
#endif // WITH_EDITORONLY_DATA
					{
						SourcePose.ResetToRefPose();				
					}
				}
				else
				{
					SourcePose.ResetToRefPose();			
				}

				SlotEvaluatePose(Montage->SlotAnimTracks[0].SlotName, SourcePose, SourceCurve, WeightInfo.SourceWeight, Output.Pose, Output.Curve, WeightInfo.SlotNodeWeight, WeightInfo.TotalNodeWeight);
			}
		}
	}

	if(CurrentVertexAnim != NULL)
	{
		AddVertexAnim(FActiveVertexAnim(CurrentVertexAnim, 1.f, CurrentTime));
	}

	return true;
}

void FAnimSingleNodeInstanceProxy::UpdateAnimationNode(float DeltaSeconds)
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
			MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendSpaceInput, BlendSampleData, BlendFilter, bLooping, NewPlayRate, 1.f, /*inout*/ CurrentTime, MarkerTickRecord);
#if WITH_EDITORONLY_DATA
			PreviewBasePose = BlendSpace->PreviewBasePose;
#endif
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(CurrentAsset))
		{
			FAnimTickRecord& TickRecord = CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			MakeSequenceTickRecord(TickRecord, Sequence, bLooping, NewPlayRate, 1.f, /*inout*/ CurrentTime, MarkerTickRecord);
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
			MakeSequenceTickRecord(TickRecord, Composite, bLooping, NewPlayRate, 1.f, /*inout*/ CurrentTime, MarkerTickRecord);
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
				// in the future, maybe we can support which slot
				const FName CurrentSlotNodeName = Montage->SlotAnimTracks[0].SlotName;
				GetSlotWeight(CurrentSlotNodeName, WeightInfo.SlotNodeWeight, WeightInfo.SourceWeight, WeightInfo.TotalNodeWeight);
				UpdateSlotNodeWeight(CurrentSlotNodeName, WeightInfo.SlotNodeWeight);
			}
			// get the montage position
			// @todo anim: temporarily just choose first slot and show the location
			const FMontageEvaluationState* ActiveMontageEvaluationState = GetActiveMontageEvaluationState();
			if (ActiveMontageEvaluationState)
			{
				CurrentTime = ActiveMontageEvaluationState->MontagePosition;
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
		float MoveDelta = DeltaSeconds * NewPlayRate;
		FAnimationRuntime::AdvanceTime(bLooping, MoveDelta, CurrentTime, CurrentVertexAnim->GetAnimLength());
	}

#if WITH_EDITORONLY_DATA
	if(PreviewBasePose)
	{
		float MoveDelta = DeltaSeconds * NewPlayRate;
		const bool bIsPreviewPoseLooping = true;

		FAnimationRuntime::AdvanceTime(bIsPreviewPoseLooping, MoveDelta, PreviewPoseCurrentTime, PreviewBasePose->SequenceLength);
	}
#endif
}

void FAnimSingleNodeInstanceProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
	FAnimInstanceProxy::PostUpdate(InAnimInstance);

	// sync up playing state for active montage instances
	int32 EvaluationDataIndex = 0;
	const TArray<FMontageEvaluationState>& EvaluationData = GetMontageEvaluationData();
	for (FAnimMontageInstance* MontageInstance : InAnimInstance->MontageInstances)
	{
		if (MontageInstance->Montage && MontageInstance->GetWeight() > ZERO_ANIMWEIGHT_THRESH)
		{
			// sanity check we are playing the same montage
			check(MontageInstance->Montage == EvaluationData[EvaluationDataIndex].Montage);
			MontageInstance->bPlaying = EvaluationData[EvaluationDataIndex].bIsPlaying;
			EvaluationDataIndex++;
		}
	}
}

void FAnimSingleNodeInstanceProxy::InternalBlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, FPoseContext& OutContext)
{
	if (BlendSpace->IsValidAdditive())
	{
		FCompactPose& OutPose = OutContext.Pose;
		FBlendedCurve& OutCurve = OutContext.Curve;
		FCompactPose AdditivePose;
		FBlendedCurve AdditiveCurve;
		AdditivePose.SetBoneContainer(&OutPose.GetBoneContainer());
		AdditiveCurve.InitFrom(OutCurve);

#if WITH_EDITORONLY_DATA
		if (BlendSpace->PreviewBasePose)
		{
			BlendSpace->PreviewBasePose->GetBonePose(/*out*/ OutPose, /*out*/OutCurve, FAnimExtractContext(PreviewPoseCurrentTime));
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			// otherwise, get ref pose
			OutPose.ResetToRefPose();
		}

		BlendSpace->GetAnimationPose(BlendSampleDataCache, AdditivePose, AdditiveCurve);

		enum EAdditiveAnimationType AdditiveType = BlendSpace->bRotationBlendInMeshSpace? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase;

		FAnimationRuntime::AccumulateAdditivePose(OutPose, AdditivePose, OutCurve, AdditiveCurve, 1.f, AdditiveType);
	}
	else
	{
		BlendSpace->GetAnimationPose(BlendSampleDataCache, OutContext.Pose, OutContext.Curve);
	}
}

void FAnimSingleNodeInstanceProxy::SetAnimationAsset(class UAnimationAsset* NewAsset, USkeletalMeshComponent* MeshComponent, bool bIsLooping, float InPlayRate)
{
	if (NewAsset != CurrentAsset)
	{
		CurrentAsset = NewAsset;
	}

	if (
#if WITH_EDITOR
		!bCanProcessAdditiveAnimations &&
#endif
		NewAsset && NewAsset->IsValidAdditive())
	{
		UE_LOG(LogAnimation, Warning, TEXT("Setting an additve animation (%s) on an AnimSingleNodeInstance is not allowed. This will not function correctly in cooked builds!"), *NewAsset->GetName());
	}

	if (MeshComponent)
	{
		if (MeshComponent->SkeletalMesh == NULL)
		{
			// if it does not have SkeletalMesh, we nullify it
			CurrentAsset = NULL;
		}
		else if (CurrentAsset != NULL)
		{
			// if we have an asset, make sure their skeleton matches, otherwise, null it
			if (GetSkeleton() != CurrentAsset->GetSkeleton())
			{
				// clear asset since we do not have matching skeleton
				CurrentAsset = NULL;
			}
		}
	}

	bLooping = bIsLooping;
	PlayRate = InPlayRate;
	CurrentTime = 0.f;
	BlendSpaceInput = FVector::ZeroVector;
	BlendSampleData.Reset();
	MarkerTickRecord.Reset();
	UpdateBlendspaceSamples(BlendSpaceInput);

#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
#endif

	UBlendSpaceBase * BlendSpace = Cast<UBlendSpaceBase>(NewAsset);
	if (BlendSpace)
	{
		BlendSpace->InitializeFilter(&BlendFilter);
	}
}

void FAnimSingleNodeInstanceProxy::UpdateBlendspaceSamples(FVector InBlendInput)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
	{
		float OutCurrentTime = 0.f;
		FMarkerTickRecord TempMarkerTickRecord;
		BlendSpaceAdvanceImmediate(BlendSpace, InBlendInput, BlendSampleData, BlendFilter, false, 1.f, 0.f, OutCurrentTime, TempMarkerTickRecord);
	}
}

void FAnimSingleNodeInstanceProxy::SetVertexAnimation(UVertexAnimation * NewVertexAnim, bool bIsLooping, float InPlayRate)
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
}

void FAnimSingleNodeInstanceProxy::SetReverse(bool bInReverse)
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

float FAnimSingleNodeInstanceProxy::GetLength()
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

void FAnimSingleNodeInstanceProxy::SetBlendSpaceInput(const FVector& InBlendInput)
{
	BlendSpaceInput = InBlendInput;
}
