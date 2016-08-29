// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_TransitionResult.generated.h"

// Root node of a state machine transition graph
USTRUCT()
struct ENGINE_API FAnimNode_TransitionResult : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Result, meta=(AlwaysAsPin))
	bool bCanEnterTransition;

	/** Native delegate to use when checking transition */
	FCanTakeTransition NativeTransitionDelegate;

public:	
	FAnimNode_TransitionResult();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};
