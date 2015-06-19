// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "PhysXSupport.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "ConstraintInstance"

const bool bIsAccelerationDrive = true;

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

float RevolutionsToRads(const float Revolutions)
{
	return Revolutions * 2.f * PI;
}

FVector RevolutionsToRads(const FVector Revolutions)
{
	return Revolutions * 2.f * PI;
}

#if WITH_PHYSX
/** Util for setting soft limit params */
void SetSoftLimitParams_AssumesLocked(PxJointLimitParameters* PLimit, bool bSoft, float Spring, float Damping)
{
	if(bSoft)
	{
		PLimit->stiffness = Spring * CVarConstraintStiffnessScale.GetValueOnGameThread();
		PLimit->damping = Damping * CVarConstraintDampingScale.GetValueOnGameThread();
	}
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

physx::PxD6Joint* FConstraintInstance::GetUnbrokenJoint_AssumesLocked() const
{
	return (ConstraintData && !(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN)) ? ConstraintData : nullptr;
}

bool FConstraintInstance::ExecuteOnUnbrokenJointReadOnly(TFunctionRef<void(const physx::PxD6Joint*)> Func) const
{
	if(ConstraintData)
	{
		SCOPED_SCENE_READ_LOCK(ConstraintData->getScene());

		if(!(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
		{
			Func(ConstraintData);
			return true;
		}
	}

	return false;
}

bool FConstraintInstance::ExecuteOnUnbrokenJointReadWrite(TFunctionRef<void(physx::PxD6Joint*)> Func) const
{
	if (ConstraintData)
	{
		SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());

		if (!(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
		{
			Func(ConstraintData);
			return true;
		}
	}

	return false;
}
#endif //WITH_PHYSX

void FConstraintInstance::UpdateLinearLimit()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		bool bLockLimitSize = (LinearLimitSize < RB_MinSizeToLockDOF);

		SetLinearMovement_AssumesLocked<PxD6Axis::eX>(Joint, LinearXMotion, bLockLimitSize);
		SetLinearMovement_AssumesLocked<PxD6Axis::eY>(Joint, LinearYMotion, bLockLimitSize);
		SetLinearMovement_AssumesLocked<PxD6Axis::eZ>(Joint, LinearZMotion, bLockLimitSize);

		// If any DOF is locked/limited, set up the joint limit
		if (LinearXMotion != LCM_Free || LinearYMotion != LCM_Free || LinearZMotion != LCM_Free)
		{
			// If limit drops below RB_MinSizeToLockDOF, just pass RB_MinSizeToLockDOF to physics - that axis will be locked anyway, and PhysX dislikes 0 here
			float LinearLimit = FMath::Max<float>(LinearLimitSize, RB_MinSizeToLockDOF);
			PxJointLinearLimit PLinearLimit(GPhysXSDK->getTolerancesScale(), LinearLimit, 0.05f * GPhysXSDK->getTolerancesScale().length);
			SetSoftLimitParams_AssumesLocked(&PLinearLimit, bLinearLimitSoft, LinearLimitStiffness*AverageMass, LinearLimitDamping*AverageMass);
			Joint->setLinearLimit(PLinearLimit);
		}
	});
#endif
}



void FConstraintInstance::UpdateAngularLimit()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		/////////////// TWIST LIMIT
		PxD6Motion::Enum TwistMotion = PxD6Motion::eFREE;
		if (AngularTwistMotion == ACM_Limited)
		{
			TwistMotion = PxD6Motion::eLIMITED;
			// If angle drops below RB_MinAngleToLockDOF, just pass RB_MinAngleToLockDOF to physics - that axis will be locked anyway, and PhysX dislikes 0 here
			float TwistLimitRad = FMath::DegreesToRadians(TwistLimitAngle);
			PxJointAngularLimitPair PTwistLimitPair(-TwistLimitRad, TwistLimitRad, FMath::DegreesToRadians(1.f));
			SetSoftLimitParams_AssumesLocked(&PTwistLimitPair, bTwistLimitSoft, TwistLimitStiffness*AverageMass, TwistLimitDamping*AverageMass);
			Joint->setTwistLimit(PTwistLimitPair);
		}
		else if (AngularTwistMotion == ACM_Locked)
		{
			TwistMotion = PxD6Motion::eLOCKED;
		}
		Joint->setMotion(PxD6Axis::eTWIST, TwistMotion);

		/////////////// SWING1 LIMIT
		const PxD6Motion::Enum Swing1Motion = U2PAngularMotion(AngularSwing1Motion);
		const PxD6Motion::Enum Swing2Motion = U2PAngularMotion(AngularSwing2Motion);

		if (AngularSwing1Motion == ACM_Limited || AngularSwing2Motion == ACM_Limited)
		{
			//Clamp the limit value to valid range which PhysX won't ignore, both value have to be clamped even there is only one degree limit in constraint
			float Limit1Rad = FMath::DegreesToRadians(FMath::ClampAngle(Swing1LimitAngle, KINDA_SMALL_NUMBER, 179.9999f));
			float Limit2Rad = FMath::DegreesToRadians(FMath::ClampAngle(Swing2LimitAngle, KINDA_SMALL_NUMBER, 179.9999f));
			PxJointLimitCone PSwingLimitCone(Limit2Rad, Limit1Rad, FMath::DegreesToRadians(1.f));
			SetSoftLimitParams_AssumesLocked(&PSwingLimitCone, bSwingLimitSoft, SwingLimitStiffness*AverageMass, SwingLimitDamping*AverageMass);
			Joint->setSwingLimit(PSwingLimitCone);
		}

		Joint->setMotion(PxD6Axis::eSWING2, Swing1Motion);
		Joint->setMotion(PxD6Axis::eSWING1, Swing2Motion);
	});
