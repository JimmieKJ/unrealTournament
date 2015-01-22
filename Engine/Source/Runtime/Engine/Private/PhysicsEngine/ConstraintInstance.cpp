// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "PhysXSupport.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "ConstraintInstance"

/** Handy macro for setting BIT of VAR based on the bool CONDITION */
#define SET_DRIVE_PARAM(VAR, CONDITION, BIT)   (VAR) = (CONDITION) ? ((VAR) | (BIT)) : ((VAR) & ~(BIT))

// Cvars
static TAutoConsoleVariable<float> CVarConstraintDampingScale(
	TEXT("p.ConstraintDampingScale"),
	100000.f,
	TEXT("The multiplier of constraint damping in simulation. Default: 100000"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarConstraintStiffnessScale(
	TEXT("p.ConstraintStiffnessScale"),
	100000.f,
	TEXT("The multiplier of constraint stiffness in simulation. Default: 100000"),
	ECVF_ReadOnly);


#if WITH_PHYSX
/** Util for setting soft limit params */
static void SetSoftLimitParams(PxJointLimitParameters* PLimit, bool bSoft, float Spring, float Damping)
{
	if(bSoft)
	{
		PLimit->stiffness = Spring;
		PLimit->damping = Damping;
	}
}

/** Util for setting linear movement for an axis */
static void SetLinearMovement(PxD6Joint* PD6Joint, PxD6Axis::Enum PAxis, uint8 Motion, bool bLockLimitSize)
{
	if(Motion == LCM_Locked || (Motion == LCM_Limited && bLockLimitSize))
	{
		PD6Joint->setMotion(PAxis, PxD6Motion::eLOCKED);
	}
	else if(Motion == LCM_Limited)
	{
		PD6Joint->setMotion(PAxis, PxD6Motion::eLIMITED);
	}
	else
	{
		PD6Joint->setMotion(PAxis, PxD6Motion::eFREE);
	}
}
#endif //WITH_PHYSX

void FConstraintInstance::UpdateLinearLimit()
{
#if WITH_PHYSX
	PxD6Joint* Joint = ConstraintData;
	if (Joint && !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		bool bLockLimitSize = (LinearLimitSize < RB_MinSizeToLockDOF);

		SetLinearMovement(Joint, PxD6Axis::eX, LinearXMotion, bLockLimitSize);
		SetLinearMovement(Joint, PxD6Axis::eY, LinearYMotion, bLockLimitSize);
		SetLinearMovement(Joint, PxD6Axis::eZ, LinearZMotion, bLockLimitSize);

		// If any DOF is locked/limited, set up the joint limit
		if (LinearXMotion != LCM_Free || LinearYMotion != LCM_Free || LinearZMotion != LCM_Free)
		{
			// If limit drops below RB_MinSizeToLockDOF, just pass RB_MinSizeToLockDOF to physics - that axis will be locked anyway, and PhysX dislikes 0 here
			float LinearLimit = FMath::Max<float>(LinearLimitSize, RB_MinSizeToLockDOF);
			PxJointLinearLimit PLinearLimit(GPhysXSDK->getTolerancesScale(), LinearLimit, 0.05f * GPhysXSDK->getTolerancesScale().length);
			SetSoftLimitParams(&PLinearLimit, bLinearLimitSoft, LinearLimitStiffness*AverageMass*CVarConstraintStiffnessScale.GetValueOnGameThread(), LinearLimitDamping*AverageMass*CVarConstraintDampingScale.GetValueOnGameThread());
			Joint->setLinearLimit(PLinearLimit);
		}
	}
#endif
}

void FConstraintInstance::UpdateAngularLimit()
{
#if WITH_PHYSX
	PxD6Joint* Joint = ConstraintData;
	if (Joint && !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		/////////////// TWIST LIMIT
		PxD6Motion::Enum TwistMotion = PxD6Motion::eFREE;
		if (AngularTwistMotion == ACM_Limited)
		{
			TwistMotion = PxD6Motion::eLIMITED;
			// If angle drops below RB_MinAngleToLockDOF, just pass RB_MinAngleToLockDOF to physics - that axis will be locked anyway, and PhysX dislikes 0 here
			float TwistLimitRad = TwistLimitAngle * (PI / 180.0f);
			PxJointAngularLimitPair PTwistLimitPair(-TwistLimitRad, TwistLimitRad, 1.f * (PI / 180.0f));
			SetSoftLimitParams(&PTwistLimitPair, bTwistLimitSoft, TwistLimitStiffness*AverageMass*CVarConstraintStiffnessScale.GetValueOnGameThread(), TwistLimitDamping*AverageMass*CVarConstraintDampingScale.GetValueOnGameThread());
			Joint->setTwistLimit(PTwistLimitPair);
		}
		else if (AngularTwistMotion == ACM_Locked)
		{
			TwistMotion = PxD6Motion::eLOCKED;
		}
		Joint->setMotion(PxD6Axis::eTWIST, TwistMotion);

		/////////////// SWING1 LIMIT
		PxD6Motion::Enum Swing1Motion = PxD6Motion::eFREE;
		PxD6Motion::Enum Swing2Motion = PxD6Motion::eFREE;

		if (AngularSwing1Motion == ACM_Limited)
		{
			Swing1Motion = PxD6Motion::eLIMITED;
		}
		else if (AngularSwing1Motion == ACM_Locked)
		{
			Swing1Motion = PxD6Motion::eLOCKED;
		}

		/////////////// SWING2 LIMIT
		if (AngularSwing2Motion == ACM_Limited)
		{
			Swing2Motion = PxD6Motion::eLIMITED;
		}
		else if (AngularSwing2Motion == ACM_Locked)
		{
			Swing2Motion = PxD6Motion::eLOCKED;
		}

		if (AngularSwing1Motion == ACM_Limited || AngularSwing2Motion == ACM_Limited)
		{
			//Clamp the limit value to valid range which PhysX won't ignore, both value have to be clamped even there is only one degree limit in constraint
			float Limit1Rad = FMath::ClampAngle(Swing1LimitAngle, KINDA_SMALL_NUMBER, 179.9999f) * (PI / 180.0f);
			float Limit2Rad = FMath::ClampAngle(Swing2LimitAngle, KINDA_SMALL_NUMBER, 179.9999f) * (PI / 180.0f);
			PxJointLimitCone PSwingLimitCone(Limit2Rad, Limit1Rad, 1.f * (PI / 180.0f));
			SetSoftLimitParams(&PSwingLimitCone, bSwingLimitSoft, SwingLimitStiffness*AverageMass*CVarConstraintStiffnessScale.GetValueOnGameThread(), SwingLimitDamping*AverageMass*CVarConstraintDampingScale.GetValueOnGameThread());
			Joint->setSwingLimit(PSwingLimitCone);
		}

		Joint->setMotion(PxD6Axis::eSWING2, Swing1Motion);
		Joint->setMotion(PxD6Axis::eSWING1, Swing2Motion);
	}
#endif
	
}

/** Constructor **/
FConstraintInstance::FConstraintInstance()
	: ConstraintIndex(0)
	, OwnerComponent(NULL)
#if WITH_PHYSX
	, ConstraintData(NULL)
#endif	//WITH_PHYSX
	, SceneIndex(0)
	, bSwingLimitSoft(true)
	, bTwistLimitSoft(true)
	, Swing1LimitAngle(45)
	, TwistLimitAngle(45)
	, Swing2LimitAngle(45)
	, SwingLimitStiffness(50)
	, SwingLimitDamping(5)
	, TwistLimitStiffness(50)
	, TwistLimitDamping(5)
	, bLinearPositionDrive(false)
	, bLinearVelocityDrive(false)
	, LinearPositionTarget(ForceInit)
	, LinearVelocityTarget(ForceInit)
	, LinearDriveSpring(50.0f)
	, LinearDriveDamping(1.0f)
	, LinearDriveForceLimit(0)
	, bAngularOrientationDrive(false)
	, bAngularVelocityDrive(false)
	, AngularOrientationTarget(ForceInit)
	, AngularVelocityTarget(ForceInit)
	, AngularDriveSpring(50.0f)
	, AngularDriveDamping(1.0f)
	, AngularDriveForceLimit(0)
	, AverageMass(0.f)
#if WITH_PHYSX
	, PhysxUserData(this)
#endif
{
	bDisableCollision = false;

	LinearXMotion = LCM_Locked;
	LinearYMotion = LCM_Locked;
	LinearZMotion = LCM_Locked;

	Pos1 = FVector(0.0f, 0.0f, 0.0f);
	PriAxis1 = FVector(1.0f, 0.0f, 0.0f);
	SecAxis1 = FVector(0.0f, 1.0f, 0.0f);

	Pos2 = FVector(0.0f, 0.0f, 0.0f);
	PriAxis2 = FVector(1.0f, 0.0f, 0.0f);
	SecAxis2 = FVector(0.0f, 1.0f, 0.0f);

	LinearBreakThreshold = 300.0f;
	AngularBreakThreshold = 500.0f;

	ProjectionLinearTolerance = 0.5; // Linear projection when error > 0.5 unreal units
	ProjectionAngularTolerance = 10.f;// Angular projection when error > 10 degrees
}

void FConstraintInstance::SetDisableCollision(bool InDisableCollision)
{
	bDisableCollision = InDisableCollision;
#if WITH_PHYSX
	PxD6Joint* Joint = ConstraintData;
	if (Joint && !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		PxConstraintFlags Flags = Joint->getConstraintFlags();
		if (bDisableCollision)
		{
			Flags &= ~PxConstraintFlag::eCOLLISION_ENABLED;
		}
		else
		{
			Flags |= PxConstraintFlag::eCOLLISION_ENABLED;
		}

		Joint->setConstraintFlags(Flags);
	}
#endif
}

/** 
 *	Create physics engine constraint.
 */
void FConstraintInstance::InitConstraint(USceneComponent* Owner, FBodyInstance* Body1, FBodyInstance* Body2, float Scale)
{
	OwnerComponent = Owner;

#if WITH_PHYSX
	PhysxUserData = FPhysxUserData(this);

	// if there's already a constraint, get rid of it first
	if (ConstraintData != NULL)
	{
		TermConstraint();
	}

	PxRigidActor* PActor1 = Body1 ? Body1->GetPxRigidActor() : NULL;
	PxRigidActor* PActor2 = Body2 ? Body2->GetPxRigidActor() : NULL;

	// Do not create joint unless you have two actors
	// Do not create joint unless one of the actors is dynamic
	if ((!PActor1 || !PActor1->isRigidBody()) && (!PActor2 || !PActor2->isRigidBody()))
	{
		return;
	}

	// Need to worry about the case where one is static and one is dynamic, and make sure the static scene is used which matches the dynamic scene
	if(PActor1 != NULL && PActor2 != NULL)
	{
		if (PActor1->isRigidStatic() && PActor2->isRigidBody())
		{
			const uint32 SceneType = Body2->RigidActorSync != NULL ? PST_Sync : PST_Async;
			PActor1 = Body1->GetPxRigidActor(SceneType);
		}
		else
		if (PActor2->isRigidStatic() && PActor1->isRigidBody())
		{
			const uint32 SceneType = Body1->RigidActorSync != NULL ? PST_Sync : PST_Async;
			PActor2 = Body2->GetPxRigidActor(SceneType);
		}
	}

	AverageMass = 0;
	{
		float TotalMass = 0;
		int NumDynamic = 0;

		if (PActor1 && PActor1->isRigidBody())
		{
			TotalMass += PActor1->isRigidBody()->getMass();
			++NumDynamic;
		}

		if (PActor2 && PActor2->isRigidBody())
		{
			TotalMass += PActor2->isRigidBody()->getMass();
			++NumDynamic;
		}

		check(NumDynamic);
		AverageMass = TotalMass / NumDynamic;
	}


	// record if actors are asleep before creating joint, so we can sleep them afterwards if so (creating joint wakes them)
	const bool bActor1Asleep = (PActor1 == NULL || !PActor1->isRigidDynamic() || PActor1->isRigidDynamic()->isSleeping());
	const bool bActor2Asleep = (PActor2 == NULL || !PActor2->isRigidDynamic() || PActor2->isRigidDynamic()->isSleeping());

	// make sure actors are in same scene
	PxScene* PScene1 = (PActor1 != NULL) ? PActor1->getScene() : NULL;
	PxScene* PScene2 = (PActor2 != NULL) ? PActor2->getScene() : NULL;

	// make sure actors are in same scene
	if(PScene1 && PScene2 && PScene1 != PScene2)
	{
		UE_LOG(LogPhysics, Log,  TEXT("Attempting to create a joint between actors in two different scenes.  No joint created.") );
		return;
	}

	PxScene* PScene = PScene1 ? PScene1 : PScene2;
	check(PScene);

	ConstraintData = NULL;

	FTransform Local1 = GetRefFrame(EConstraintFrame::Frame1);
	Local1.ScaleTranslation(FVector(Scale));
	checkf(Local1.IsValid() && !Local1.ContainsNaN(), TEXT("%s"), *Local1.ToString());

	FTransform Local2 = GetRefFrame(EConstraintFrame::Frame2);
	Local2.ScaleTranslation(FVector(Scale));
	checkf(Local2.IsValid() && !Local2.ContainsNaN(), TEXT("%s"), *Local2.ToString());

	SCOPED_SCENE_WRITE_LOCK(PScene);

	// Because PhysX keeps limits/axes locked in the first body reference frame, whereas Unreal keeps them in the second body reference frame, we have to flip the bodies here.
	PxD6Joint* PD6Joint = PxD6JointCreate(*GPhysXSDK, PActor2, U2PTransform(Local2), PActor1, U2PTransform(Local1));

	if(PD6Joint == NULL)
	{
		UE_LOG(LogPhysics, Log, TEXT("URB_ConstraintInstance::InitConstraint - Invalid 6DOF joint (%s)"), *JointName.ToString());
		return;
	}

	///////// POINTERS
	PD6Joint->userData = &PhysxUserData;

	// Remember reference to scene index.
	FPhysScene* RBScene = FPhysxUserData::Get<FPhysScene>(PScene->userData);
	if(RBScene->GetPhysXScene(PST_Sync) == PScene)
	{
		SceneIndex = RBScene->PhysXSceneIndex[PST_Sync];
	}
	else
	if(RBScene->GetPhysXScene(PST_Async) == PScene)
	{
		SceneIndex = RBScene->PhysXSceneIndex[PST_Async];
	}
	else
	{
		UE_LOG(LogPhysics, Log,  TEXT("URB_ConstraintInstance::InitConstraint: PxScene has inconsistent FPhysScene userData.  No joint created.") );
		return;
	}

	ConstraintData = PD6Joint;

	///////// FLAGS/PROJECTION

	PxConstraintFlags Flags = PxConstraintFlags();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Flags |= PxConstraintFlag::eVISUALIZATION;
#endif

	if(!bDisableCollision)
	{
		Flags |= PxConstraintFlag::eCOLLISION_ENABLED;
	}

	if( bEnableProjection )
	{
		Flags |= PxConstraintFlag::ePROJECTION;

		PD6Joint->setProjectionLinearTolerance(ProjectionLinearTolerance); 
		PD6Joint->setProjectionAngularTolerance(ProjectionAngularTolerance * ((float)PI/180.0f)); 
	}

	PD6Joint->setConstraintFlags(Flags);

	/////////////// ANUGULAR LIMIT
	UpdateAngularLimit();

	/////////////// LINEAR LIMIT
	UpdateLinearLimit();

	///////// BREAKABLE
	float LinearBreakForce = PX_MAX_REAL;
	float AngularBreakForce = PX_MAX_REAL;
	if (bLinearBreakable)
	{
		LinearBreakForce = LinearBreakThreshold;
	}

	if (bAngularBreakable)
	{
		AngularBreakForce = AngularBreakThreshold;
	}

	PD6Joint->setBreakForce(LinearBreakForce, AngularBreakForce);

	///////// DRIVE
	const PxReal LinearForceLimit = LinearDriveForceLimit > 0.0f ? LinearDriveForceLimit : PX_MAX_F32;
	const PxReal AngularForceLimit = AngularDriveForceLimit > 0.0f ? AngularDriveForceLimit : PX_MAX_F32;
	const bool bAccelerationDrive = true;

	// Set up the linear drives
	if ( bLinearPositionDrive || bLinearVelocityDrive )
	{
		// X-Axis linear drive
		float DriveSpring = bLinearXPositionDrive ? LinearDriveSpring : 0.0f;
		float DriveDamping = (bLinearVelocityDrive && FMath::Abs(LinearVelocityTarget.X) > 0.0f) ? LinearDriveDamping : 0.0f;

		PD6Joint->setDrive(PxD6Drive::eX, PxD6JointDrive(DriveSpring, DriveDamping, LinearForceLimit, bAccelerationDrive));

		// Y-Axis linear drive
		DriveSpring = bLinearYPositionDrive ? LinearDriveSpring : 0.0f;
		DriveDamping = (bLinearVelocityDrive && FMath::Abs(LinearVelocityTarget.Y) > 0.0f) ? LinearDriveDamping : 0.0f;

		PD6Joint->setDrive(PxD6Drive::eY, PxD6JointDrive(DriveSpring, DriveDamping, LinearForceLimit, bAccelerationDrive));

		// Z-Axis linear drive
		DriveSpring = bLinearZPositionDrive ? LinearDriveSpring : 0.0f;
		DriveDamping = (bLinearVelocityDrive && FMath::Abs(LinearVelocityTarget.Z) > 0.0f) ? LinearDriveDamping : 0.0f;

		PD6Joint->setDrive(PxD6Drive::eZ, PxD6JointDrive(DriveSpring, DriveDamping, LinearForceLimit, bAccelerationDrive));

		// Warn the user if he specified a drive for an axis that has no DOF
		if (LinearXMotion == LCM_Locked )
		{
			UE_LOG(LogPhysics, Warning,  TEXT("Attempting to create a linear joint drive for an locked axis.") );
		}
	}

	if ( bAngularOrientationDrive || bAngularVelocityDrive )
	{
		float DriveSpring = bAngularOrientationDrive ? AngularDriveSpring : 0.0f;
		float DriveDamping = bAngularVelocityDrive  ? AngularDriveDamping : 0.0f;

		if (AngularDriveMode == EAngularDriveMode::SLERP)
		{
			PD6Joint->setDrive(PxD6Drive::eSLERP, PxD6JointDrive(DriveSpring, DriveDamping, AngularForceLimit, bAccelerationDrive));
		}
		else
		{
			PD6Joint->setDrive(PxD6Drive::eTWIST, PxD6JointDrive(DriveSpring, DriveDamping, AngularForceLimit, bAccelerationDrive));
			PD6Joint->setDrive(PxD6Drive::eSWING, PxD6JointDrive(DriveSpring, DriveDamping, AngularForceLimit, bAccelerationDrive));
		}
	}

	FQuat OrientationTargetQuat(AngularOrientationTarget);

	PD6Joint->setDrivePosition(PxTransform(U2PVector(LinearPositionTarget), U2PQuat(OrientationTargetQuat)));
	PD6Joint->setDriveVelocity(U2PVector(LinearVelocityTarget), U2PVector(AngularVelocityTarget * 2 * PI));
	
	// creation of joints wakes up rigid bodies, so we put them to sleep again if both were initially asleep
	if(bActor1Asleep && bActor2Asleep)
	{
		if ( PActor1 != NULL && IsRigidBodyNonKinematic(PActor1->isRigidDynamic()))
		{
			PActor1->isRigidDynamic()->putToSleep();
		}

		if (PActor2 != NULL && IsRigidBodyNonKinematic(PActor2->isRigidDynamic()))
		{
			PActor2->isRigidDynamic()->putToSleep();
		}
	}
#endif // WITH_PHYSX

}

void FConstraintInstance::TermConstraint()
{
#if WITH_PHYSX
	if (!ConstraintData)
	{
		return;
	}

	// use correct scene
	PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
	if(PScene != NULL)
	{
		ConstraintData->release();
	}

	ConstraintData = NULL;
#endif
}

bool FConstraintInstance::IsTerminated() const
{
#if WITH_PHYSX
	return (ConstraintData == NULL);
#else 
	return true;
#endif //WITH_PHYSX
}

bool FConstraintInstance::IsValidConstraintInstance() const
{
#if WITH_PHYSX
	return ConstraintData != NULL;
#else
	return false;
#endif // WITH_PHYSX
}



void FConstraintInstance::CopyConstraintGeometryFrom(const FConstraintInstance* FromInstance)
{
	Pos1 = FromInstance->Pos1;
	PriAxis1 = FromInstance->PriAxis1;
	SecAxis1 = FromInstance->SecAxis1;

	Pos2 = FromInstance->Pos2;
	PriAxis2 = FromInstance->PriAxis2;
	SecAxis2 = FromInstance->SecAxis2;
}

void FConstraintInstance::CopyConstraintParamsFrom(const FConstraintInstance* FromInstance)
{
#if WITH_PHYSX
	check(!FromInstance->ConstraintData);
	check(!ConstraintData);
#endif
	check(FromInstance->SceneIndex == 0);

	*this = *FromInstance;
}

FTransform FConstraintInstance::GetRefFrame(EConstraintFrame::Type Frame) const
{
	FTransform Result;

	if(Frame == EConstraintFrame::Frame1)
	{
		Result = FTransform(PriAxis1, SecAxis1, PriAxis1 ^ SecAxis1, Pos1);
	}
	else
	{
		Result = FTransform(PriAxis2, SecAxis2, PriAxis2 ^ SecAxis2, Pos2);
	}

	float Error = FMath::Abs( Result.GetDeterminant() - 1.0f );
	if(Error > 0.01f)
	{
		UE_LOG(LogPhysics, Warning,  TEXT("FConstraintInstance::GetRefFrame : Contained scale."));
	}

	return Result;
}

#if WITH_PHYSX
FORCEINLINE physx::PxJointActorIndex::Enum U2PConstraintFrame(EConstraintFrame::Type Frame)
{
	// Swap frame order, since Unreal reverses physx order
	return (Frame == EConstraintFrame::Frame1) ? physx::PxJointActorIndex::eACTOR1 : physx::PxJointActorIndex::eACTOR0;
}
#endif


void FConstraintInstance::SetRefFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame)
{
#if WITH_PHYSX
	PxJointActorIndex::Enum PxFrame = U2PConstraintFrame(Frame);
#endif
	if(Frame == EConstraintFrame::Frame1)
	{
		Pos1 = RefFrame.GetTranslation();
		PriAxis1 = RefFrame.GetUnitAxis( EAxis::X );
		SecAxis1 = RefFrame.GetUnitAxis( EAxis::Y );
	}
	else
	{
		Pos2 = RefFrame.GetTranslation();
		PriAxis2 = RefFrame.GetUnitAxis( EAxis::X );
		SecAxis2 = RefFrame.GetUnitAxis( EAxis::Y );
	}

#if WITH_PHYSX
	if (ConstraintData && !(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
	PxTransform PxRefFrame = U2PTransform(RefFrame);
	ConstraintData->setLocalPose(PxFrame, PxRefFrame);
	}
#endif

}

void FConstraintInstance::SetRefPosition(EConstraintFrame::Type Frame, const FVector& RefPosition)
{
#if WITH_PHYSX
	PxJointActorIndex::Enum PxFrame = U2PConstraintFrame(Frame);
#endif

	if (Frame == EConstraintFrame::Frame1)
	{
		Pos1 = RefPosition;
	}
	else
	{
		Pos2 = RefPosition;
	}

#if WITH_PHYSX
	if (ConstraintData && !(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		PxTransform PxRefFrame = ConstraintData->getLocalPose(PxFrame);
		PxRefFrame.p = U2PVector(RefPosition);
		ConstraintData->setLocalPose(PxFrame, PxRefFrame);
	}
#endif
}

void FConstraintInstance::SetRefOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis)
{
#if WITH_PHYSX
	PxJointActorIndex::Enum PxFrame = U2PConstraintFrame(Frame);
	FVector RefPos;
#endif
		
	if (Frame == EConstraintFrame::Frame1)
	{
		RefPos = Pos1;
		PriAxis1 = PriAxis;
		SecAxis1 = SecAxis;
	}
	else
	{
		RefPos = Pos2;
		PriAxis2 = PriAxis;
		SecAxis2 = SecAxis;
	}
	
#if WITH_PHYSX
	if (ConstraintData && !(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		FTransform URefTransform = FTransform(PriAxis1, SecAxis, PriAxis ^ SecAxis, RefPos);
		PxTransform PxRefFrame = U2PTransform(URefTransform);
		ConstraintData->setLocalPose(PxFrame, PxRefFrame);
		}
#endif
}

/** Get the position of this constraint in world space. */
FVector FConstraintInstance::GetConstraintLocation()
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)	ConstraintData;
	if (!Joint)
	{
		return FVector::ZeroVector;
	}

	PxRigidActor* JointActor0, *JointActor1;
	Joint->getActors(JointActor0, JointActor1);

	PxVec3 JointPos(0);

	// get the first anchor point in global frame
	if(JointActor0)
	{
		JointPos = JointActor0->getGlobalPose().transform(Joint->getLocalPose(PxJointActorIndex::eACTOR0).p);
	}

	// get the second archor point in global frame
	if(JointActor1)
	{
		JointPos += JointActor1->getGlobalPose().transform(Joint->getLocalPose(PxJointActorIndex::eACTOR1).p);
	}

	JointPos *= 0.5f;
	
	return P2UVector(JointPos);

#else
	return FVector::ZeroVector;
#endif
}

void FConstraintInstance::GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce)
{
#if WITH_PHYSX
	if (ConstraintData && !(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		PxVec3 PxOutLinearForce;
		PxVec3 PxOutAngularForce;
		ConstraintData->getConstraint()->getForce(PxOutLinearForce, PxOutAngularForce);

		OutLinearForce = P2UVector(PxOutLinearForce);
		OutAngularForce = P2UVector(PxOutAngularForce);
	}
	else
	{
		OutLinearForce = FVector::ZeroVector;
		OutAngularForce = FVector::ZeroVector;
	}

#else
	OutLinearForce = FVector::ZeroVector;
	OutAngularForce = FVector::ZeroVector;
#endif
}



/** Function for turning linear position drive on and off. */
void FConstraintInstance::SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	
	if (Joint && !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		// Get the current drives
		PxD6JointDrive CurrentDriveX = Joint->getDrive(PxD6Drive::eX);
		PxD6JointDrive CurrentDriveY = Joint->getDrive(PxD6Drive::eY);
		PxD6JointDrive CurrentDriveZ = Joint->getDrive(PxD6Drive::eZ);
		
		CurrentDriveX.stiffness = bEnableXDrive ? LinearDriveSpring : 0.0f;
		CurrentDriveY.stiffness = bEnableYDrive ? LinearDriveSpring : 0.0f;
		CurrentDriveZ.stiffness = bEnableZDrive ? LinearDriveSpring : 0.0f;

		Joint->setDrive(PxD6Drive::eX, CurrentDriveX);
		Joint->setDrive(PxD6Drive::eY, CurrentDriveY);
		Joint->setDrive(PxD6Drive::eZ, CurrentDriveZ);
	}
#endif

	bLinearXPositionDrive = bEnableXDrive;
	bLinearYPositionDrive = bEnableYDrive;
	bLinearZPositionDrive = bEnableZDrive;
	bLinearPositionDrive = bEnableXDrive || bEnableYDrive || bEnableZDrive;
}


