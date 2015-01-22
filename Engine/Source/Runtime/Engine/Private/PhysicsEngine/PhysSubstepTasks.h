// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//This is only here for now while we transition into substepping
#if WITH_PHYSX
#include "pxtask/PxTask.h"
class PhysXCompletionTask : public PxLightCpuTask
{
	FGraphEventRef& EventToFire;
public:
	PhysXCompletionTask(FGraphEventRef& InEventToFire, PxTaskManager* TaskManager)
		: EventToFire(InEventToFire)
	{
		setContinuation(*TaskManager, NULL);
	}
	virtual void run()
	{
	}
	virtual void release()
	{
		PxLightCpuTask::release();
		EventToFire->DispatchSubsequents();
		delete this;
	}
	virtual const char *getName() const
	{
		return "CompleteSimulate";
	}
};
#endif	//#if WITH_PHYSX


#if WITH_SUBSTEPPING


/** Hold information about kinematic target */
struct FKinematicTarget
{
	FKinematicTarget() : BodyInstance(0) {}
	FKinematicTarget(FBodyInstance* Body, const FTransform& TM) :
		BodyInstance(Body),
		TargetTM(TM),
		OriginalTM(Body->GetUnrealWorldTransform())
	{
	}

	/** Kinematic actor we are setting target for*/
	FBodyInstance* BodyInstance;

	/** Target transform for kinematic actor*/
	FTransform TargetTM;

	/** Start transform for kinematic actor*/
	FTransform OriginalTM;
};

/** Holds information about requested force */
struct FForceTarget
{
	FForceTarget(){}
	FForceTarget(const FVector& GivenForce) : Force(GivenForce), bPosition(false){}
	FForceTarget(const FVector& GivenForce, const FVector& GivenPosition) : Force(GivenForce), Position(GivenPosition), bPosition(true){}
	FVector Force;
	FVector Position;
	bool bPosition;
};

struct FTorqueTarget
{
	FTorqueTarget(){}
	FTorqueTarget(const FVector& GivenTorque) : Torque(GivenTorque){}
	FVector Torque;
};

struct FCustomTarget
{
	FCustomTarget(){ }
	FCustomTarget(const FCalculateCustomPhysics& GivenCalculateCustomPhysics) : CalculateCustomPhysics(&GivenCalculateCustomPhysics){}
	const FCalculateCustomPhysics* CalculateCustomPhysics;
};

/** Holds information on everything we need to fix for substepping of a single frame */
struct FPhysTarget
{
	FPhysTarget() : bKinematicTarget(false){}

	TArray<FForceTarget>  Forces;	//we can apply force at multiple places
	TArray<FTorqueTarget> Torques;
	TArray<FCustomTarget> CustomPhysics; //for calculating custom physics forces
	FKinematicTarget KinematicTarget;

	/** Tells us if the kinematic target has been set */
	bool bKinematicTarget;
	
	
};

#if WITH_PHYSX
#if WITH_APEX
typedef NxApexScene PxApexScene;	//helper typedef so we don't have to use as many ifdefs
#else
typedef PxScene PxApexScene;
#endif
#endif

/** Holds information used for substepping a scene */
class FPhysSubstepTask
{
public:
#if WITH_PHYSX
	FPhysSubstepTask(PxApexScene * GivenScene);
#endif

	void SetKinematicTarget(FBodyInstance* Body, const FTransform& TM);
	bool GetKinematicTarget(const FBodyInstance* Body, FTransform& OutTM) const;
	void AddCustomPhysics(FBodyInstance* Body, const FCalculateCustomPhysics& CalculateCustomPhysics);
	void AddForce(FBodyInstance* Body, const FVector& Force);
	void AddForceAtPosition(FBodyInstance* Body, const FVector& Force, const FVector& Position);
	void AddTorque(FBodyInstance* Body, const FVector& Torque);

	/** Removes a BodyInstance from doing substep work - should only be called when the FBodyInstance is getting destroyed */
	void RemoveBodyInstance(FBodyInstance* Body);

	
	void SwapBuffers();
	float UpdateTime(float UseDelta);

	void SetVehicleManager(class FPhysXVehicleManager *	InVehicleManager);
	void SubstepSimulationStart();
	void SubstepSimulationEnd(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
#if WITH_PHYSX
	void StepSimulation(PhysXCompletionTask * Task);
#endif
		
private:

	/** Applies interpolation and forces on all needed actors*/
	void SubstepInterpolation(float Scale, float DeltaTime);
	void ApplyCustomPhysics(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance, float DeltaTime);
	void ApplyForces(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance);
	void ApplyTorques(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance);
	void InterpolateKinematicActor(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance, float Alpha);

	typedef TMap<FBodyInstance*, FPhysTarget> PhysTargetMap;
	PhysTargetMap PhysTargetBuffers[2];	//need to double buffer between physics thread and game thread
	uint32 NumSubsteps;
	float SubTime;
	float DeltaSeconds;
	bool External;
	class  PhysXCompletionTask * FullSimulationTask;
	float Alpha;
	float StepScale;
	float TotalSubTime;
	uint32 CurrentSubStep;
	FGraphEventRef CompletionEvent;

	/** Vehicle scene */
	class FPhysXVehicleManager*			VehicleManager;

#if WITH_PHYSX
	PxApexScene * PAScene;
#endif

};

#endif //if WITH_SUBSTEPPING