#endif
	
}

void FConstraintInstance::UpdateBreakable()
{
	const float LinearBreakForce = bLinearBreakable ? LinearBreakThreshold : PX_MAX_REAL;
	const float AngularBreakForce = bAngularBreakable ? AngularBreakThreshold : PX_MAX_REAL;

#if WITH_PHYSX
	ConstraintData->setBreakForce(LinearBreakForce, AngularBreakForce);
#endif
}

void FConstraintInstance::UpdateDriveTarget()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		FQuat OrientationTargetQuat(AngularOrientationTarget);

		Joint->setDrivePosition(PxTransform(U2PVector(LinearPositionTarget), U2PQuat(OrientationTargetQuat)));
		Joint->setDriveVelocity(U2PVector(LinearVelocityTarget), U2PVector(RevolutionsToRads(AngularVelocityTarget)));
	});
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
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
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
	});
#endif
}

#if WITH_PHYSX
float ComputeAverageMass_AssumesLocked(const PxRigidActor* PActor1, const PxRigidActor* PActor2)
{
	float AverageMass = 0;
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

	return AverageMass;
}

/** Finds the common scene and appropriate actors for the passed in body instances. Makes sure to do this without requiring a scene lock*/
PxScene* GetPScene_LockFree(const FBodyInstance* Body1, const FBodyInstance* Body2)
{
	const int32 SceneIndex1 = Body1 ? Body1->GetSceneIndex() : -1;
	const int32 SceneIndex2 = Body2 ? Body2->GetSceneIndex() : -1;
	PxScene* PScene = nullptr;

	//now we check if the two actors are valid
	if(SceneIndex1 == -1 && SceneIndex2 == -1)
	{
		UE_LOG(LogPhysics, Log, TEXT("Attempting to create a joint between two null actors.  No joint created."));
	}
	else if(SceneIndex1 >= 0 && SceneIndex2 >= 0 && SceneIndex1 != SceneIndex2)
	{
		UE_LOG(LogPhysics, Log, TEXT("Attempting to create a joint between two actors in different scenes.  No joint created."));
	}
	else
	{
		PScene = GetPhysXSceneFromIndex(SceneIndex1);
	}

	return PScene;
}

