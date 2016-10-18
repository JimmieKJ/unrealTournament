// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AnimSingleNodeInstanceProxy.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/PoseAsset.h"
#include "Animation/AnimSingleNodeInstance.h"

void FAnimSingleNodeInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	CurrentAsset = NULL;
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

	if (CurrentAsset != NULL &&
//@HSL_BEGIN - CCL - Seeing crashes due to processing animations that have begun to be destroyed
		!CurrentAsset->HasAnyFlags(RF_BeginDestroyed))
//@HSL_END
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
		else if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(CurrentAsset))
		{
			const TArray<FSmartName>& PoseNames = PoseAsset->GetPoseNames();

			int32 TotalPoses = PoseNames.Num();
			FAnimExtractContext ExtractContext;
			ExtractContext.PoseCurves.AddZeroed(TotalPoses);

			USkeleton* MySkeleton = PoseAsset->GetSkeleton();
 			for (auto Iter=PreviewCurveOverride.CreateConstIterator(); Iter; ++Iter)
 			{
 				const FName& Name = Iter.Key();
				const float Value = Iter.Value();
				
				FSmartName PoseName;
				
				if (MySkeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, Name, PoseName))
				{
					int32 PoseIndex = PoseNames.Find(PoseName);
					if (PoseIndex != INDEX_NONE)
					{
						ExtractContext.PoseCurves[PoseIndex] = Value;
					}
				}
 			}

 			if (PoseAsset->IsValidAdditive())
 			{
 				PoseAsset->GetBaseAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext());

				FCompactPose AdditivePose;
				FBlendedCurve AdditiveCurve;
				AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				AdditiveCurve.InitFrom(Output.Curve);
				float Weight = PoseAsset->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractContext);
				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
 			}
 			else
 			{
 				PoseAsset->GetAnimationPose(Output.Pose, Output.Curve, ExtractContext);
 			}
		}

#if WITH_EDITORONLY_DATA
		// have to propagate output curve before pose asset as it can use pose curve data
		PropagatePreviewCurve(Output);

		// if it has preview pose asset, we have to handle that after we do all animation
		if (const UPoseAsset* PoseAsset = CurrentAsset->PreviewPoseAsset)
		{
			USkeleton* MySkeleton = CurrentAsset->GetSkeleton();
			// if skeleton doesn't match it won't work
			if (PoseAsset->GetSkeleton() == MySkeleton)
			{
				const TArray<FSmartName>& PoseNames = PoseAsset->GetPoseNames();

				int32 TotalPoses = PoseNames.Num();
				FAnimExtractContext ExtractContext;
				ExtractContext.PoseCurves.AddZeroed(TotalPoses);

				for (const auto& PoseName : PoseNames)
				{
					if (PoseName.UID != FSmartNameMapping::MaxUID)
					{
						int32 PoseIndex = PoseNames.Find(PoseName);
						if (PoseIndex != INDEX_NONE)
						{
							ExtractContext.PoseCurves[PoseIndex] = Output.Curve.Get(PoseName.UID);
						}
					}
				}

				if (PoseAsset->IsValidAdditive())
				{
					FCompactPose AdditivePose;
					FBlendedCurve AdditiveCurve;
					AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
					AdditiveCurve.InitFrom(Output.Curve);
					float Weight = PoseAsset->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractContext);
					FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
				}
				else
				{
					FPoseContext CurrentPose(Output);
					FPoseContext SourcePose(Output);

					SourcePose = Output;

					if (PoseAsset->GetAnimationPose(CurrentPose.Pose, CurrentPose.Curve, ExtractContext))
					{
						TArray<float> BoneWeights;
						BoneWeights.AddZeroed(CurrentPose.Pose.GetNumBones());
						// once we get it, we have to blend by weight
						FAnimationRuntime::BlendTwoPosesTogetherPerBone(CurrentPose.Pose, SourcePose.Pose, CurrentPose.Curve, SourcePose.Curve, BoneWeights, Output.Pose, Output.Curve);
					}
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
#if WITH_EDITORONLY_DATA
		// even if you don't have any asset curve, we want to output this curve values
		PropagatePreviewCurve(Output);
#endif // WITH_EDITORONLY_DATA
	}

	return true;
}

#if WITH_EDITORONLY_DATA
void FAnimSingleNodeInstanceProxy::PropagatePreviewCurve(FPoseContext& Output) 
{
	USkeleton* MySkeleton = GetSkeleton();
	for (auto Iter = PreviewCurveOverride.CreateConstIterator(); Iter; ++Iter)
	{
		const FName& Name = Iter.Key();
		const float Value = Iter.Value();

		FSmartName PreviewCurveName;

		if (MySkeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, Name, PreviewCurveName))
		{
			Output.Curve.Set(PreviewCurveName.UID, Value, ACF_EditorPreviewCurves);

		}
	}
}
#endif // WITH_EDITORONLY_DATA

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
				UpdateSlotNodeWeight(CurrentSlotNodeName, WeightInfo.SlotNodeWeight, 1.f);
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
		else if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(CurrentAsset))
		{
			FAnimTickRecord& TickRecord = CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			MakePoseAssetTickRecord(TickRecord, PoseAsset, 1.f);
		}
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

void FAnimSingleNodeInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) 
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
#if WITH_EDITOR
	// @fixme only do this in pose asset
// 	// copy data to PreviewPoseOverride
// 	TMap<FName, float> PoseCurveList;
// 
// 	InAnimInstance->GetAnimationCurveList(ACF_DrivesPose, PoseCurveList);
// 
// 	if (PoseCurveList.Num() > 0)
// 	{
// 		PreviewPoseOverride.Append(PoseCurveList);
// 	}
#endif // WITH_EDITOR
}
void FAnimSingleNodeInstanceProxy::InitializeObjects(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::InitializeObjects(InAnimInstance);

	UAnimSingleNodeInstance* AnimSingleNodeInstance = CastChecked<UAnimSingleNodeInstance>(InAnimInstance);
	CurrentAsset = AnimSingleNodeInstance->CurrentAsset;
}

void FAnimSingleNodeInstanceProxy::ClearObjects()
{
	FAnimInstanceProxy::ClearObjects();

	CurrentAsset = nullptr;
}

void FAnimSingleNodeInstanceProxy::SetPreviewCurveOverride(const FName& PoseName, float Value, bool bRemoveIfZero)
{
	float *CurveValPtr = PreviewCurveOverride.Find(PoseName);
	bool bShouldAddToList = bRemoveIfZero == false || FPlatformMath::Abs(Value) > ZERO_ANIMWEIGHT_THRESH;
	if (bShouldAddToList)
	{
		if (CurveValPtr)
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr = Value;
		}
		else
		{
			PreviewCurveOverride.Add(PoseName, Value);
		}
	}
	// if less than ZERO_ANIMWEIGHT_THRESH
	// no reason to keep them on the list
	else 
	{
		// remove if found
		PreviewCurveOverride.Remove(PoseName);
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
	bLooping = bIsLooping;
	PlayRate = InPlayRate;
	CurrentTime = 0.f;
	BlendSpaceInput = FVector::ZeroVector;
	BlendSampleData.Reset();
	MarkerTickRecord.Reset();
	UpdateBlendspaceSamples(BlendSpaceInput);

#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
	PreviewCurveOverride.Reset();
#endif

	
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(NewAsset))
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

void FAnimSingleNodeInstanceProxy::SetBlendSpaceInput(const FVector& InBlendInput)
{
	BlendSpaceInput = InBlendInput;
}

