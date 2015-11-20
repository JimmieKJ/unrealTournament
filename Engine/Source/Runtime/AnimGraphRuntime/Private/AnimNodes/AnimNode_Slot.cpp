// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "Animation/AnimMontage.h"

/////////////////////////////////////////////////////
// FAnimNode_Slot

void FAnimNode_Slot::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	Source.Initialize(Context);
	WeightData.Reset();

	// If this node has not already been registered with the AnimInstance, do it.
	if (!SlotNodeInitializationCounter.IsSynchronizedWith(Context.AnimInstance->SlotNodeInitializationCounter))
	{
		SlotNodeInitializationCounter.SynchronizeWith(Context.AnimInstance->SlotNodeInitializationCounter);
		Context.AnimInstance->RegisterSlotNodeWithAnimInstance(SlotName);
	}
}

void FAnimNode_Slot::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	Source.CacheBones(Context);
}

void FAnimNode_Slot::Update(const FAnimationUpdateContext& Context)
{
	// Update weights.
	Context.AnimInstance->GetSlotWeight(SlotName, WeightData.SlotNodeWeight, WeightData.SourceWeight, WeightData.TotalNodeWeight);

	// Update cache in AnimInstance.
	Context.AnimInstance->UpdateSlotNodeWeight(SlotName, WeightData.SlotNodeWeight);
	Context.AnimInstance->UpdateSlotRootMotionWeight(SlotName, Context.GetFinalBlendWeight());

	if (WeightData.SourceWeight > ZERO_ANIMWEIGHT_THRESH)
	{
		Source.Update(Context.FractionalWeight(WeightData.SourceWeight));
	}
}

void FAnimNode_Slot::Evaluate(FPoseContext & Output)
{
	// If not playing a montage, just pass through
	if (WeightData.SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		Source.Evaluate(Output);
	}
	else
	{
		FPoseContext SourceContext(Output);
		if (WeightData.SourceWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			Source.Evaluate(SourceContext);
		}

		Output.AnimInstance->SlotEvaluatePose(SlotName, SourceContext.Pose, SourceContext.Curve, WeightData.SourceWeight, Output.Pose, Output.Curve, WeightData.SlotNodeWeight, WeightData.TotalNodeWeight);

		checkSlow(!Output.ContainsNaN());
		checkSlow(Output.IsNormalized());
	}
}

void FAnimNode_Slot::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Slot Name: '%s' Weight:%.1f%%)"), *SlotName.ToString(), WeightData.SlotNodeWeight*100.f);
	
	bool const bIsPoseSource = (WeightData.SourceWeight <= ZERO_ANIMWEIGHT_THRESH);
	DebugData.AddDebugItem(DebugLine, bIsPoseSource);
	Source.GatherDebugData(DebugData.BranchFlow(WeightData.SourceWeight));

	for (FAnimMontageInstance* MontageInstance : DebugData.AnimInstance->MontageInstances)
	{
		if (MontageInstance->IsValid() && MontageInstance->Montage->IsValidSlot(SlotName))
		{
			const FAnimTrack* Track = MontageInstance->Montage->GetAnimationData(SlotName);
			for (const FAnimSegment& Segment : Track->AnimSegments)
			{
				float Weight;
				float CurrentAnimPos;
				if (UAnimSequenceBase* Anim = Segment.GetAnimationData(MontageInstance->GetPosition(), CurrentAnimPos, Weight))
				{
					FString MontageLine = FString::Printf(TEXT("Montage: '%s' Anim: '%s' Play Time:%.2f W:%.2f%%"), *MontageInstance->Montage->GetName(), *Anim->GetName(), CurrentAnimPos, Weight*100.f);
					DebugData.BranchFlow(1.0f).AddDebugItem(MontageLine, true);
					break;
				}
			}
		}
	}
}

FAnimNode_Slot::FAnimNode_Slot()
	: SlotName(FAnimSlotGroup::DefaultSlotName)
{
}
