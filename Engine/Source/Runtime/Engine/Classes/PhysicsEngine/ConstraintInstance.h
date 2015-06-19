// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ConstraintInstance.generated.h"

#if WITH_PHYSX
namespace physx
{
	class PxD6Joint;
	class PxRigidActor;
	class PxScene;
}
#endif // WITH_PHYSX

// LINEAR DOF
UENUM()
enum ELinearConstraintMotion
{
	/** No constraint against this axis */ 
	LCM_Free	UMETA(DisplayName="Free"),
	/** Limited freedom along this axis */ 
	LCM_Limited UMETA(DisplayName="Limited"),
	/** Fully constraint against this axis */
	LCM_Locked UMETA(DisplayName="Locked"),

	LCM_MAX,
};

/** Enum to indicate which frame we want */
UENUM()
namespace EConstraintFrame
{
	enum Type
	{
		Frame1,
		Frame2
	};
}

UENUM()
namespace EAngularDriveMode
{
	enum Type
	{
		//Follows the shortest arc between a pair of anuglar configurations (Ignored if any angular limits/locks are used)
		SLERP,
		//Path is decomposed into twist and swing. Doesn't follow shortest arc and may have gimbal lock. (Works with angular limits/locks)
		TwistAndSwing
	};
}

/** Container for a physics representation of an object */
USTRUCT()
struct ENGINE_API FConstraintInstance
{
	GENERATED_USTRUCT_BODY()

	///////////////////////////// INSTANCE DATA

	/**
	 *	Indicates position of this constraint within the array in SkeletalMeshComponent
	 */
	int32 ConstraintIndex;

	/** The component that created this instance */
	UPROPERTY()
	USceneComponent* OwnerComponent;

#if WITH_PHYSX
	/** Internal use. Physics-engine representation of this constraint. */
	physx::PxD6Joint*		ConstraintData;
#endif	//WITH_PHYSX

	/** Physics scene index. */
	int32 SceneIndex;

	/** Name of bone that this joint is associated with. */
	UPROPERTY(VisibleAnywhere, Category=Constraint)
	FName JointName;

	///////////////////////////// CONSTRAINT GEOMETRY
	
	/** 
	 *	Name of first bone (body) that this constraint is connecting. 
	 *	This will be the 'child' bone in a PhysicsAsset.
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FName ConstraintBone1;

	/** 
	 *	Name of second bone (body) that this constraint is connecting. 
	 *	This will be the 'parent' bone in a PhysicsAset.
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FName ConstraintBone2;

	///////////////////////////// Body1 ref frame
	
	/** Location of constraint in Body1 reference frame. */
	UPROPERTY()
	FVector Pos1;

	/** Primary (twist) axis in Body1 reference frame. */
	UPROPERTY()
	FVector PriAxis1;

	/** Seconday axis in Body1 reference frame. Orthogonal to PriAxis1. */
	UPROPERTY()
	FVector SecAxis1;

	///////////////////////////// Body2 ref frame
	
	/** Location of constraint in Body2 reference frame. */
	UPROPERTY()
	FVector Pos2;

	/** Primary (twist) axis in Body2 reference frame. */
	UPROPERTY()
	FVector PriAxis2;

	/** Seconday axis in Body2 reference frame. Orthogonal to PriAxis2. */
	UPROPERTY()
	FVector SecAxis2;

	// Disable collision between bodies joined by this constraint.
	UPROPERTY(EditAnywhere, Category=Constraint)
	uint32 bDisableCollision:1;

	/** 
	 * If distance error between bodies exceeds 0.1 units, or rotation error exceeds 10 degrees, body will be projected to fix this.
	 * For example a chain spinning too fast will have its elements appear detached due to velocity, this will project all bodies so they still appear attached to each other. 
	 */
	UPROPERTY(EditAnywhere, Category=Projection)
	uint32 bEnableProjection:1;

	/** Linear tolerance value in world units. If the distance error exceeds this tolarance limit, the body will be projected. */
	UPROPERTY(EditAnywhere, Category=Projection, meta=(editcondition = "bEnableProjection", ClampMin = "0.0"))
	float ProjectionLinearTolerance;

	/** Angular tolerance value in world units. If the distance error exceeds this tolarance limit, the body will be projected. */
	UPROPERTY(EditAnywhere, Category=Projection, meta=(editcondition = "bEnableProjection", ClampMin = "0.0"))
	float ProjectionAngularTolerance;

	/** Indicates whether linear motion along the x axis is allowed, blocked or limited. If limited, the LinearLimit property will be used
		to determine if a motion is allowed. See ELinearConstraintMotion. */
	UPROPERTY(EditAnywhere, Category=Linear)
	TEnumAsByte<enum ELinearConstraintMotion> LinearXMotion;