/** Function for turning linear velocity drive on and off. */
void FConstraintInstance::SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;

	if (Joint && !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		// Get the current drives
		PxD6JointDrive CurrentDriveX = Joint->getDrive(PxD6Drive::eX);
		PxD6JointDrive CurrentDriveY = Joint->getDrive(PxD6Drive::eY);
		PxD6JointDrive CurrentDriveZ = Joint->getDrive(PxD6Drive::eZ);

		CurrentDriveX.damping = bEnableXDrive && FMath::Abs(LinearVelocityTarget.X) > 0.0f ? LinearDriveDamping : 0.0f;
		CurrentDriveY.damping = bEnableYDrive && FMath::Abs(LinearVelocityTarget.Y) > 0.0f ? LinearDriveDamping : 0.0f;
		CurrentDriveZ.damping = bEnableZDrive && FMath::Abs(LinearVelocityTarget.Z) > 0.0f ? LinearDriveDamping : 0.0f;

		Joint->setDrive(PxD6Drive::eX, CurrentDriveX);
		Joint->setDrive(PxD6Drive::eY, CurrentDriveY);
		Joint->setDrive(PxD6Drive::eZ, CurrentDriveZ);
	}
#endif

	bLinearVelocityDrive = bEnableXDrive || bEnableYDrive || bEnableZDrive;
}


