// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_Trail.generated.h"

/**
 * Trail Controller
 */

USTRUCT()
struct ENGINE_API FAnimNode_Trail : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Reference to the active bone in the hierarchy to modify. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	FBoneReference TrailBone;

	/** Number of bones above the active one in the hierarchy to modify. ChainLength should be at least 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Trail, meta = (ClampMin = "2", UIMin = "2"))
	int32	ChainLength;

	/** Axis of the bones to point along trail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	TEnumAsByte<EAxis::Type>	ChainBoneAxis;

	/** Invert the direction specified in ChainBoneAxis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	bool	bInvertChainBoneAxis;

	/** Limit the amount that a bone can stretch from its ref-pose length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	bool	bLimitStretch;

	/** How quickly we 'relax' the bones to their animated positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	float	TrailRelaxation;

	/** If bLimitStretch is true, this indicates how long a bone can stretch beyond its length in the ref-pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	float	StretchLimit;

	/** 'Fake' velocity applied to bones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	FVector	FakeVelocity;

	/** Whether 'fake' velocity should be applied in actor or world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Trail)
	bool	bActorSpaceFakeVel;

	/** Internal use - we need the timestep to do the relaxation in CalculateNewBoneTransforms. */
	float	ThisTimstep;

	/** Did we have a non-zero ControlStrength last frame. */
	bool	bHadValidStrength;

	/** Component-space locations of the bones from last frame. Each frame these are moved towards their 'animated' locations. */
	TArray<FVector>	TrailBoneLocations;

	/** LocalToWorld used last frame, used for building transform between frames. */
	FMatrix		OldLocalToWorld;


	FAnimNode_Trail();

	// FAnimNode_Base interface
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer& RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	FVector GetAlignVector(EAxis::Type AxisOption, bool bInvert);
};