// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "AnimNode_SubInput.generated.h"

USTRUCT()
struct ENGINE_API FAnimNode_SubInput : public FAnimNode_Base
{
	GENERATED_BODY()

	// Begin poses for the sub instance, these will be populated by the calling subinstance node
	// before this graph is processed
	FCompactHeapPose InputPose;
	FBlendedHeapCurve InputCurve;

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

};