/*various logical checks to find the correct physx actor. Returns true if found valid actors that can be constrained*/
bool GetPActors_AssumesLocked(const FBodyInstance* Body1, const FBodyInstance* Body2, PxRigidActor** PActor1Out, PxRigidActor** PActor2Out)
{
	PxRigidActor* PActor1 = Body1 ? Body1->GetPxRigidActor_AssumesLocked() : NULL;
	PxRigidActor* PActor2 = Body2 ? Body2->GetPxRigidActor_AssumesLocked() : NULL;

	// Do not create joint unless you have two actors
	// Do not create joint unless one of the actors is dynamic
	if ((!PActor1 || !PActor1->isRigidBody()) && (!PActor2 || !PActor2->isRigidBody()))
	{
		UE_LOG(LogPhysics, Log, TEXT("Attempting to create a joint between actors that are static.  No joint created."));
		return false;
	}

	// Need to worry about the case where one is static and one is dynamic, and make sure the static scene is used which matches the dynamic scene
	if (PActor1 != NULL && PActor2 != NULL)
	{
		if (PActor1->isRigidStatic() && PActor2->isRigidBody())
		{
			const uint32 SceneType = Body2->RigidActorSync != NULL ? PST_Sync : PST_Async;
			PActor1 = Body1->GetPxRigidActor_AssumesLocked(SceneType);
		}
		else
		if (PActor2->isRigidStatic() && PActor1->isRigidBody())
		{
			const uint32 SceneType = Body1->RigidActorSync != NULL ? PST_Sync : PST_Async;
			PActor2 = Body2->GetPxRigidActor_AssumesLocked(SceneType);
		}
	}

	*PActor1Out = PActor1;
	*PActor2Out = PActor2;
	return true;
}

bool FConstraintInstance::CreatePxJoint_AssumesLocked(physx::PxRigidActor* PActor1, physx::PxRigidActor* PActor2, physx::PxScene* PScene, const float Scale)
{
	ConstraintData = nullptr;

	FTransform Local1 = GetRefFrame(EConstraintFrame::Frame1);
	Local1.ScaleTranslation(FVector(Scale));
	checkf(Local1.IsValid() && !Local1.ContainsNaN(), TEXT("%s"), *Local1.ToString());

	FTransform Local2 = GetRefFrame(EConstraintFrame::Frame2);
	Local2.ScaleTranslation(FVector(Scale));
	checkf(Local2.IsValid() && !Local2.ContainsNaN(), TEXT("%s"), *Local2.ToString());

	SCOPED_SCENE_WRITE_LOCK(PScene);

	// Because PhysX keeps limits/axes locked in the first body reference frame, whereas Unreal keeps them in the second body reference frame, we have to flip the bodies here.
	PxD6Joint* PD6Joint = PxD6JointCreate(*GPhysXSDK, PActor2, U2PTransform(Local2), PActor1, U2PTransform(Local1));

	if (PD6Joint == nullptr)
	{
		UE_LOG(LogPhysics, Log, TEXT("URB_ConstraintInstance::InitConstraint - Invalid 6DOF joint (%s)"), *JointName.ToString());
		return false;
	}

	///////// POINTERS
	PD6Joint->userData = &PhysxUserData;

	// Remember reference to scene index.
	FPhysScene* RBScene = FPhysxUserData::Get<FPhysScene>(PScene->userData);
	if (RBScene->GetPhysXScene(PST_Sync) == PScene)
	{
		SceneIndex = RBScene->PhysXSceneIndex[PST_Sync];
	}
	else
	if (RBScene->GetPhysXScene(PST_Async) == PScene)
	{
		SceneIndex = RBScene->PhysXSceneIndex[PST_Async];
	}
	else
	{
		UE_LOG(LogPhysics, Log, TEXT("URB_ConstraintInstance::InitConstraint: PxScene has inconsistent FPhysScene userData.  No joint created."));
		return false;
	}

	ConstraintData = PD6Joint;
	return true;
}

void FConstraintInstance::UpdateConstraintFlags_AssumesLocked()
{
	PxConstraintFlags Flags = PxConstraintFlags();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Flags |= PxConstraintFlag::eVISUALIZATION;
#endif

	if (!bDisableCollision)
	{
		Flags |= PxConstraintFlag::eCOLLISION_ENABLED;
	}

	if (bEnableProjection)
	{
		Flags |= PxConstraintFlag::ePROJECTION;

		ConstraintData->setProjectionLinearTolerance(ProjectionLinearTolerance);
		ConstraintData->setProjectionAngularTolerance(FMath::DegreesToRadians(ProjectionAngularTolerance));
	}

	ConstraintData->setConstraintFlags(Flags);
}


