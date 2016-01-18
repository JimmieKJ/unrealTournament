// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsPublic.h"
#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif // WITH_PHYSX

UPhysicsHandleComponent::UPhysicsHandleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	LinearDamping = 200.0f;
	LinearStiffness = 750.0f;
	AngularDamping = 500.0f;
	AngularStiffness = 1500.0f;
	InterpolationSpeed = 50.f;
}

void UPhysicsHandleComponent::OnUnregister()
{
	if(GrabbedComponent)
	{
		ReleaseComponent();
	}

#if WITH_PHYSX
	if(HandleData)
	{
		check(KinActorData);

		// use correct scene
		PxScene* PScene = GetPhysXSceneFromIndex( SceneIndex );
		SCOPED_SCENE_WRITE_LOCK(PScene);
		if(PScene)
		{
			// destroy joint
			HandleData->release();
			HandleData = NULL;

			// Destroy temporary actor.
			KinActorData->release();
			KinActorData = NULL;
		}
	}
#endif // WITH_PHYSX

	Super::OnUnregister();
}


void UPhysicsHandleComponent::GrabComponent(UPrimitiveComponent* InComponent, FName InBoneName, FVector Location, bool bConstrainRotation)
{
	// If we are already holding something - drop it first.
	if(GrabbedComponent != NULL)
	{
		ReleaseComponent();
	}

	if(!InComponent)
	{
		return;
	}

#if WITH_PHYSX
	// Get the PxRigidDynamic that we want to grab.
	FBodyInstance* BodyInstance = InComponent->GetBodyInstance(InBoneName);
	if (!BodyInstance)
	{
		return;
	}

	ExecuteOnPxRigidDynamicReadWrite(BodyInstance, [&](PxRigidDynamic* Actor)
	{
		PxScene* Scene = Actor->getScene();
		
		// Get transform of actor we are grabbing
		PxVec3 KinLocation = U2PVector(Location);
		PxTransform GrabbedActorPose = Actor->getGlobalPose();
		PxTransform KinPose(KinLocation, GrabbedActorPose.q);

		// set target and current, so we don't need another "Tick" call to have it right
		TargetTransform = CurrentTransform = P2UTransform(KinPose);

		// If we don't already have a handle - make one now.
		if (!HandleData)
		{
			// Create kinematic actor we are going to create joint with. This will be moved around with calls to SetLocation/SetRotation.
			PxRigidDynamic* KinActor = Scene->getPhysics().createRigidDynamic(KinPose);
			KinActor->setRigidDynamicFlag(PxRigidDynamicFlag::eKINEMATIC, true);
			KinActor->setMass(1.0f);
			KinActor->setMassSpaceInertiaTensor(PxVec3(1.0f, 1.0f, 1.0f));

			// No bodyinstance
			KinActor->userData = NULL;

			// Add to Scene
			Scene->addActor(*KinActor);

			// Save reference to the kinematic actor.
			KinActorData = KinActor;

			// Create the joint
			PxVec3 LocalHandlePos = GrabbedActorPose.transformInv(KinLocation);
			PxD6Joint* NewJoint = PxD6JointCreate(Scene->getPhysics(), KinActor, PxTransform::createIdentity(), Actor, PxTransform(LocalHandlePos));

			if (!NewJoint)
			{
				HandleData = 0;
			}
			else
			{
				// No constraint instance
				NewJoint->userData = NULL;
				HandleData = NewJoint;

				// Remember the scene index that the handle joint/actor are in.
				FPhysScene* RBScene = FPhysxUserData::Get<FPhysScene>(Scene->userData);
				const uint32 SceneType = InComponent->BodyInstance.UseAsyncScene(RBScene) ? PST_Async : PST_Sync;
				SceneIndex = RBScene->PhysXSceneIndex[SceneType];

				// Setting up the joint
				NewJoint->setMotion(PxD6Axis::eX, PxD6Motion::eFREE);
				NewJoint->setMotion(PxD6Axis::eY, PxD6Motion::eFREE);
				NewJoint->setMotion(PxD6Axis::eZ, PxD6Motion::eFREE);
				NewJoint->setDrivePosition(PxTransform(PxVec3(0, 0, 0)));

				NewJoint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eFREE);
				NewJoint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eFREE);
				NewJoint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eFREE);

				bRotationConstrained = bConstrainRotation;

				UpdateDriveSettings();
			}

		}
	});
	
	
#endif // WITH_PHYSX


	GrabbedComponent = InComponent;
	GrabbedBoneName = InBoneName;
}

void UPhysicsHandleComponent::UpdateDriveSettings()
{
#if WITH_PHYSX
	if(HandleData != nullptr)
	{
		HandleData->setDrive(PxD6Drive::eX, PxD6JointDrive(LinearStiffness, LinearDamping, PX_MAX_F32, PxD6JointDriveFlag::eACCELERATION));
		HandleData->setDrive(PxD6Drive::eY, PxD6JointDrive(LinearStiffness, LinearDamping, PX_MAX_F32, PxD6JointDriveFlag::eACCELERATION));
		HandleData->setDrive(PxD6Drive::eZ, PxD6JointDrive(LinearStiffness, LinearDamping, PX_MAX_F32, PxD6JointDriveFlag::eACCELERATION));

		if (bRotationConstrained)
		{
			HandleData->setDrive(PxD6Drive::eSLERP, PxD6JointDrive(AngularStiffness, AngularDamping, PX_MAX_F32, PxD6JointDriveFlag::eACCELERATION));

			//NewJoint->setDrive(PxD6Drive::eTWIST, PxD6JointDrive(AngularStiffness, AngularDamping, PX_MAX_F32, PxD6JointDriveFlag::eACCELERATION));
			//NewJoint->setDrive(PxD6Drive::eSWING, PxD6JointDrive(AngularStiffness, AngularDamping, PX_MAX_F32, PxD6JointDriveFlag::eACCELERATION));
		}
	}
#endif // WITH_PHYSX
}

