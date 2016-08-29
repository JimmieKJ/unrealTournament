// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_Slot.generated.h"

// An animation slot node normally acts as a passthru, but a montage or PlaySlotAnimation call from
// game code can cause an animation to blend in and be played on the slot temporarily, overriding the
// Source input.
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_Slot : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// The source input, passed thru to the output unless a montage or slot animation is currently playing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FPoseLink Source;

	// The name of this slot, exposed to gameplay code, etc...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(CustomizeProperty))
	FName SlotName;

protected:
	FSlotNodeWeightInfo WeightData;
	FGraphTraversalCounter SlotNodeInitializationCounter;

public:	
	FAnimNode_Slot();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
};
