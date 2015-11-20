// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_BoneDrivenController.generated.h"

// Evaluation of the bone transforms relies on the size and ordering of this
// enum, if this needs to change make sure EvaluateBoneTransforms is updated.

// The transform component (attribute) to read from
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
		Scale UMETA(DisplayName="Scale (largest component)"),
		ScaleX,
		ScaleY,
		ScaleZ
	};
}

// The type of modification to make to the destination component(s)
UENUM()
enum class EDrivenBoneModificationMode : uint8
{
	// Add the driven value to the input component value(s)
	AddToInput,

	// Replace the input component value(s) with the driven value
	ReplaceComponent,

	// Add the driven value to the reference pose value
	AddToRefPose
};

/**
 * This is the runtime version of a bone driven controller, which maps part of the state from one bone to another (e.g., 2 * source.x -> target.z)
 */
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_BoneDrivenController : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()

	// Bone to use as controller input
	UPROPERTY(EditAnywhere, Category="Source (driver)")
	FBoneReference SourceBone;

	// Transform component to use as input
	UPROPERTY(EditAnywhere, Category="Source (driver)")
	TEnumAsByte<EComponentType::Type> SourceComponent;

	/** Curve used to map from the source attribute to the driven attributes if present (otherwise the Multiplier will be used) */
	UPROPERTY(EditAnywhere, Category=Mapping)
	UCurveFloat* DrivingCurve;

	// Multiplier to apply to the input value (Note: Ignored when a curve is used)
	UPROPERTY(EditAnywhere, Category=Mapping)
	float Multiplier;

	// Whether or not to clamp the driver value and remap it before scaling it
	UPROPERTY(EditAnywhere, Category=Mapping, meta=(DisplayName="Remap Source"))
	bool bUseRange;

	// Minimum limit of the input value (mapped to RemappedMin, only used when limiting the source range)
	UPROPERTY(EditAnywhere, Category=Mapping, meta=(EditCondition=bUseRange, DisplayName="Source Range Min"))
	float RangeMin;

	// Maximum limit of the input value (mapped to RemappedMax, only used when limiting the source range)
	UPROPERTY(EditAnywhere, Category=Mapping, meta=(EditCondition=bUseRange, DisplayName="Source Range Max"))
	float RangeMax;

	// Minimum value to apply to the destination (remapped from the input range)
	UPROPERTY(EditAnywhere, Category=Mapping, meta=(EditCondition=bUseRange, DisplayName="Mapped Range Min"))
	float RemappedMin;

	// Maximum value to apply to the destination (remapped from the input range)
	UPROPERTY(EditAnywhere, Category = Mapping, meta = (EditCondition = bUseRange, DisplayName="Mapped Range Max"))
	float RemappedMax;

	// Bone to drive using controller input
	UPROPERTY(EditAnywhere, Category="Destination (driven)")
	FBoneReference TargetBone;

private:
	UPROPERTY()
	TEnumAsByte<EComponentType::Type> TargetComponent_DEPRECATED;

public:
	// Affect the X component of translation on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="X"))
	uint32 bAffectTargetTranslationX : 1;

	// Affect the Y component of translation on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="Y"))
	uint32 bAffectTargetTranslationY : 1;

	// Affect the Z component of translation on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="Z"))
	uint32 bAffectTargetTranslationZ : 1;

	// Affect the X component of rotation on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="X"))
	uint32 bAffectTargetRotationX : 1;

	// Affect the Y component of rotation on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="Y"))
	uint32 bAffectTargetRotationY : 1;

	// Affect the Z component of rotation on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="Z"))
	uint32 bAffectTargetRotationZ : 1;

	// Affect the X component of scale on the target bone
	UPROPERTY(EditAnywhere, Category = "Destination (driven)", meta=(DisplayName="X"))
	uint32 bAffectTargetScaleX : 1;

	// Affect the Y component of scale on the target bone
	UPROPERTY(EditAnywhere, Category = "Destination (driven)", meta=(DisplayName="Y"))
	uint32 bAffectTargetScaleY : 1;

	// Affect the Z component of scale on the target bone
	UPROPERTY(EditAnywhere, Category="Destination (driven)", meta=(DisplayName="Z"))
	uint32 bAffectTargetScaleZ : 1;

	// The type of modification to make to the destination component(s)
	UPROPERTY(EditAnywhere, Category="Destination (driven)")
	EDrivenBoneModificationMode ModificationMode;

public:
	FAnimNode_BoneDrivenController();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	// Upgrade a node from the output enum to the output bits (change made in FAnimationCustomVersion::BoneDrivenControllerMatchingMaya)
	void ConvertTargetComponentToBits();
protected:
	
	// FAnimNode_SkeletalControlBase protected interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase protected interface
};