/** Function for turning angular position drive on and off. */
void FConstraintInstance::SetAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint &&  !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		// Get the current drives
		PxD6JointDrive CurrentDriveSwing = Joint->getDrive(PxD6Drive::eSWING);
		PxD6JointDrive CurrentDriveTwist = Joint->getDrive(PxD6Drive::eTWIST);
		PxD6JointDrive CurrentDriveSlerp = Joint->getDrive(PxD6Drive::eSLERP);


		CurrentDriveSwing.stiffness = bEnableSwingDrive ? AngularDriveSpring : 0.0f;
		CurrentDriveTwist.stiffness = bEnableTwistDrive ? AngularDriveSpring : 0.0f;
		CurrentDriveSlerp.stiffness = (bEnableSwingDrive && bEnableTwistDrive) ? AngularDriveSpring : 0.0f;

		Joint->setDrive(PxD6Drive::eSWING, CurrentDriveSwing);
		Joint->setDrive(PxD6Drive::eTWIST, CurrentDriveTwist);
		Joint->setDrive(PxD6Drive::eSLERP, CurrentDriveSlerp);
		
		bAngularOrientationDrive = bEnableSwingDrive || bEnableTwistDrive;
	}
#endif
}

/** Function for turning angular velocity drive on and off. */
void FConstraintInstance::SetAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint &&  !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		// Get the current drives
		PxD6JointDrive CurrentDriveSwing = Joint->getDrive(PxD6Drive::eSWING);
		PxD6JointDrive CurrentDriveTwist = Joint->getDrive(PxD6Drive::eTWIST);
		PxD6JointDrive CurrentDriveSlerp = Joint->getDrive(PxD6Drive::eSLERP);

		CurrentDriveSwing.damping = bEnableSwingDrive ? AngularDriveDamping : 0.0f;
		CurrentDriveTwist.damping = bEnableTwistDrive ? AngularDriveDamping : 0.0f;
		CurrentDriveSlerp.damping = (bEnableSwingDrive && bEnableTwistDrive) ? AngularDriveDamping : 0.0f;

		Joint->setDrive(PxD6Drive::eSWING, CurrentDriveSwing);
		Joint->setDrive(PxD6Drive::eTWIST, CurrentDriveTwist);
		Joint->setDrive(PxD6Drive::eSLERP, CurrentDriveSlerp);
		
		bAngularVelocityDrive = bEnableSwingDrive || bEnableTwistDrive;
	}
