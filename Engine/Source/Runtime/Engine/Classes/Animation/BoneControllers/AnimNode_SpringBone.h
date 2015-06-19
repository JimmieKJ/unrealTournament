// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_SpringBone.generated.h"

/**
 *	Simple controller that replaces or adds to the translation/rotation of a single bone.
 */

USTRUCT()
struct ENGINE_API FAnimNode_SpringBone : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring) 
	FBoneReference SpringBone;

	/** Limit the amount that a bone can stretch from its ref-pose length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring)
	bool bLimitDisplacement;

	/** If bLimitDisplacement is true, this indicates how long a bone can stretch beyond its length in the ref-pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring, meta=(EditCondition="bLimitDisplacement"))
	float MaxDisplacement;

	/** Stiffness of spring */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring)
	float SpringStiffness;

	/** Damping of spring */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring)
	float SpringDamping;

	/** If spring stretches more than this, reset it. Useful for catching teleports etc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring)
	float ErrorResetThresh;

	/** If true, Z position is always correct, no spring applied */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Spring)
	bool bNoZSpring;

	/** Internal use - Amount of time we need to simulate. */
	float RemainingTime;

	/** Internal use - Current timestep */
	float FixedTimeStep;

	/** Did we have a non-zero ControlStrength last frame. */
	bool bHadValidStrength;

	/** World-space location of the bone. */
	FVector BoneLocation;

	/** World-space velocity of the bone. */
	FVector BoneVelocity;

public:
	FAnimNode_SpringBone();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
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
};
