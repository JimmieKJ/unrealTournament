// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysXSupport.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"

UPhysicsConstraintTemplate::UPhysicsConstraintTemplate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LinearXMotion_DEPRECATED = LCM_Locked;
	LinearYMotion_DEPRECATED = LCM_Locked;
	LinearZMotion_DEPRECATED = LCM_Locked;

	Pos1_DEPRECATED = FVector(0.0f, 0.0f, 0.0f);
	PriAxis1_DEPRECATED = FVector(1.0f, 0.0f, 0.0f);
	SecAxis1_DEPRECATED = FVector(0.0f, 1.0f, 0.0f);

	Pos2_DEPRECATED = FVector(0.0f, 0.0f, 0.0f);
	PriAxis2_DEPRECATED = FVector(1.0f, 0.0f, 0.0f);
	SecAxis2_DEPRECATED = FVector(0.0f, 1.0f, 0.0f);

	LinearBreakThreshold_DEPRECATED = 300.0f;
	AngularBreakThreshold_DEPRECATED = 500.0f;

	ProjectionLinearTolerance_DEPRECATED = 0.5; // Linear projection when error > 5.0 unreal units
	ProjectionAngularTolerance_DEPRECATED = 10.f; // Angular projection when error > 10 degrees
}

void UPhysicsConstraintTemplate::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// If old content, copy properties out of setup into instance
	if(Ar.UE4Ver() < VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE)
	{
		CopySetupPropsToInstance(&DefaultInstance);
	}
}

void UPhysicsConstraintTemplate::CopySetupPropsToInstance(FConstraintInstance* Instance)
{
	Instance->JointName			= JointName_DEPRECATED;
	Instance->ConstraintBone1	= ConstraintBone1_DEPRECATED;
	Instance->ConstraintBone2	= ConstraintBone2_DEPRECATED;

	Instance->Pos1				= Pos1_DEPRECATED;
	Instance->PriAxis1			= PriAxis1_DEPRECATED;
	Instance->SecAxis1			= SecAxis1_DEPRECATED;
	Instance->Pos2				= Pos2_DEPRECATED;
	Instance->PriAxis2			= PriAxis2_DEPRECATED;
	Instance->SecAxis2			= SecAxis2_DEPRECATED;

	Instance->bEnableProjection				= bEnableProjection_DEPRECATED;
	Instance->ProjectionLinearTolerance		= ProjectionLinearTolerance_DEPRECATED;
	Instance->ProjectionAngularTolerance	= ProjectionAngularTolerance_DEPRECATED;
	Instance->LinearXMotion					= LinearXMotion_DEPRECATED;
	Instance->LinearYMotion					= LinearYMotion_DEPRECATED;
	Instance->LinearZMotion					= LinearZMotion_DEPRECATED;
	Instance->LinearLimitSize				= LinearLimitSize_DEPRECATED;
	Instance->bLinearLimitSoft				= bLinearLimitSoft_DEPRECATED;
	Instance->LinearLimitStiffness			= LinearLimitStiffness_DEPRECATED;
	Instance->LinearLimitDamping			= LinearLimitDamping_DEPRECATED;
	Instance->bLinearBreakable				= bLinearBreakable_DEPRECATED;
	Instance->LinearBreakThreshold			= LinearBreakThreshold_DEPRECATED;
	Instance->AngularSwing1Motion			= AngularSwing1Motion_DEPRECATED;
	Instance->AngularSwing2Motion			= AngularSwing2Motion_DEPRECATED;
	Instance->AngularTwistMotion			= AngularTwistMotion_DEPRECATED;
	Instance->bSwingLimitSoft				= bSwingLimitSoft_DEPRECATED;
	Instance->bTwistLimitSoft				= bTwistLimitSoft_DEPRECATED;
	Instance->Swing1LimitAngle				= Swing1LimitAngle_DEPRECATED;
	Instance->Swing2LimitAngle				= Swing2LimitAngle_DEPRECATED;
	Instance->TwistLimitAngle				= TwistLimitAngle_DEPRECATED;
	Instance->SwingLimitStiffness			= SwingLimitStiffness_DEPRECATED;
	Instance->SwingLimitDamping				= SwingLimitDamping_DEPRECATED;
	Instance->TwistLimitStiffness			= TwistLimitStiffness_DEPRECATED;
	Instance->TwistLimitDamping				= TwistLimitDamping_DEPRECATED;
	Instance->bAngularBreakable				= bAngularBreakable_DEPRECATED;
	Instance->AngularBreakThreshold			= AngularBreakThreshold_DEPRECATED;
}