#endif
}

/** Function for setting linear position target. */
void FConstraintInstance::SetLinearPositionTarget(const FVector& InPosTarget)
{
	// If settings are the same, don't do anything.
	if( LinearPositionTarget == InPosTarget )
	{
		return;
	}

#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		PxVec3 Pos = U2PVector(InPosTarget);
		Joint->setDrivePosition(PxTransform(Pos, Joint->getDrivePosition().q));
	}
#endif

	LinearPositionTarget = InPosTarget;
}

/** Function for setting linear velocity target. */
void FConstraintInstance::SetLinearVelocityTarget(const FVector& InVelTarget)
{
	// If settings are the same, don't do anything.
	if( LinearVelocityTarget == InVelTarget )
	{
		return;
	}

#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		PxVec3 CurrentLinearVel, CurrentAngVel;
		Joint->getDriveVelocity(CurrentLinearVel, CurrentAngVel);

		PxVec3 NewLinearVel = U2PVector(InVelTarget);
		Joint->setDriveVelocity(NewLinearVel, CurrentAngVel);
	}
#endif
	LinearVelocityTarget = InVelTarget;
}

/** Function for setting angular motor parameters. */
void FConstraintInstance::SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint &&  !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		float LinearForceLimit = InForceLimit;

		PxD6JointDrive NewJointDrive(InSpring, InDamping, LinearForceLimit > 0.f ? LinearForceLimit : PX_MAX_F32, Joint->getDrive(PxD6Drive::eX).flags);
		Joint->setDrive(PxD6Drive::eX, NewJointDrive);

		NewJointDrive.flags = Joint->getDrive(PxD6Drive::eY).flags;
		Joint->setDrive(PxD6Drive::eY, NewJointDrive);

		NewJointDrive.flags = Joint->getDrive(PxD6Drive::eZ).flags;
		Joint->setDrive(PxD6Drive::eZ, NewJointDrive);
	}
