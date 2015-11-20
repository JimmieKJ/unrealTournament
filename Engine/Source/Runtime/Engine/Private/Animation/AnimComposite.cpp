// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimComposite.cpp: Composite classes that contains sequence for each section
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/AnimComposite.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"

UAnimComposite::UAnimComposite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
bool UAnimComposite::GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences) 
{
	return AnimationTrack.GetAllAnimationSequencesReferred(AnimationSequences);
}

void UAnimComposite::ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap)
{
	AnimationTrack.ReplaceReferredAnimations(ReplacementMap);
}
#endif

void UAnimComposite::OnAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, class UAnimInstance* InstanceOwner) const
{
	Super::OnAssetPlayerTickedInternal(Context, PreviousTime, MoveDelta, Instance, InstanceOwner);

	ExtractRootMotionFromTrack(AnimationTrack, PreviousTime, PreviousTime + MoveDelta, Context.RootMotionMovementParams);
}

void UAnimComposite::GetAnimationPose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	AnimationTrack.GetAnimationPose(OutPose, OutCurve, ExtractionContext);

	FBlendedCurve CompositeCurve;
	CompositeCurve.InitFrom(OutCurve);
	EvaluateCurveData(CompositeCurve, ExtractionContext.CurrentTime);

	// combine both curve
	OutCurve.Combine(CompositeCurve);
}