	/** Indicates whether linear motion along the y axis is allowed, blocked or limited. If limited, the LinearLimit property will be used
		to determine if a motion is allowed. See ELinearConstraintMotion. */
	UPROPERTY(EditAnywhere, Category=Linear)
	TEnumAsByte<enum ELinearConstraintMotion> LinearYMotion;

	/** Indicates whether linear motion along the z axis is allowed, blocked or limited. If limited, the LinearLimit property will be used
		to determine if a motion is allowed. See ELinearConstraintMotion. */
	UPROPERTY(EditAnywhere, Category=Linear)
	TEnumAsByte<enum ELinearConstraintMotion> LinearZMotion;

	/** The limiting extent in world untis of the linear motion for limitied motion axes. */
	UPROPERTY(EditAnywhere, Category=Linear, meta=(ClampMin = "0.0"))
	float LinearLimitSize;

	/** Whether we want to use soft limits instead of hard limits. With enabled soft limit, a constraint is used 
		instead of hard-capping the motion. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Linear)
	uint32 bLinearLimitSoft:1;

	/** Stiffness of the linear soft limit constraint. Only used, when bLinearLimitSoft is true. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Linear, meta=(editcondition = "bLinearLimitSoft", ClampMin = "0.0"))
	float LinearLimitStiffness;
	
	/** Damping of the linear soft limit constraint. Only used, when bLinearLimitSoft is true. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Linear, meta=(editcondition = "bLinearLimitSoft", ClampMin = "0.0"))
	float LinearLimitDamping;

	/** Defines whether the joint is breakable or not. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Linear)
	uint32 bLinearBreakable:1;

	/** Force needed to break the joint. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Linear, meta=(editcondition = "bLinearBreakable", ClampMin = "0.0"))
	float LinearBreakThreshold;

	/** Indicates whether rotation about the Z axis is allowed, blocked, or limited. If limited, the 
		AngularLimit property will be used to determine the range of motion. See EAngularConstraintMotion. */
	UPROPERTY(EditAnywhere, Category=Angular)
	TEnumAsByte<enum EAngularConstraintMotion> AngularSwing1Motion;
		
	/** Indicates whether rotation about the the X axis is allowed, blocked, or limited. If limited, the
		AngularLimit property will be used to determine the range of motion. See EAngularConstraintMotion. */
	UPROPERTY(EditAnywhere, Category=Angular)
	TEnumAsByte<enum EAngularConstraintMotion> AngularTwistMotion;

	/** Indicates whether rotation about the Y axis is allowed, blocked, or limited. If limited, the
		AngularLimit property will be used to determine the range of motion. See EAngularConstraintMotion. */
	UPROPERTY(EditAnywhere, Category = Angular)
	TEnumAsByte<enum EAngularConstraintMotion> AngularSwing2Motion;


	/** Whether we want to use soft limits for swing motions instead of hard limits. With enabled 
		soft limit, a constraint is used instead of hard-capping the motion. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular)
	uint32 bSwingLimitSoft:1;

	/** Whether we want to use soft limits for twist motions instead of hard limits. With enabled 
		soft limit, a constraint is used instead of hard-capping the motion. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular)
	uint32 bTwistLimitSoft:1;

	/** Used if swing motion along the y axis is limited. The limit angle is specified in degrees and should be
		between 0 and 180. */
	UPROPERTY(EditAnywhere, Category=Angular, meta=(ClampMin = "0.0", ClampMax = "180.0"))
	float Swing1LimitAngle;
	
	/** Used if twist motion along the x axis is limited. The limit angle is specified in degrees and should be
		between 0 and 180. */
	UPROPERTY(EditAnywhere, Category=Angular, meta=(ClampMin = "0.0", ClampMax = "180.0"))
	float TwistLimitAngle;