#endif

	LinearDriveSpring = InSpring;
	LinearDriveDamping = InDamping;
	LinearDriveForceLimit = InForceLimit;
}

/** Function for setting target angular position. */
void FConstraintInstance::SetAngularOrientationTarget(const FQuat& InPosTarget)
{
	FRotator OrientationTargetRot(InPosTarget);

	// If settings are the same, don't do anything.
	if( AngularOrientationTarget == OrientationTargetRot )
	{
		return;
	}
	
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		PxQuat Quat = U2PQuat(InPosTarget);
		Joint->setDrivePosition(PxTransform(Joint->getDrivePosition().p, Quat));
	}
#endif

	AngularOrientationTarget = OrientationTargetRot;
}

float FConstraintInstance::GetCurrentSwing1() const
{
	float Swing1 = 0.f;
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		Swing1 = Joint->getSwingZAngle();
	}
#endif

	return Swing1;
}

float FConstraintInstance::GetCurrentSwing2() const
{
	float Swing2 = 0.f;
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		Swing2 = Joint->getSwingYAngle();
	}
#endif

	return Swing2;
}

float FConstraintInstance::GetCurrentTwist() const
{
	float Twist = 0.f;
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		Twist = Joint->getTwist();
	}
#endif

	return Twist;
}


/** Function for setting target angular velocity. */
void FConstraintInstance::SetAngularVelocityTarget(const FVector& InVelTarget)
{
	// If settings are the same, don't do anything.
	if( AngularVelocityTarget == InVelTarget )
	{
		return;
	}

#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint)
	{
		PxVec3 CurrentLinearVel, CurrentAngVel;
		Joint->getDriveVelocity(CurrentLinearVel, CurrentAngVel);

		PxVec3 AngVel = U2PVector(InVelTarget * 2 * (float)PI); // Convert from revs per second to radians
		Joint->setDriveVelocity(CurrentLinearVel, AngVel);
	}
#endif

	AngularVelocityTarget = InVelTarget;
}

