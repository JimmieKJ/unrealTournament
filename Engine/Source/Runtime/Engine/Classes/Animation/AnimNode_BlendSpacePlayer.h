// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNodeBase.h"
#include "AnimNode_BlendSpacePlayer.generated.h"

//@TODO: Comment
USTRUCT()
struct ENGINE_API FAnimNode_BlendSpacePlayer : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	// The X coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Coordinates, meta=(PinShownByDefault))
	mutable float X;

	// The Y coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates, meta = (PinShownByDefault))
	mutable float Y;

	// The Z coordinate to sample in the blendspace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Coordinates, meta = (PinHiddenByDefault))
	mutable float Z;

	// The play rate multiplier. Can be negative, which will cause the animation to play in reverse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DefaultValue = "1.0", PinHiddenByDefault))
	mutable float PlayRate;

	// Should the animation continue looping when it reaches the end?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (DefaultValue = "true", PinHiddenByDefault))
	mutable bool bLoop;

	// The blendspace asset to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	UBlendSpaceBase* BlendSpace;

	// The group index, assigned at compile time based on the editoronly GroupName (or INDEX_NONE if it is not part of any group)
	UPROPERTY()
	int32 GroupIndex;

	// The role this player can assume within the group (ignored if GroupIndex is INDEX_NONE)
	UPROPERTY()
	TEnumAsByte<EAnimGroupRole::Type> GroupRole;
protected:
	UPROPERTY(BlueprintReadWrite, Transient, Category=DoNotEdit)
	float InternalTimeAccumulator;

	UPROPERTY()
	FBlendFilter BlendFilter;

	UPROPERTY()
	TArray<FBlendSampleData> BlendSampleDataCache;

public:	
	FAnimNode_BlendSpacePlayer();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void OverrideAsset(UAnimationAsset* NewAsset) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

protected:
	void UpdateInternal(const FAnimationUpdateContext& Context);
};