void FConstraintInstance::UpdateAverageMass_AssumesLocked(const PxRigidActor* PActor1, const PxRigidActor* PActor2)
{
	AverageMass = ComputeAverageMass_AssumesLocked(PActor1, PActor2);
}

void EnsureSleepingActorsStaySleeping_AssumesLocked(PxRigidActor* PActor1, PxRigidActor* PActor2)
{
	// record if actors are asleep before creating joint, so we can sleep them afterwards if so (creating joint wakes them)
	const bool bActor1Asleep = (PActor1 == nullptr || !PActor1->isRigidDynamic() || PActor1->isRigidDynamic()->isSleeping());
	const bool bActor2Asleep = (PActor2 == nullptr || !PActor2->isRigidDynamic() || PActor2->isRigidDynamic()->isSleeping());

	// creation of joints wakes up rigid bodies, so we put them to sleep again if both were initially asleep
	if (bActor1Asleep && bActor2Asleep)
	{
		if (PActor1 && IsRigidBodyNonKinematic_AssumesLocked(PActor1->isRigidDynamic()))
		{
			PActor1->isRigidDynamic()->putToSleep();
		}

		if (PActor2 && IsRigidBodyNonKinematic_AssumesLocked(PActor2->isRigidDynamic()))
		{
			PActor2->isRigidDynamic()->putToSleep();
		}
	}
}

#endif

/** 
 *	Create physics engine constraint.
 */
void FConstraintInstance::InitConstraint(USceneComponent* Owner, FBodyInstance* Body1, FBodyInstance* Body2, float Scale)
{
	OwnerComponent = Owner;

#if WITH_PHYSX
	PhysxUserData = FPhysxUserData(this);

	// if there's already a constraint, get rid of it first
	if (ConstraintData)
	{
		TermConstraint();
	}

	PxRigidActor* PActor1 = nullptr;
	PxRigidActor* PActor2 = nullptr;
	PxScene* PScene = GetPScene_LockFree(Body1, Body2);
	SCOPED_SCENE_WRITE_LOCK(PScene);

	const bool bValidConstraintSetup = PScene && GetPActors_AssumesLocked(Body1, Body2, &PActor1, &PActor2) && CreatePxJoint_AssumesLocked(PActor1, PActor2, PScene, Scale);
	if (!bValidConstraintSetup)
	{
		return;
	}


	// update mass
	UpdateAverageMass_AssumesLocked(PActor1, PActor2);
	
	//flags and projection settings
	UpdateConstraintFlags_AssumesLocked();
	
	//limits
	UpdateAngularLimit();
	UpdateLinearLimit();

	//breakable
	UpdateBreakable();
	
	//motors
	SetLinearDriveParams(LinearDriveSpring, LinearDriveDamping, LinearDriveForceLimit);
	SetAngularDriveParams(AngularDriveSpring, AngularDriveDamping, AngularDriveForceLimit);
	UpdateDriveTarget();
	
	EnsureSleepingActorsStaySleeping_AssumesLocked(PActor1, PActor2);
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
	if(PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex))
	{
		SCOPED_SCENE_WRITE_LOCK(PScene);
		ConstraintData->release();
	}

	ConstraintData = nullptr;
#endif
}

bool FConstraintInstance::IsTerminated() const
{
#if WITH_PHYSX
	return (ConstraintData == nullptr);
#else 
	return true;
#endif //WITH_PHYSX
}