/** Function for setting angular motor parameters. */
void FConstraintInstance::SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit)
{
#if WITH_PHYSX
	
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint &&  !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		float AngularForceLimit = InForceLimit;

		PxD6JointDrive NewJointDrive(InSpring, InDamping, AngularForceLimit > 0.f ? AngularForceLimit : PX_MAX_F32, Joint->getDrive(PxD6Drive::eSWING).flags);
		Joint->setDrive(PxD6Drive::eSWING, NewJointDrive);

		NewJointDrive.flags = Joint->getDrive(PxD6Drive::eTWIST).flags;
		Joint->setDrive(PxD6Drive::eTWIST, NewJointDrive);

		NewJointDrive.flags = Joint->getDrive(PxD6Drive::eSLERP).flags;
		Joint->setDrive(PxD6Drive::eSLERP, NewJointDrive);
	}
#endif

	AngularDriveSpring = InSpring;
	AngularDriveDamping = InDamping;
	AngularDriveForceLimit = InForceLimit;
}






/** Scale Angular Limit Constraints (as defined in RB_ConstraintSetup) */
void FConstraintInstance::SetAngularDOFLimitScale(float InSwing1LimitScale, float InSwing2LimitScale, float InTwistLimitScale)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint &&  !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		if ( AngularSwing1Motion == ACM_Limited || AngularSwing2Motion == ACM_Limited )
		{
			// PhysX swing directions are different from Unreal's - so change here.
			if (AngularSwing1Motion == ACM_Limited)
			{
				Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
			}

			if (AngularSwing2Motion == ACM_Limited)
			{
				Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
			}
		
			//The limite values need to be clamped so it will be valid in PhysX
			PxReal ZLimitAngle = FMath::ClampAngle(Swing1LimitAngle * InSwing1LimitScale, KINDA_SMALL_NUMBER, 179.9999f) * (PI/180.0f);
			PxReal YLimitAngle = FMath::ClampAngle(Swing2LimitAngle * InSwing2LimitScale, KINDA_SMALL_NUMBER, 179.9999f) * (PI/180.0f);
			PxReal LimitContractDistance =  1.f * (PI/180.0f);

			Joint->setSwingLimit(PxJointLimitCone(YLimitAngle, ZLimitAngle, LimitContractDistance));
		}

		if ( AngularSwing1Motion == ACM_Locked )
		{
			Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLOCKED);
		}

		if ( AngularSwing2Motion == ACM_Locked )
		{
			Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLOCKED);
		}

		if ( AngularTwistMotion == ACM_Limited )
		{
			Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
			const float TwistLimitRad	= TwistLimitAngle * InTwistLimitScale * (PI/180.0f);
			PxReal LimitContractDistance =  1.f * (PI/180.0f);

			Joint->setTwistLimit(PxJointAngularLimitPair(-TwistLimitRad, TwistLimitRad, LimitContractDistance)); 
		}
		else if ( AngularTwistMotion == ACM_Locked )
		{
			Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLOCKED);
		}
		
	}
