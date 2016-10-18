// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_LookAt.generated.h"

UENUM()
namespace EAxisOption
{
	enum Type
	{
		X,
		Y,
		Z, 
		X_Neg, 
		Y_Neg, 
		Z_Neg
	};
}


UENUM()
/** Various ways to interpolate TAlphaBlend. */
namespace EInterpolationBlend
{
	enum Type
	{
		Linear,
		Cubic,
		Sinusoidal,
		EaseInOutExponent2,
		EaseInOutExponent3,
		EaseInOutExponent4,
		EaseInOutExponent5,
		MAX
	};
}

/**
 *	Simple controller that make a bone to look at the point or another bone
 */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_LookAt : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl) 
	FBoneReference BoneToModify;

	/** Target Bone to look at - you can't use LookAtLocation as alternative as you'll get a delay on bone location if you query directly **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Target)
	FBoneReference LookAtBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Target)
	FName LookAtSocket;

	/** Target Location in world space if LookAtBone is empty */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Target, meta = (PinHiddenByDefault))
	FVector LookAtLocation;

	/** Look at axis, which axis to align to look at point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl) 
	TEnumAsByte<EAxisOption::Type>	LookAtAxis;

	/** Whether or not to use Look up axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl)
	bool bUseLookUpAxis;

	/** Look up axis in local space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl)
	TEnumAsByte<EAxisOption::Type>	LookUpAxis;

	/** Look at Clamp value in degree - if you're look at axis is Z, only X, Y degree of clamp will be used*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	float LookAtClamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	TEnumAsByte<EInterpolationBlend::Type>	InterpolationType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	float	InterpolationTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	float	InterpolationTriggerThreashold;

	UPROPERTY(EditAnywhere, Category=SkeletalControl)
	bool	bEnableDebug;

	// in the future, it would be nice to have more options, -i.e. lag, interpolation speed
	FAnimNode_LookAt();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp);

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	FVector GetAlignVector(const FTransform& Transform, EAxisOption::Type AxisOption);

	EAlphaBlendType GetInterpolationType()
	{
		switch (InterpolationType)
		{
		case EInterpolationBlend::Cubic:
			return ABT_Cubic;
		case EInterpolationBlend::Sinusoidal:
			return ABT_Cubic;
		case EInterpolationBlend::EaseInOutExponent2:
			return ABT_EaseInOutExponent2;
		case EInterpolationBlend::EaseInOutExponent3:
			return ABT_EaseInOutExponent3;
		case EInterpolationBlend::EaseInOutExponent4:
			return ABT_EaseInOutExponent4;
		case EInterpolationBlend::EaseInOutExponent5:
			return ABT_EaseInOutExponent5;
		}

		return ABT_Linear;
	}

	/** Debug transient data */
	FVector CurrentLookAtLocation;

	/** Current Target Location */
	FVector CurrentTargetLocation;
	FVector PreviousTargetLocation;

	/** Current Alpha */
	float AccumulatedInterpoolationTime;

	/** Look at socket bone cache data */
	int32 CachedLookAtSocketMeshBoneIndex;
	FCompactPoseBoneIndex CachedLookAtSocketBoneIndex;
	FTransform CachedSocketLocalTransform;

	/** Debug draw cached data */
	FVector CachedComponentBoneLocation;
	FVector CachedPreviousTargetLocation;
	FVector CachedCurrentTargetLocation;
	FVector CachedCurrentLookAtLocation;
	
};
