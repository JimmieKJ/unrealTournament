// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_ObserveBone.generated.h"

/**
 *	Debugging node that displays the current value of a bone in a specific space.
 */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_ObserveBone : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to observe. */
	UPROPERTY(EditAnywhere, Category=SkeletalControl)
	FBoneReference BoneToObserve;

	/** Reference frame to display the bone transform in. */
	UPROPERTY(EditAnywhere, Category=SkeletalControl)
	TEnumAsByte<EBoneControlSpace> DisplaySpace;

	/** Show the difference from the reference pose? */
	UPROPERTY(EditAnywhere, Category=SkeletalControl)
	bool bRelativeToRefPose;

	/** Translation of the bone being observed. */
	UPROPERTY()
	FVector Translation;

	/** Rotation of the bone being observed. */
	UPROPERTY()
	FRotator Rotation;

	/** Scale of the bone being observed. */
	UPROPERTY()
	FVector Scale;

public:
	FAnimNode_ObserveBone();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface
};