#endif
}

/** Allows you to dynamically change the size of the linear limit 'sphere'. */
void FConstraintInstance::SetLinearLimitSize(float NewLimitSize)
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)ConstraintData;
	if (Joint &&  !(Joint->getConstraintFlags()&PxConstraintFlag::eBROKEN))
	{
		PxReal LimitContractDistance =  1.f * (PI/180.0f);
		Joint->setLinearLimit(PxJointLinearLimit(GPhysXSDK->getTolerancesScale(), NewLimitSize, LimitContractDistance * GPhysXSDK->getTolerancesScale().length)); // LOC_MOD33 need to scale the contactDistance if not using its default value
	}
#endif
}


void FConstraintInstance::ConfigureAsHinge(bool bOverwriteLimits)
{
	LinearXMotion = LCM_Locked;
	LinearYMotion = LCM_Locked;
	LinearZMotion = LCM_Locked;
	AngularSwing1Motion = ACM_Locked;
	AngularSwing2Motion = ACM_Locked;
	AngularTwistMotion = ACM_Free;

	if (bOverwriteLimits)
	{
		Swing1LimitAngle = 0.0f;
		Swing2LimitAngle = 0.0f;
		TwistLimitAngle = 45.0f;
	}
}

void FConstraintInstance::ConfigureAsPrismatic(bool bOverwriteLimits)
{
	LinearXMotion = LCM_Free;
	LinearYMotion = LCM_Locked;
	LinearZMotion = LCM_Locked;
	AngularSwing1Motion = ACM_Locked;
	AngularSwing2Motion = ACM_Locked;
	AngularTwistMotion = ACM_Locked;

	if (bOverwriteLimits)
	{
		Swing1LimitAngle = 0.0f;
		Swing2LimitAngle = 0.0f;
		TwistLimitAngle = 0.0f;
	}
}

void FConstraintInstance::ConfigureAsSkelJoint(bool bOverwriteLimits)
{
	LinearXMotion = LCM_Locked;
	LinearYMotion = LCM_Locked;
	LinearZMotion = LCM_Locked;
	AngularSwing1Motion = ACM_Limited;
	AngularSwing2Motion = ACM_Limited;
	AngularTwistMotion = ACM_Limited;

	if (bOverwriteLimits)
	{
		Swing1LimitAngle = 45.0f;
		Swing2LimitAngle = 45.0f;
		TwistLimitAngle = 15.0f;
	}
}