	/** Used if swing motion along the z axis is limited. The limit angle is specified in degrees and should be
	between 0 and 180. */
	UPROPERTY(EditAnywhere, Category = Angular, meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float Swing2LimitAngle;

	/** Stiffness of the swing limit constraint if soft limit is used for swing motions. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular, meta=(editcondition = "bSwingLimitSoft", ClampMin = "0.0"))
	float SwingLimitStiffness;

	/** Damping of the swing limit constraint if soft limit is used for swing motions. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular, meta=(editcondition = "bSwingLimitSoft", ClampMin = "0.0"))
	float SwingLimitDamping;

	/** Stiffness of the twist limit constraint if soft limit is used for twist motions. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular, meta=(editcondition = "bTwistLimitSoft", ClampMin = "0.0"))
	float TwistLimitStiffness;

	/** Damping of the twist limit constraint if soft limit is used for twist motions. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular, meta=(editcondition = "bTwistLimitSoft", ClampMin = "0.0"))
	float TwistLimitDamping;

	/** Specifies the angular offset between the two frames of reference. By default limit goes from (-Angle, +Angle)
	  * This allows you to bias the limit for swing1 swing2 and twist. */
	UPROPERTY(EditAnywhere, Category = Angular)
	FRotator AngularRotationOffset;
	
	/** Whether it is possible to break the joint with angular force. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular)
	uint32 bAngularBreakable:1;

	/** Angular force needed to break the joint. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Angular, meta=(editcondition = "bAngularBreakable", ClampMin = "0.0"))
	float AngularBreakThreshold;

	/** Enables/Disables linear drive along the x axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	uint32 bLinearXPositionDrive:1;

	UPROPERTY()
	uint32 bLinearXVelocityDrive_DEPRECATED:1;

	/** Enables/Disables linear drive along the y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	uint32 bLinearYPositionDrive:1;

	UPROPERTY()
	uint32 bLinearYVelocityDrive_DEPRECATED:1;

	/** Enables/Disables linear drive along the z axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	uint32 bLinearZPositionDrive:1;

	UPROPERTY()
	uint32 bLinearZVelocityDrive_DEPRECATED:1;

	/** Enables/Disables the linear position drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	uint32 bLinearPositionDrive:1;
	
	/** Enables/Disables the linear velocity drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	uint32 bLinearVelocityDrive:1;

	/** Target position the linear drive. Only the components that are enabled are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	FVector LinearPositionTarget;

	/** Target velocity the linear drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	FVector LinearVelocityTarget;

	/** Spring to apply to the for linear drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LinearMotor, meta = (DisplayName = "Linear Position Strength", editcondition = "bLinearPositionDrive"))
	float LinearDriveSpring;

	/** Damping to apply to the for linear drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LinearMotor, meta = (DisplayName = "Linear Velocity Strength", editcondition = "bLinearVelocityDrive"))
	float LinearDriveDamping;

	/** Limit to the force the linear drive can apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LinearMotor)
	float LinearDriveForceLimit;

	UPROPERTY()
	uint32 bSwingPositionDrive_DEPRECATED:1;

	UPROPERTY()
	uint32 bSwingVelocityDrive_DEPRECATED:1;

	UPROPERTY()
	uint32 bTwistPositionDrive_DEPRECATED:1;

	UPROPERTY()
	uint32 bTwistVelocityDrive_DEPRECATED:1;

	UPROPERTY()
	uint32 bAngularSlerpDrive_DEPRECATED:1;

	/** Enables the angular drive towards a target orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AngularMotor)
	uint32 bAngularOrientationDrive:1;
	
	/** Enables the angular drive towards a target velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AngularMotor)
	uint32 bAngularVelocityDrive:1;

	UPROPERTY()
	FQuat AngularPositionTarget_DEPRECATED;
	
	/** The way rotation paths are estimated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AngularMotor)
	TEnumAsByte<enum EAngularDriveMode::Type> AngularDriveMode;

	/** Target orientation for the angular drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AngularMotor)
	FRotator AngularOrientationTarget;

	/** Target velocity for the angular drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AngularMotor)
	FVector AngularVelocityTarget;    // Revolutions per second

	/** Spring value to apply to the for angular drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AngularMotor, meta = (DisplayName = "Angular Position Strength", editcondition = "bAngularOrientationDrive"))
	float AngularDriveSpring;

	/** Damping value to apply to the for angular drive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AngularMotor, meta = (DisplayName = "Angular Velocity Strength"))
	float AngularDriveDamping;

	/** Limit to the force the angular drive can apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AngularMotor)
	float AngularDriveForceLimit;

	float AverageMass;


	/** Sets the LinearX Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetLinearXLimit(ELinearConstraintMotion ConstraintType, float LinearLimitSize);

	/** Sets the LinearY Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetLinearYLimit(ELinearConstraintMotion ConstraintType, float LinearLimitSize);

	/** Sets the LinearZ Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetLinearZLimit(ELinearConstraintMotion ConstraintType, float LinearLimitSize);

	/** Sets the Angular Swing1 Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetAngularSwing1Limit(EAngularConstraintMotion MotionType, float Swing1LimitAngle);

	/** Sets the Angular Swing2 Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetAngularSwing2Limit(EAngularConstraintMotion MotionType, float Swing2LimitAngle);

	/** Sets the Angular Twist Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetAngularTwistLimit(EAngularConstraintMotion MotionType, float TwistLimitAngle);

#if WITH_PHYSX
	FPhysxUserData PhysxUserData;
#endif


public:


	/** Constructor **/
	FConstraintInstance();

	/** Create physics engine constraint. */
	void InitConstraint(USceneComponent* Owner, FBodyInstance* Body1, FBodyInstance* Body2, float Scale);

	/** Terminate physics engine constraint */
	void TermConstraint();

	bool IsTerminated() const;

	/** See if this constraint is valid. */
	bool IsValidConstraintInstance() const;

	// Pass in reference frame in. If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint. 
	void SetRefFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame);