bool FConstraintInstance::IsValidConstraintInstance() const
{
#if WITH_PHYSX
	return ConstraintData != nullptr;
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
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxTransform PxRefFrame = U2PTransform(RefFrame);
		Joint->setLocalPose(PxFrame, PxRefFrame);
	});
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
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxTransform PxRefFrame = ConstraintData->getLocalPose(PxFrame);
		PxRefFrame.p = U2PVector(RefPosition);
		Joint->setLocalPose(PxFrame, PxRefFrame);
	});
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
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		FTransform URefTransform = FTransform(PriAxis1, SecAxis, PriAxis ^ SecAxis, RefPos);
		PxTransform PxRefFrame = U2PTransform(URefTransform);
		Joint->setLocalPose(PxFrame, PxRefFrame);
	});
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
	OutLinearForce = FVector::ZeroVector;
	OutAngularForce = FVector::ZeroVector;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadOnly([&] (const PxD6Joint* Joint)
	{
		PxVec3 PxOutLinearForce;
		PxVec3 PxOutAngularForce;
		Joint->getConstraint()->getForce(PxOutLinearForce, PxOutAngularForce);

		OutLinearForce = P2UVector(PxOutLinearForce);
		OutAngularForce = P2UVector(PxOutAngularForce);
	});
#endif
}



/** Function for turning linear position drive on and off. */
void FConstraintInstance::SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
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
	});
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
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		// Get the current drives
		PxD6JointDrive CurrentDriveSwing = Joint->getDrive(PxD6Drive::eSWING);
		PxD6JointDrive CurrentDriveTwist = Joint->getDrive(PxD6Drive::eTWIST);
		PxD6JointDrive CurrentDriveSlerp = Joint->getDrive(PxD6Drive::eSLERP);
		const bool bSlerp = AngularDriveMode == EAngularDriveMode::SLERP;

		CurrentDriveSwing.stiffness = !bSlerp && bEnableSwingDrive ? AngularDriveSpring : 0.0f;
		CurrentDriveTwist.stiffness = !bSlerp && bEnableTwistDrive ? AngularDriveSpring : 0.0f;
		CurrentDriveSlerp.stiffness = bSlerp  && (bEnableSwingDrive || bEnableTwistDrive) ? AngularDriveSpring : 0.0f;

		Joint->setDrive(PxD6Drive::eSWING, CurrentDriveSwing);
		Joint->setDrive(PxD6Drive::eTWIST, CurrentDriveTwist);
		Joint->setDrive(PxD6Drive::eSLERP, CurrentDriveSlerp);
		
		bAngularOrientationDrive = bEnableSwingDrive || bEnableTwistDrive;
	});
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
		const bool bSlerp = AngularDriveMode == EAngularDriveMode::SLERP;

		CurrentDriveSwing.damping = !bSlerp && bEnableSwingDrive ? AngularDriveDamping : 0.0f;
		CurrentDriveTwist.damping = !bSlerp && bEnableTwistDrive ? AngularDriveDamping : 0.0f;
		CurrentDriveSlerp.damping = bSlerp  && (bEnableSwingDrive && bEnableTwistDrive) ? AngularDriveDamping : 0.0f;

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
	if (PxD6Joint* Joint = ConstraintData)
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
	if (PxD6Joint* Joint = ConstraintData)
	{
		PxVec3 CurrentLinearVel, CurrentAngVel;
		Joint->getDriveVelocity(CurrentLinearVel, CurrentAngVel);

		PxVec3 NewLinearVel = U2PVector(InVelTarget);
		Joint->setDriveVelocity(NewLinearVel, CurrentAngVel);
	}
#endif
	LinearVelocityTarget = InVelTarget;
}