void FConstraintInstance::ConfigureAsBS(bool bOverwriteLimits)
{
	LinearXMotion = LCM_Locked;
	LinearYMotion = LCM_Locked;
	LinearZMotion = LCM_Locked;
	AngularSwing1Motion = ACM_Free;
	AngularSwing2Motion = ACM_Free;
	AngularTwistMotion = ACM_Free;
	
	if (bOverwriteLimits)
	{
		Swing1LimitAngle = 0.0f;
		Swing2LimitAngle = 0.0f;
		TwistLimitAngle = 0.0f;
	}
}

bool FConstraintInstance::IsHinge() const
{
	int32 AngularDOF = 0;
	AngularDOF += AngularSwing1Motion != ACM_Locked ? 1 : 0;
	AngularDOF += AngularSwing2Motion != ACM_Locked ? 1 : 0;
	AngularDOF += AngularTwistMotion != ACM_Locked ? 1 : 0;

	return	LinearXMotion		== LCM_Locked &&
			LinearYMotion		== LCM_Locked &&
			LinearZMotion		== LCM_Locked &&
			AngularDOF == 1;
}

bool FConstraintInstance::IsPrismatic() const
{
	int32 LinearDOF = 0;
	LinearDOF += LinearXMotion != LCM_Locked ? 1 : 0;
	LinearDOF += LinearYMotion != LCM_Locked ? 1 : 0;
	LinearDOF += LinearZMotion != LCM_Locked ? 1 : 0;

	return	LinearDOF > 0 &&
			AngularSwing1Motion == ACM_Locked &&
			AngularSwing2Motion == ACM_Locked &&
			AngularTwistMotion	== ACM_Locked;
}

bool FConstraintInstance::IsSkelJoint() const
{
	return	LinearXMotion		== LCM_Locked &&
			LinearYMotion		== LCM_Locked &&
			LinearZMotion		== LCM_Locked &&
			AngularSwing1Motion == ACM_Limited &&
			AngularSwing2Motion == ACM_Limited &&
			AngularTwistMotion	== ACM_Limited;
}

bool FConstraintInstance::IsBallAndSocket() const
{
	int32 AngularDOF = 0;
	AngularDOF += AngularSwing1Motion != ACM_Locked ? 1 : 0;
	AngularDOF += AngularSwing2Motion != ACM_Locked ? 1 : 0;
	AngularDOF += AngularTwistMotion != ACM_Locked ? 1 : 0;

	return	LinearXMotion		== LCM_Locked &&
			LinearYMotion		== LCM_Locked &&
			LinearZMotion		== LCM_Locked &&
			AngularDOF > 1;
}

void FConstraintInstance::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_STIFFNESS_AND_DAMPING_SCALE)
	{
		LinearLimitStiffness	/= CVarConstraintStiffnessScale.GetValueOnGameThread();
		SwingLimitStiffness		/= CVarConstraintStiffnessScale.GetValueOnGameThread();
		TwistLimitStiffness		/= CVarConstraintStiffnessScale.GetValueOnGameThread();
		LinearLimitDamping		/=  CVarConstraintDampingScale.GetValueOnGameThread();
		SwingLimitDamping		/=  CVarConstraintDampingScale.GetValueOnGameThread();
		TwistLimitDamping		/=  CVarConstraintDampingScale.GetValueOnGameThread();
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_MOTOR_UNITS)
	{
		AngularVelocityTarget *= 1.f / (2.f * PI);	//we want to use revolutions per second - old system was using radians directly
	}
}

void FConstraintInstance::OnConstraintBroken()
{
	UPhysicsConstraintComponent* ConstraintComp = Cast<UPhysicsConstraintComponent>(OwnerComponent);
	USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponent);

	if (ConstraintComp)
	{
		ConstraintComp->OnConstraintBroken.Broadcast(ConstraintIndex);
	}
	else if (SkelMeshComp)
	{
		SkelMeshComp->OnConstraintBroken.Broadcast(ConstraintIndex);
	}

}

void FConstraintInstance::SetLinearXLimit(ELinearConstraintMotion XMotion, float InLinearLimitSize)
{
	LinearXMotion = XMotion;
	LinearLimitSize = InLinearLimitSize;

	UpdateLinearLimit();
}

void FConstraintInstance::SetLinearYLimit(ELinearConstraintMotion YMotion, float InLinearLimitSize)
{
	LinearYMotion = YMotion;
	LinearLimitSize = InLinearLimitSize;
	
	UpdateLinearLimit();
}

void FConstraintInstance::SetLinearZLimit(ELinearConstraintMotion ZMotion, float InLinearLimitSize)
{
	LinearZMotion = ZMotion;
	LinearLimitSize = InLinearLimitSize;

	UpdateLinearLimit();
}

void FConstraintInstance::SetAngularSwing1Limit(EAngularConstraintMotion Swing1Motion, float InSwing1LimitAngle)
{
	AngularSwing1Motion = Swing1Motion;
	Swing1LimitAngle = InSwing1LimitAngle;
	
	UpdateAngularLimit();
}

void FConstraintInstance::SetAngularSwing2Limit(EAngularConstraintMotion Swing2Motion, float InSwing2LimitAngle)
{
	AngularSwing2Motion = Swing2Motion;
	Swing2LimitAngle = InSwing2LimitAngle;

	UpdateAngularLimit();
}

void FConstraintInstance::SetAngularTwistLimit(EAngularConstraintMotion TwistMotion, float InTwistLimitAngle)
{
	AngularTwistMotion = TwistMotion;
	TwistLimitAngle = InTwistLimitAngle;

	UpdateAngularLimit();
}

//Hacks to easily get zeroed memory for special case when we don't use GC
void FConstraintInstance::Free(FConstraintInstance * Ptr)
{
	Ptr->~FConstraintInstance();
	FMemory::Free(Ptr);
}
FConstraintInstance * FConstraintInstance::Alloc()
{
	void* Memory = FMemory::Malloc(sizeof(FConstraintInstance));
	FMemory::Memzero(Memory, sizeof(FConstraintInstance));
	return new (Memory)FConstraintInstance();
}

#undef LOCTEXT_NAMESPACE