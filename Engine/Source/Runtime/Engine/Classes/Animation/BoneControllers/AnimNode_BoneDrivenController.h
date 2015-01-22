// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_BoneDrivenController.generated.h"

// Evaluation of the bone transforms relies on the size and ordering of this
// enum, if this needs to change make sure EvaluateBoneTransforms is updated.
UENUM()
namespace EComponentType
{
	enum Type
	{
		None = 0,
		TranslationX,
		TranslationY,
		TranslationZ,
		RotationX,
		RotationY,
		RotationZ,
		Scale
	};
}

USTRUCT()
struct ENGINE_API FAnimNode_BoneDrivenController : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	// Bone to use as controller input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bones)
	FBoneReference SourceBone;

	// Bone to drive using controller input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bones)
	FBoneReference TargetBone;

	// Transform component to use as input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Controller)
	TEnumAsByte<EComponentType::Type> SourceComponent;

	// Transform component to write to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Controller)
	TEnumAsByte<EComponentType::Type> TargetComponent;

	// Multiplier to apply to the input value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Controller)
	float Multiplier;

	// Whether or not to use the range limits
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	bool bUseRange;

	// Min limit of the output value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	float RangeMin;

	// Max limit of the output value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	float RangeMax;

	FAnimNode_BoneDrivenController();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	//////////////////////////////////////////////////////////////////////////

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer& RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	//////////////////////////////////////////////////////////////////////////

protected:
	
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	//////////////////////////////////////////////////////////////////////////
};