// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_PoseDriver.generated.h"

class UPoseAsset;

/** One named parameter being driven by the orientation of a bone. */
struct FPoseDriverParam
{
	/** Information about this curve (name etc) */
	FAnimCurveParam ParamInfo;

	/** Value of this parameter */
	float ParamValue;
};

/** One target pose for the bone, with parameters to drive bone approaches this orientation. */
struct FPoseDriverPoseInfo
{
	/** Name of pose, from PoseAsset */
	FName PoseName;

	/** Rotation of source bone in this pose, from PoseAsset */
	FQuat PoseQuat;

	/** Distance to nearest pose */
	float NearestPoseDist;

	/** Last weight calculated for this pose. */
	float PoseWeight;

	/** Last distance calculated from this pose */
	float PoseDistance;

	FPoseDriverPoseInfo()
	: PoseQuat(FQuat::Identity)
	{}
};

/** Set of parameters being driven */
struct FPoseDriverParamSet
{
	/** Array of parameters that makes up a set */
	TArray<FPoseDriverParam> Params;

	/** Increase value of a named parameter in a set. */
	void AddParam(const FPoseDriverParam& InParam, float InScale);

	/** Add a set of params to another set */
	void AddParams(const TArray<FPoseDriverParam>& InParams, float InScale);

	/** Scale all params in a set */
	void ScaleAllParams(float InScale);

	/** Clear all params */
	void ClearParams();
};

/** Orientation aspect used to drive interpolation */
UENUM()
enum class EPoseDriverType : uint8
{
	/** Consider full rotation for interpolation */
	SwingAndTwist,

	/** Consider only swing for interpolation */
	SwingOnly,
};

/** RBF based orientation driver */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_PoseDriver : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** Bone to use for driving parameters based on its orientation */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FBoneReference SourceBone;

	/** Scaling of radial basis, applied to max distance between poses */
	UPROPERTY(EditAnywhere, Category = PoseDriver, meta = (ClampMin = "0.01"))
	float RadialScaling;

	/** PoseAsset used as source of  */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	UPoseAsset* PoseSource;

	/** Should we consider the mesh ref pose of SourceBone as a 'neutral' pose (zero curves)   */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	bool bIncludeRefPoseAsNeutralPose;

	/** Type of orientation for driving parameter  */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	EPoseDriverType Type;

	/** Axis to use when Type is SwingOnly */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	TEnumAsByte<EBoneAxis> TwistAxis;

	/** Previous set of interpolated params, kept for debugging */
	FPoseDriverParamSet ResultParamSet;

	/** Cached set of info for each pose */
	TArray<FPoseDriverPoseInfo> PoseInfos;

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	FAnimNode_PoseDriver();

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual void EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context);
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	/** Find max distance between any of the poses */
	void UpdateCachedPoseInfo(const FQuat& RefQuat);

	/** Util to return unit vector for current twist axis */
	FVector GetTwistAxisVector();
	/** Util to find distance between 2 quats, using Type and TwistAxis settings */
	float FindDistanceBetweenPoses(const FQuat& A, const FQuat& B);


protected:

	/** Is cached info up to date */
	bool bCachedPoseInfoUpToDate;

	// FAnimNode_SkeletalControlBase protected interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase protected interface
};