void UPhysicsHandleComponent::ReleaseComponent()
{
#if WITH_PHYSX
	if(GrabbedComponent)
	{
		if(HandleData)
		{
			check(KinActorData);

			// use correct scene
			PxScene* PScene = GetPhysXSceneFromIndex( SceneIndex );
			if(PScene)
			{
				SCOPED_SCENE_WRITE_LOCK(PScene);
				// Destroy joint.
				HandleData->release();
				
				// Destroy temporary actor.
				KinActorData->release();
				
			}
			KinActorData = NULL;
			HandleData = NULL;
		}

		bRotationConstrained = false;

		GrabbedComponent = NULL;
		GrabbedBoneName = NAME_None;
	}
#endif // WITH_PHYSX
}




void UPhysicsHandleComponent::SetTargetLocation(FVector NewLocation)
{
	TargetTransform.SetTranslation(NewLocation);
}

void UPhysicsHandleComponent::SetTargetRotation(FRotator NewRotation)
{
	TargetTransform.SetRotation(NewRotation.Quaternion());
}

void UPhysicsHandleComponent::SetTargetLocationAndRotation(FVector NewLocation, FRotator NewRotation)
{
	TargetTransform = FTransform(NewRotation, NewLocation);
}


void UPhysicsHandleComponent::UpdateHandleTransform(const FTransform& NewTransform)
{
	if(!KinActorData)
	{
		return;
	}

#if WITH_PHYSX
	bool bChangedPosition = true;
	bool bChangedRotation = true;

	PxRigidDynamic* KinActor = KinActorData;
	PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
	SCOPED_SCENE_WRITE_LOCK(PScene);

	// Check if the new location is worthy of change
	PxVec3 PNewLocation = U2PVector(NewTransform.GetTranslation());
	PxVec3 PCurrentLocation = KinActor->getGlobalPose().p;
	if((PNewLocation - PCurrentLocation).magnitudeSquared() <= 0.01f*0.01f)
	{
		PNewLocation = PCurrentLocation;
		bChangedPosition = false;
	}

	// Check if the new rotation is worthy of change
	PxQuat PNewOrientation = U2PQuat(NewTransform.GetRotation());
	PxQuat PCurrentOrientation = KinActor->getGlobalPose().q;
	if((FMath::Abs(PNewOrientation.dot(PCurrentOrientation)) > (1.f - SMALL_NUMBER)))
	{
		PNewOrientation = PCurrentOrientation;
		bChangedRotation = false;
	}

	// Don't call moveKinematic if it hasn't changed - that will stop bodies from going to sleep.
	if (bChangedPosition || bChangedRotation)
	{
		KinActor->setKinematicTarget(PxTransform(PNewLocation, PNewOrientation));

		//LOC_MOD
		//PxD6Joint* Joint = (PxD6Joint*) HandleData;
		//if(Joint)// && (PNewLocation - PCurrentLocation).magnitudeSquared() > 0.01f*0.01f)
		//{
		//	PxRigidActor* Actor0, *Actor1;
		//	Joint->getActors(Actor0, Actor1);
		//	//Joint->setDrivePosition(PxTransform(Actor0->getGlobalPose().transformInv(PNewLocation)));
		//	Joint->setDrivePosition(PxTransform::createIdentity());
		//	//Joint->setDriveVelocity(PxVec3(0), PxVec3(0));
		//}
	}
#endif // WITH_PHYSX
}



void UPhysicsHandleComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Alpha = FMath::Clamp(DeltaTime * InterpolationSpeed, 0.f, 1.f);

	FTransform C = CurrentTransform;
	FTransform T = TargetTransform;
	C.NormalizeRotation();
	T.NormalizeRotation();
	CurrentTransform.Blend(C, T, Alpha);

	UpdateHandleTransform(CurrentTransform);
}

void UPhysicsHandleComponent::GetTargetLocationAndRotation(FVector& OutLocation, FRotator& OutRotation) const 
{
	OutRotation = TargetTransform.Rotator();
	OutLocation = TargetTransform.GetTranslation();
}

void UPhysicsHandleComponent::SetLinearDamping(float NewLinearDamping)
{
	LinearDamping = NewLinearDamping;
	UpdateDriveSettings();
}

void UPhysicsHandleComponent::SetLinearStiffness(float NewLinearStiffness)
{
	LinearStiffness = NewLinearStiffness;
	UpdateDriveSettings();
}

void UPhysicsHandleComponent::SetAngularDamping(float NewAngularDamping)
{
	AngularDamping = NewAngularDamping;
	UpdateDriveSettings();
}

void UPhysicsHandleComponent::SetAngularStiffness(float NewAngularStiffness)
{
	AngularStiffness = NewAngularStiffness;
	UpdateDriveSettings();
}

void UPhysicsHandleComponent::SetInterpolationSpeed(float NewInterpolationSpeed)
{
	InterpolationSpeed = NewInterpolationSpeed;
}