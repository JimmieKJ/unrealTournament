// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// Complete constraint definition used by rigid body physics.
// 
// Defaults here will give you a ball and socket joint.
// Positions are in Physics scale.
// When adding stuff here, make sure to update URB_ConstraintSetup::CopyConstraintParamsFrom
//=============================================================================

#pragma once 

#include "ConstraintInstance.h"
#include "PhysicsConstraintTemplate.generated.h"


UCLASS(hidecategories=Object, MinimalAPI)
class UPhysicsConstraintTemplate : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FName JointName_DEPRECATED;
	UPROPERTY()
	FName ConstraintBone1_DEPRECATED;
	UPROPERTY()
	FName ConstraintBone2_DEPRECATED;
	UPROPERTY()
	FVector Pos1_DEPRECATED;
	UPROPERTY()
	FVector PriAxis1_DEPRECATED;
	UPROPERTY()
	FVector SecAxis1_DEPRECATED;
	UPROPERTY()
	FVector Pos2_DEPRECATED;
	UPROPERTY()
	FVector PriAxis2_DEPRECATED;
	UPROPERTY()
	FVector SecAxis2_DEPRECATED;
	UPROPERTY()
	uint32 bEnableProjection_DEPRECATED:1;
	UPROPERTY()
	float ProjectionLinearTolerance_DEPRECATED;
	UPROPERTY()
	float ProjectionAngularTolerance_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum ELinearConstraintMotion> LinearXMotion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum ELinearConstraintMotion> LinearYMotion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum ELinearConstraintMotion> LinearZMotion_DEPRECATED;
	UPROPERTY()
	float LinearLimitSize_DEPRECATED;
	UPROPERTY()
	uint32 bLinearLimitSoft_DEPRECATED:1;
	UPROPERTY()
	float LinearLimitStiffness_DEPRECATED;
	UPROPERTY()
	float LinearLimitDamping_DEPRECATED;
	UPROPERTY()
	uint32 bLinearBreakable_DEPRECATED:1;
	UPROPERTY()
	float LinearBreakThreshold_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum EAngularConstraintMotion> AngularSwing1Motion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum EAngularConstraintMotion> AngularSwing2Motion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum EAngularConstraintMotion> AngularTwistMotion_DEPRECATED;
	UPROPERTY()
	uint32 bSwingLimitSoft_DEPRECATED:1;
	UPROPERTY()
	uint32 bTwistLimitSoft_DEPRECATED:1;
	UPROPERTY()
	float Swing1LimitAngle_DEPRECATED;
	UPROPERTY()
	float Swing2LimitAngle_DEPRECATED;
	UPROPERTY()
	float TwistLimitAngle_DEPRECATED;
	UPROPERTY()
	float SwingLimitStiffness_DEPRECATED;
	UPROPERTY()
	float SwingLimitDamping_DEPRECATED;
	UPROPERTY()
	float TwistLimitStiffness_DEPRECATED;
	UPROPERTY()
	float TwistLimitDamping_DEPRECATED;
	UPROPERTY()
	uint32 bAngularBreakable_DEPRECATED:1;
	UPROPERTY()
	float AngularBreakThreshold_DEPRECATED;

	UPROPERTY(EditAnywhere, Category=Joint, meta=(ShowOnlyInnerProperties))
	FConstraintInstance DefaultInstance;


	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	// End UObject interface.

private:
	// Only needed for old content! Pre VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE
	void CopySetupPropsToInstance(FConstraintInstance* Instance);
};



