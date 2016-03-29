// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_WheelHandler.generated.h"

struct FWheelSimulator
{
	int32					WheelIndex;
	FBoneReference			BoneReference;
	FRotator				RotOffset;
	FVector					LocOffset;
};

/**
 *	Simple controller that replaces or adds to the translation/rotation of a single bone.
 */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_WheelHandler : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	/** Current Asset being played **/
	UPROPERTY(transient)
	class UWheeledVehicleMovementComponent* VehicleSimComponent;

	TArray<FWheelSimulator>			WheelSimulators;

	FAnimNode_WheelHandler();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual bool CanUpdateInWorkerThread() const override { return false; }
	// End of FAnimNode_SkeletalControlBase interface

private:
	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface
};
