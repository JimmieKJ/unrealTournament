// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "AnimNode_SubInstance.generated.h"

USTRUCT()
struct ENGINE_API FAnimNode_SubInstance : public FAnimNode_Base
{
	GENERATED_BODY()

public:

	FAnimNode_SubInstance();

	/** 
	 *  Input pose for the node, intentionally not accessible because if there's no input
	 *  Node in the target class we don't want to show this as a pin
	 */
	UPROPERTY()
	FPoseLink InPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TSubclassOf<UAnimInstance> InstanceClass;

	/** This is the actual instance allocated at runtime that will run */
	UPROPERTY(transient)
	UAnimInstance* InstanceToRun;

	/** List of properties on the calling instance to push from */
	UPROPERTY(transient)
	TArray<UProperty*> InstanceProperties;

	/** List of properties on the sub instance to push to, built from name list when initialised */
	UPROPERTY(transient)
	TArray<UProperty*> SubInstanceProperties;

	/** List of source properties to use, 1-1 with Dest names below, built by the compiler */
	UPROPERTY()
	TArray<FName> SourcePropertyNames;

	/** List of destination properties to use, 1-1 with Source names above, built by the compiler */
	UPROPERTY()
	TArray<FName> DestPropertyNames;

	// Temporary storage for the output of the subinstance, will be copied into output pose.
	// Declared at member level to avoid reallocating the objects
	TArray<FTransform> BoneTransforms;
	FBlendedHeapCurve BlendedCurve;

	// Validate the instance and create if invalid
	void ValidateInstance();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override;
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

protected:
	virtual void RootInitialize(const FAnimInstanceProxy* InProxy) override;
	// End of FAnimNode_Base interface

	// Shutdown the currently running instance
	void TeardownInstance();
};