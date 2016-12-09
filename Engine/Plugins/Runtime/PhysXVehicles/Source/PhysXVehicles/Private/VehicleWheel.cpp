// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VehicleWheel.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "PhysxUserData.h"
#include "Engine/StaticMesh.h"
#include "Vehicles/TireType.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TireConfig.h"
#include "PhysXVehicleManager.h"
#include "PhysXPublic.h"


UVehicleWheel::UVehicleWheel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CollisionMeshObj(TEXT("/Engine/EngineMeshes/Cylinder"));
	CollisionMesh = CollisionMeshObj.Object;

	ShapeRadius = 30.0f;
	ShapeWidth = 10.0f;
	bAutoAdjustCollisionSize = true;
	Mass = 20.0f;
	bAffectedByHandbrake = true;
	SteerAngle = 70.0f;
	MaxBrakeTorque = 1500.f;
	MaxHandBrakeTorque = 3000.f;
	DampingRate = 0.25f;
	LatStiffMaxLoad = 2.0f;
	LatStiffValue = 17.0f;
	LongStiffValue = 1000.0f;
	SuspensionForceOffset = 0.0f;
	SuspensionMaxRaise = 10.0f;
	SuspensionMaxDrop = 10.0f;
	SuspensionNaturalFrequency = 7.0f;
	SuspensionDampingRatio = 1.0f;
}

float UVehicleWheel::GetSteerAngle() const
{
	FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(VehicleSim->GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(VehicleManager->GetScene());
	return FMath::RadiansToDegrees( VehicleManager->GetWheelsStates_AssumesLocked(VehicleSim)[WheelIndex].steerAngle );
}

float UVehicleWheel::GetRotationAngle() const
{
	FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(VehicleSim->GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(VehicleManager->GetScene());

	float RotationAngle = -1.0f * FMath::RadiansToDegrees( VehicleSim->PVehicle->mWheelsDynData.getWheelRotationAngle( WheelIndex ) );
	check(!FMath::IsNaN(RotationAngle));
	return RotationAngle;
}

float UVehicleWheel::GetSuspensionOffset() const
{
	FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(VehicleSim->GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(VehicleManager->GetScene());

	return VehicleManager->GetWheelsStates_AssumesLocked(VehicleSim)[WheelIndex].suspJounce;
}

void UVehicleWheel::Init( UWheeledVehicleMovementComponent* InVehicleSim, int32 InWheelIndex )
{
	check(InVehicleSim);
	check(InVehicleSim->Wheels.IsValidIndex(InWheelIndex));

	VehicleSim = InVehicleSim;
	WheelIndex = InWheelIndex;
	WheelShape = NULL;

	FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(VehicleSim->GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(VehicleManager->GetScene());

	const int32 WheelShapeIdx = VehicleSim->PVehicle->mWheelsSimData.getWheelShapeMapping( WheelIndex );
	check(WheelShapeIdx >= 0);

	VehicleSim->PVehicle->getRigidDynamicActor()->getShapes( &WheelShape, 1, WheelShapeIdx );
	check(WheelShape);

	Location = GetPhysicsLocation();
	OldLocation = Location;
}

void UVehicleWheel::Shutdown()
{
	WheelShape = NULL;
}

FWheelSetup& UVehicleWheel::GetWheelSetup()
{
	return VehicleSim->WheelSetups[WheelIndex];
}

void UVehicleWheel::Tick( float DeltaTime )
{
	OldLocation = Location;
	Location = GetPhysicsLocation();
	Velocity = ( Location - OldLocation ) / DeltaTime;
}

FVector UVehicleWheel::GetPhysicsLocation()
{
	if ( WheelShape )
	{
		FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(VehicleSim->GetWorld()->GetPhysicsScene());
		SCOPED_SCENE_READ_LOCK(VehicleManager->GetScene());

		PxVec3 PLocation = VehicleSim->PVehicle->getRigidDynamicActor()->getGlobalPose().transform( WheelShape->getLocalPose() ).p;
		return P2UVector( PLocation );
	}

	return FVector(0.0f);
}

#if WITH_EDITOR

void UVehicleWheel::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	// Trigger a runtime rebuild of the PhysX vehicle
	FPhysXVehicleManager::VehicleSetupTag++;
}

#endif //WITH_EDITOR


UPhysicalMaterial* UVehicleWheel::GetContactSurfaceMaterial()
{
	UPhysicalMaterial* PhysMaterial = NULL;

	FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(VehicleSim->GetWorld()->GetPhysicsScene());
	SCOPED_SCENE_READ_LOCK(VehicleManager->GetScene());

	const PxMaterial* ContactSurface = VehicleManager->GetWheelsStates_AssumesLocked(VehicleSim)[WheelIndex].tireSurfaceMaterial;
	if (ContactSurface)
	{
		PhysMaterial = FPhysxUserData::Get<UPhysicalMaterial>(ContactSurface->userData);
	}

	return PhysMaterial;
}
