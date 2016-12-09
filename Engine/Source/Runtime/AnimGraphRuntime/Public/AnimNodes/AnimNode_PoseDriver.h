// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_PoseHandler.h"
#include "AnimNode_PoseDriver.generated.h"

/** One target pose for the bone, with parameters to drive bone approaches this orientation. */
struct FPoseDriverPoseInfo
{
	/** Name of pose, from PoseAsset */
	FName PoseName;

	/** Transform of source bone in this pose, from PoseAsset */
	FTransform PoseTM;

	/** Distance to nearest pose */
	float NearestPoseDist;

	/** Last weight calculated for this pose. */
	float PoseWeight;

	/** Last distance calculated from this pose */
	float PoseDistance;

	FPoseDriverPoseInfo()
	: PoseTM(FTransform::Identity)
	{}
};

/** Orientation aspect used to drive interpolation */
UENUM()
enum class EPoseDriverType : uint8
{
	/** Consider full rotation for interpolation */
	SwingAndTwist,

	/** Consider only swing for interpolation */
	SwingOnly,

	/** Consider translation relative to parent */
	Translation
};

/** RBF based orientation driver */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_PoseDriver : public FAnimNode_PoseHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = Links)
	FPoseLink SourcePose;

	/** Bone to use for driving parameters based on its orientation */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	FBoneReference SourceBone;

	/** Scaling of radial basis, applied to max distance between poses */
	UPROPERTY(EditAnywhere, Category = PoseDriver, meta = (ClampMin = "0.01"))
	float RadialScaling;

	/** Should we consider the mesh ref pose of SourceBone as a 'neutral' pose (zero curves)   */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	bool bIncludeRefPoseAsNeutralPose;

	/** Type of orientation for driving parameter  */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	EPoseDriverType Type;

	/** Axis to use when Type is SwingOnly */
	UPROPERTY(EditAnywhere, Category = PoseDriver)
	TEnumAsByte<EBoneAxis> TwistAxis;

	/** Input source bone TM, used for debug drawing */
	FTransform SourceBoneTM;

	/** Cached set of info for each pose */
	TArray<FPoseDriverPoseInfo> PoseInfos;

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_PoseHandler interface
	virtual void OnPoseAssetChange() override;
	// End FAnimNode_PoseHandler interface


	FAnimNode_PoseDriver();

	/** Find max distance between any of the poses */
	void UpdateCachedPoseInfo(const FTransform& RefTM);

	/** Util to return unit vector for current twist axis */
	FVector GetTwistAxisVector();
	/** Util to find 'distance' between 2 transforms, using Type and TwistAxis settings */
	float FindDistanceBetweenPoses(const FTransform& A, const FTransform& B);


protected:
	/** Is cached info up to date */
	bool bCachedPoseInfoUpToDate;
};
