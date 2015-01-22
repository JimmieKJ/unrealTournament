// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

/**
 *	Simple controller that make a bone to look at the point or another bone
 */
USTRUCT()
struct ENGINE_API FAnimNode_LookAt : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl) 
	FBoneReference BoneToModify;

	/** Target Bone to look at - you can't use LookAtLocation as alternative as you'll get a delay on bone location if you query directly **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl)
	FBoneReference LookAtBone;

	/** Target Location in world space if LookAtBone is empty */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl, meta=(PinHiddenByDefault))
	FVector LookAtLocation;

	/** Look at axis, which axis to align to look at point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SkeletalControl) 
	TEnumAsByte<EAxisOption::Type>	LookAtAxis;

	/** Debug transient data */
	FVector CurrentLookAtLocation;

	// in the future, it would be nice to have more options, -i.e. lag, interpolation speed
	FAnimNode_LookAt();

	// FAnimNode_Base interface
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

	FVector GetAlignVector(FTransform& Transform, EAxisOption::Type AxisOption);
};