	// Pass in reference position in (maintains reference orientation). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	void SetRefPosition(EConstraintFrame::Type Frame, const FVector& RefPosition);
	
	// Pass in reference orientation in (maintains reference position). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	void SetRefOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis);
	
	// The current twist of the constraint
	float GetCurrentTwist() const;

	// The current swing1 of the constraint
	float GetCurrentSwing1() const;

	// The current swing2 of the constraint
	float GetCurrentSwing2() const;

	// Get component ref frame
	FTransform GetRefFrame(EConstraintFrame::Type Frame) const;

	// @todo document
	void CopyConstraintGeometryFrom(const FConstraintInstance* FromInstance);

	// @todo document
	void CopyConstraintParamsFrom(const FConstraintInstance* FromInstance);

	/** Get the position of this constraint in world space. */
	FVector GetConstraintLocation();

	// Retrieve the constraint force most recently applied to maintain this constraint. Returns 0 forces if the constraint is not initialized or broken.
	void GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce);

	void SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);
	void SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);
	void SetAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);
	void SetAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive);

	void SetLinearPositionTarget(const FVector& InPosTarget);
	void SetLinearVelocityTarget(const FVector& InVelTarget);
	void SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit);

	void SetAngularOrientationTarget(const FQuat& InPosTarget);
	void SetAngularVelocityTarget(const FVector& InVelTarget);
	void SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit);

	void SetDisableCollision(bool InDisableCollision);

	void UpdateLinearLimit();
	void UpdateAngularLimit();


	/** Scale Angular Limit Constraints (as defined in RB_ConstraintSetup) */
	void	SetAngularDOFLimitScale(float InSwing1LimitScale, float InSwing2LimitScale, float InTwistLimitScale);

	/** Allows you to dynamically change the size of the linear limit 'sphere'. */
	void	SetLinearLimitSize(float NewLimitSize);

	// @todo document
	void DrawConstraint(class FPrimitiveDrawInterface* PDI, 
		float Scale, float LimitDrawScale, bool bDrawLimits, bool bDrawSelected,
		const FTransform& Con1Frame, const FTransform& Con2Frame, bool bDrawAsPoint) const;

	void ConfigureAsHinge(bool bOverwriteLimits = true);

	void ConfigureAsPrismatic(bool bOverwriteLimits = true);

	void ConfigureAsSkelJoint(bool bOverwriteLimits = true);

	void ConfigureAsBS(bool bOverwriteLimits = true);

	bool IsHinge() const;

	bool IsPrismatic() const;

	bool IsSkelJoint() const;

	bool IsBallAndSocket() const;

	void PostSerialize(const FArchive& Ar);

	void OnConstraintBroken();

	void EnableProjection();

	void DisableProjection();

	//Hacks to easily get zeroed memory for special case when we don't use GC
	static void Free(FConstraintInstance * Ptr);
	static FConstraintInstance * Alloc();

private:
#if WITH_PHYSX 
	bool CreatePxJoint_AssumesLocked(physx::PxRigidActor* PActor1, physx::PxRigidActor* PActor2, physx::PxScene* PScene, const float Scale);
	void UpdateConstraintFlags_AssumesLocked();
	void UpdateAverageMass_AssumesLocked(const physx::PxRigidActor* PActor1, const physx::PxRigidActor* PActor2);
	physx::PxD6Joint* GetUnbrokenJoint_AssumesLocked() const;

	bool ExecuteOnUnbrokenJointReadOnly(TFunctionRef<void(const physx::PxD6Joint*)> Func) const;
	bool ExecuteOnUnbrokenJointReadWrite(TFunctionRef<void(physx::PxD6Joint*)> Func) const;
#endif

	void UpdateBreakable();
	void UpdateDriveTarget();
};

template<>
struct TStructOpsTypeTraits<FConstraintInstance> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithPostSerialize = true,
	};
};