/** Function for setting linear motor parameters. */
void FConstraintInstance::SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit)
{
#if WITH_PHYSX
	if (bLinearPositionDrive || bLinearVelocityDrive)
	{
		ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
		{
			// X-Axis linear drive
			const float DriveSpringX = bLinearPositionDrive && bLinearXPositionDrive ? InSpring : 0.0f;
			const float DriveDampingX = (bLinearVelocityDrive && LinearVelocityTarget.X != 0.f) ? InDamping : 0.0f;
			const float LinearForceLimit = InForceLimit > 0.f ? InForceLimit : PX_MAX_F32;
			Joint->setDrive(PxD6Drive::eX, PxD6JointDrive(DriveSpringX, DriveDampingX, LinearForceLimit, bIsAccelerationDrive));

			// Y-Axis linear drive
			const float DriveSpringY = bLinearPositionDrive && bLinearYPositionDrive ? InSpring : 0.0f;
			const float DriveDampingY = (bLinearVelocityDrive && LinearVelocityTarget.Y != 0.f) ? InDamping : 0.0f;
			Joint->setDrive(PxD6Drive::eY, PxD6JointDrive(DriveSpringY, DriveDampingY, LinearForceLimit, bIsAccelerationDrive));

			// Z-Axis linear drive
			const float DriveSpringZ = bLinearPositionDrive && bLinearZPositionDrive ? InSpring : 0.0f;
			const float DriveDampingZ = (bLinearVelocityDrive && LinearVelocityTarget.Z != 0.f) ? InDamping : 0.0f;
			Joint->setDrive(PxD6Drive::eZ, PxD6JointDrive(DriveSpringZ, DriveDampingZ, LinearForceLimit, bIsAccelerationDrive));
		});
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
	if (PxD6Joint* Joint = ConstraintData)
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
	if (PxD6Joint* Joint = ConstraintData)
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
	if (PxD6Joint* Joint = ConstraintData)
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
	if (PxD6Joint* Joint = ConstraintData)
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
	if (PxD6Joint* Joint = ConstraintData)
	{
		PxVec3 CurrentLinearVel, CurrentAngVel;
		Joint->getDriveVelocity(CurrentLinearVel, CurrentAngVel);

		PxVec3 AngVel = U2PVector(RevolutionsToRads(InVelTarget));
		Joint->setDriveVelocity(CurrentLinearVel, AngVel);
	}
#endif

	AngularVelocityTarget = InVelTarget;
}

/** Function for setting angular motor parameters. */
void FConstraintInstance::SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit)
{
#if WITH_PHYSX
	if (bAngularOrientationDrive || bAngularVelocityDrive)
	{
		ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
		{
			const float AngularForceLimit = InForceLimit > 0.0f ? InForceLimit : PX_MAX_F32;
			const float DriveSpring = bAngularOrientationDrive ? InSpring : 0.0f;
			const float DriveDamping = bAngularVelocityDrive ? InDamping : 0.0f;

			if (AngularDriveMode == EAngularDriveMode::SLERP)
			{
				Joint->setDrive(PxD6Drive::eTWIST, PxD6JointDrive());
				Joint->setDrive(PxD6Drive::eSWING, PxD6JointDrive());
				Joint->setDrive(PxD6Drive::eSLERP, PxD6JointDrive(DriveSpring, DriveDamping, AngularForceLimit, bIsAccelerationDrive));
			}
			else
			{
				Joint->setDrive(PxD6Drive::eTWIST, PxD6JointDrive(DriveSpring, DriveDamping, AngularForceLimit, bIsAccelerationDrive));
				Joint->setDrive(PxD6Drive::eSWING, PxD6JointDrive(DriveSpring, DriveDamping, AngularForceLimit, bIsAccelerationDrive));
				Joint->setDrive(PxD6Drive::eSLERP, PxD6JointDrive());
			}
		});
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
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
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
		
	});
#endif
}

/** Allows you to dynamically change the size of the linear limit 'sphere'. */
void FConstraintInstance::SetLinearLimitSize(float NewLimitSize)
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxReal LimitContractDistance =  1.f * (PI/180.0f);
		Joint->setLinearLimit(PxJointLinearLimit(GPhysXSDK->getTolerancesScale(), NewLimitSize, LimitContractDistance * GPhysXSDK->getTolerancesScale().length)); // LOC_MOD33 need to scale the contactDistance if not using its default value
	});
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

void FConstraintInstance::EnableProjection()
{
	SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());
	ConstraintData->setProjectionLinearTolerance(ProjectionLinearTolerance);
	ConstraintData->setProjectionAngularTolerance(ProjectionAngularTolerance);
	ConstraintData->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);
}

void FConstraintInstance::DisableProjection()
{
	SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());
	ConstraintData->setConstraintFlag(PxConstraintFlag::ePROJECTION, false);
}

#undef LOCTEXT_NAMESPACE