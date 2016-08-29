// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_Fabrik.generated.h"

/**
*	Controller which implements the FABRIK IK approximation algorithm -  see http://andreasaristidou.com/publications/FABRIK.pdf for details
*/

/** Transient structure for FABRIK node evaluation */
struct FABRIKChainLink
{
public:
	/** Position of bone in component space. */
	FVector Position;

	/** Distance to its parent link. */
	float Length;

	/** Bone Index in SkeletalMesh */
	FCompactPoseBoneIndex BoneIndex;

	/** Transform Index that this control will output */
	int32 TransformIndex;

	/** Child bones which are overlapping this bone. 
	 * They have a zero length distance, so they will inherit this bone's transformation. */
	TArray<int32> ChildZeroLengthTransformIndices;

	FABRIKChainLink()
		: Position(FVector::ZeroVector)
		, Length(0.f)
		, BoneIndex(INDEX_NONE)
		, TransformIndex(INDEX_NONE)
	{
	}

	FABRIKChainLink(const FVector& InPosition, const float& InLength, const FCompactPoseBoneIndex& InBoneIndex, const int32& InTransformIndex)
		: Position(InPosition)
		, Length(InLength)
		, BoneIndex(InBoneIndex)
		, TransformIndex(InTransformIndex)
	{
	}
};

USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_Fabrik : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Coordinates for target location of tip bone - if EffectorLocationSpace is bone, this is the offset from Target Bone to use as target location*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EndEffector, meta = (PinShownByDefault))
	FTransform EffectorTransform;

	/** Reference frame of Effector Transform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EndEffector)
	TEnumAsByte<enum EBoneControlSpace> EffectorTransformSpace;

	/** If EffectorTransformSpace is a bone, this is the bone to use. **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EndEffector)
	FBoneReference EffectorTransformBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EndEffector)
	TEnumAsByte<enum EBoneRotationSource> EffectorRotationSource;

	/** Name of tip bone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	FBoneReference TipBone;

	/** Name of the root bone*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	FBoneReference RootBone;

	/** Tolerance for final tip location delta from EffectorLocation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	float Precision;

	/** Maximum number of iterations allowed, to control performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	int32 MaxIterations;

	/** Toggle drawing of axes to debug joint rotation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Solver)
	bool bEnableDebugDraw;

public:
	FAnimNode_Fabrik();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	virtual void ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const;

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	// Convenience function to get current (pre-translation iteration) component space location of bone by bone index
	FVector GetCurrentLocation(FCSPose<FCompactPose>& MeshBases, const FCompactPoseBoneIndex& BoneIndex);

#if WITH_EDITOR
	// Cached CS location when in editor for debug drawing
	FTransform CachedEffectorCSTransform;
#endif
};