// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "PhysXSupport.h"
#include "PhysicsEngine/ConstraintInstance.h"

extern TAutoConsoleVariable<float> CVarConstraintDampingScale;
extern TAutoConsoleVariable<float> CVarConstraintStiffnessScale;

#if WITH_PHYSX

/** Util for converting from UE motion enum to physx motion enum */
PxD6Motion::Enum U2PAngularMotion(EAngularConstraintMotion InMotion)
{
	switch (InMotion)
	{
	case EAngularConstraintMotion::ACM_Free: return PxD6Motion::eFREE;
	case EAngularConstraintMotion::ACM_Limited: return PxD6Motion::eLIMITED;
	case EAngularConstraintMotion::ACM_Locked: return PxD6Motion::eLOCKED;
	default: check(0);	//unsupported motion type
	}

	return PxD6Motion::eFREE;
}

/** Util for converting from UE motion enum to physx motion enum */
PxD6Motion::Enum U2PLinearMotion(ELinearConstraintMotion InMotion)
{
	switch (InMotion)
	{
	case ELinearConstraintMotion::LCM_Free: return PxD6Motion::eFREE;
	case ELinearConstraintMotion::LCM_Limited: return PxD6Motion::eLIMITED;
	case ELinearConstraintMotion::LCM_Locked: return PxD6Motion::eLOCKED;
	default: check(0);	//unsupported motion type
	}

	return PxD6Motion::eFREE;
}


/** Util for setting soft limit params */
void SetSoftLimitParams_AssumesLocked(PxJointLimitParameters* PLimit, bool bSoft, float Spring, float Damping)
{
	if(bSoft)
	{
		PLimit->stiffness = Spring * CVarConstraintStiffnessScale.GetValueOnGameThread();
		PLimit->damping = Damping * CVarConstraintDampingScale.GetValueOnGameThread();
	}
}

/** Util for setting linear movement for an axis */
template <PxD6Axis::Enum PAxis>
void SetLinearMovement_AssumesLocked(PxD6Joint* PD6Joint, ELinearConstraintMotion Motion, bool bLockLimitSize)
{
	if(Motion == LCM_Locked || (Motion == LCM_Limited && bLockLimitSize))
	{
		PD6Joint->setMotion(PAxis, PxD6Motion::eLOCKED);
	}else
	{
		PD6Joint->setMotion(PAxis, U2PLinearMotion(Motion));
	}
}

#endif //WITH_PHYSX

FConstraintBaseParams::FConstraintBaseParams()
	: Stiffness(50.f)
	, Damping(5.f)
	, Restitution(0.f)
	, ContactDistance(1.f)
	, bSoftConstraint(false)
{

}

FLinearConstraint::FLinearConstraint()
	: Limit(0.f)
	, XMotion(LCM_Locked)
	, YMotion(LCM_Locked)
	, ZMotion(LCM_Locked)
{
	ContactDistance = 5.f;
	Stiffness = 0.f;
	Damping = 0.f;
}

FConeConstraint::FConeConstraint()
	: Swing1LimitDegrees(45.f)
	, Swing2LimitDegrees(45.f)
	, Swing1Motion(ACM_Free)
	, Swing2Motion(ACM_Free)
{
	bSoftConstraint = true;
	ContactDistance = 1.f;
}

FTwistConstraint::FTwistConstraint()
	: TwistLimitDegrees(45)
	, TwistMotion(ACM_Free)
{
	bSoftConstraint = true;
	ContactDistance = 1.f;
}

#if WITH_PHYSX
void FLinearConstraint::UpdatePhysXLinearLimit_AssumesLocked(PxD6Joint* Joint, float AverageMass, float Scale) const
{
	const float UseLimit = Limit * Scale;
	const bool bLockLimitSize = (UseLimit < RB_MinSizeToLockDOF);
		
	SetLinearMovement_AssumesLocked<PxD6Axis::eX>(Joint, XMotion, bLockLimitSize);
	SetLinearMovement_AssumesLocked<PxD6Axis::eY>(Joint, YMotion, bLockLimitSize);
	SetLinearMovement_AssumesLocked<PxD6Axis::eZ>(Joint, ZMotion, bLockLimitSize);

	// If any DOF is locked/limited, set up the joint limit
	if (XMotion != LCM_Free || YMotion != LCM_Free || ZMotion != LCM_Free)
	{
		PxJointLinearLimit PLinearLimit(GPhysXSDK->getTolerancesScale(), UseLimit, FMath::Clamp(ContactDistance, 5.f, UseLimit * 0.49f));
		PLinearLimit.restitution = Restitution;
		SetSoftLimitParams_AssumesLocked(&PLinearLimit, bSoftConstraint, Stiffness*AverageMass, Damping*AverageMass);
		Joint->setLinearLimit(PLinearLimit);
	}
}
#endif

#if WITH_PHYSX
void FConeConstraint::UpdatePhysXConeLimit_AssumesLocked(PxD6Joint* Joint, float AverageMass) const
{
	if (Swing1Motion == ACM_Limited || Swing2Motion == ACM_Limited)
	{
		//Clamp the limit value to valid range which PhysX won't ignore, both value have to be clamped even if there is only one degree limit in constraint
		const float Limit1Rad = FMath::DegreesToRadians(FMath::ClampAngle(Swing1LimitDegrees, KINDA_SMALL_NUMBER, 179.9999f));
		const float Limit2Rad = FMath::DegreesToRadians(FMath::ClampAngle(Swing2LimitDegrees, KINDA_SMALL_NUMBER, 179.9999f));

		//Clamp the contact distance so that it's not too small (jittery joint) or too big (always active joint)
		const float ContactRad = FMath::DegreesToRadians(FMath::Clamp(ContactDistance, 1.f, FMath::Min(Swing1LimitDegrees, Swing2LimitDegrees) * 0.49f));

		PxJointLimitCone PSwingLimitCone(Limit2Rad, Limit1Rad, ContactRad);
		PSwingLimitCone.restitution = Restitution;
		SetSoftLimitParams_AssumesLocked(&PSwingLimitCone, bSoftConstraint, Stiffness * AverageMass, Damping * AverageMass);
		Joint->setSwingLimit(PSwingLimitCone);
	}

	Joint->setMotion(PxD6Axis::eSWING2, U2PAngularMotion(Swing1Motion));
	Joint->setMotion(PxD6Axis::eSWING1, U2PAngularMotion(Swing2Motion));
}
#endif

#if WITH_PHYSX
void FTwistConstraint::UpdatePhysXTwistLimit_AssumesLocked(PxD6Joint* Joint, float AverageMass) const
{
	if (TwistMotion == ACM_Limited)
	{
		const float TwistLimitRad = FMath::DegreesToRadians(TwistLimitDegrees);
		
		//Clamp the contact distance so that it's not too small (jittery joint) or too big (always active joint)
		const float ContactRad = FMath::DegreesToRadians(FMath::Clamp(ContactDistance, 1.f, TwistLimitDegrees * .95f));

		PxJointAngularLimitPair PTwistLimitPair(-TwistLimitRad, TwistLimitRad, ContactRad);
		PTwistLimitPair.restitution = Restitution;
		SetSoftLimitParams_AssumesLocked(&PTwistLimitPair, bSoftConstraint, Stiffness * AverageMass, Damping * AverageMass);
		Joint->setTwistLimit(PTwistLimitPair);
	}

	Joint->setMotion(PxD6Axis::eTWIST, U2PAngularMotion(TwistMotion));
}
#endif