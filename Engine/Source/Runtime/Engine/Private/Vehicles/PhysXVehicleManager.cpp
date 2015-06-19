// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "Vehicles/TireType.h"
#include "Vehicles/WheeledVehicleMovementComponent.h"

#if WITH_VEHICLE

#include "../PhysicsEngine/PhysXSupport.h"
#include "PhysXVehicleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

DEFINE_LOG_CATEGORY(LogVehicles);

bool FPhysXVehicleManager::bUpdateTireFrictionTable = false;
PxVehicleDrivableSurfaceToTireFrictionPairs* FPhysXVehicleManager::SurfaceTirePairs = NULL;
uint32 FPhysXVehicleManager::VehicleSetupTag = 0;

/**
 * prefilter shader for suspension raycasts
 */
static PxQueryHitType::Enum WheelRaycastPreFilter(	
	PxFilterData SuspensionData, 
	PxFilterData HitData,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& filterFlags)
{
	// SuspensionData is the vehicle suspension raycast.
	// HitData is the shape potentially hit by the raycast.

	// don't collide with owner chassis
	if ( SuspensionData.word0 == HitData.word0 )
	{
		return PxSceneQueryHitType::eNONE;
	}

	// collision channels filter
	ECollisionChannel SuspensionChannel = (ECollisionChannel)(SuspensionData.word3 >> 24);

	if ( ECC_TO_BITFIELD(SuspensionChannel) & HitData.word1)
	{
		// debug what object we hit
		if ( false )
		{
			for ( FObjectIterator It; It; ++It )
			{
				if ( It->GetUniqueID() == HitData.word0 )
				{
					UObject* HitObj = *It;
					FString HitObjName = HitObj->GetName();
					break;
				}
			}
		}

		return PxSceneQueryHitType::eBLOCK;
	}

	return PxSceneQueryHitType::eNONE;
}

FPhysXVehicleManager::FPhysXVehicleManager(PxScene* PhysXScene)
	: Scene(PhysXScene)
	, WheelRaycastBatchQuery(NULL)

#if PX_DEBUG_VEHICLE_ON
	, TelemetryData4W(NULL)
	, TelemetryVehicle(NULL)
#endif
{
	// Set the correct basis vectors with Z up, X forward. It's very IMPORTANT to set the Ackermann axle separation and frontWidth, rearWidth accordingly
	PxVehicleSetBasisVectors( PxVec3(0,0,1), PxVec3(1,0,0) );
}

FPhysXVehicleManager::~FPhysXVehicleManager()
{
#if PX_DEBUG_VEHICLE_ON
	if(TelemetryData4W)
	{
		TelemetryData4W->free();
		TelemetryData4W = NULL;
	}

	TelemetryVehicle = NULL;
#endif

	// Remove the N-wheeled vehicles.
	while( Vehicles.Num() > 0 )
	{
		RemoveVehicle( Vehicles.Last() );
	}

	// Release batch query data
	if ( WheelRaycastBatchQuery )
	{
		WheelRaycastBatchQuery->release();
		WheelRaycastBatchQuery = NULL;
	}

	// Release the  friction values used for combinations of tire type and surface type.
	//if ( SurfaceTirePairs )
	//{
	//	SurfaceTirePairs->release();
	//	SurfaceTirePairs = NULL;
	//}
}

void FPhysXVehicleManager::UpdateTireFrictionTable()
{
	bUpdateTireFrictionTable = true;
}

void FPhysXVehicleManager::UpdateTireFrictionTableInternal()
{
	const PxU32 MAX_NUM_MATERIALS = 128;

	// There are tire types and then there are drivable surface types.
	// PhysX supports physical materials that share a drivable surface type,
	// but we just create a drivable surface type for every type of physical material
	PxMaterial*							AllPhysicsMaterials[MAX_NUM_MATERIALS];
	PxVehicleDrivableSurfaceType		DrivableSurfaceTypes[MAX_NUM_MATERIALS];

	// Gather all the physical materials
	uint32 NumMaterials = GPhysXSDK->getMaterials(AllPhysicsMaterials, MAX_NUM_MATERIALS);

	uint32 NumTireTypes = UTireType::AllTireTypes.Num();

	for ( uint32 m = 0; m < NumMaterials; ++m )
	{
		// Set up the drivable surface type that will be used for the new material.
		DrivableSurfaceTypes[m].mType = m;
	}

	// Release the previous SurfaceTirePairs, if any
	if ( SurfaceTirePairs )
	{
		SurfaceTirePairs->release();
		SurfaceTirePairs = NULL;
	}

	// Set up the friction values arising from combinations of tire type and surface type.
	SurfaceTirePairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate( NumTireTypes, NumMaterials );
	SurfaceTirePairs->setup( NumTireTypes, NumMaterials, (const PxMaterial**)AllPhysicsMaterials, DrivableSurfaceTypes );

	for ( uint32 m = 0; m < NumMaterials; ++m )
	{
		UPhysicalMaterial* PhysMat = FPhysxUserData::Get<UPhysicalMaterial>(AllPhysicsMaterials[m]->userData);

		for ( uint32 t = 0; t < NumTireTypes; ++t )
		{
			TWeakObjectPtr<UTireType> TireType = UTireType::AllTireTypes[t];

			float TireFrictionScale = 1.0f;

			if ( PhysMat != NULL )
			{
				TireFrictionScale *= PhysMat->GetTireFrictionScale( TireType );
			}
			else if ( TireType != NULL )
			{
				TireFrictionScale *= TireType->GetFrictionScale();
			}
			
			SurfaceTirePairs->setTypePairFriction( m, t, TireFrictionScale );
		}
	}
}

void FPhysXVehicleManager::SetUpBatchedSceneQuery()
{
	int32 NumWheels = 0;

	for ( int32 v = PVehicles.Num() - 1; v >= 0; --v )
	{
		NumWheels += PVehicles[v]->mWheelsSimData.getNbWheels();
	}

	if ( NumWheels > WheelQueryResults.Num() )
	{
		WheelQueryResults.AddZeroed( NumWheels - WheelQueryResults.Num() );
		WheelHitResults.AddZeroed( NumWheels - WheelHitResults.Num() );

		check( WheelHitResults.Num() == WheelQueryResults.Num() );

		if ( WheelRaycastBatchQuery )
		{
			WheelRaycastBatchQuery->release();
			WheelRaycastBatchQuery = NULL;
		}
		
		PxBatchQueryDesc SqDesc(NumWheels, 0, 0);
		SqDesc.queryMemory.userRaycastResultBuffer = WheelQueryResults.GetData();
		SqDesc.queryMemory.userRaycastTouchBuffer = WheelHitResults.GetData();
		SqDesc.queryMemory.raycastTouchBufferSize = WheelHitResults.Num();
		SqDesc.preFilterShader = WheelRaycastPreFilter;

		WheelRaycastBatchQuery = Scene->createBatchQuery( SqDesc );
	}
}

void FPhysXVehicleManager::AddVehicle( TWeakObjectPtr<UWheeledVehicleMovementComponent> Vehicle )
{
	check(Vehicle != NULL);
	check(Vehicle->PVehicle);

	Vehicles.Add( Vehicle );
	PVehicles.Add( Vehicle->PVehicle );

	// init wheels' states
	int32 NewIndex = PVehiclesWheelsStates.AddZeroed();
	PxU32 NumWheels = Vehicle->PVehicle->mWheelsSimData.getNbWheels();
	PVehiclesWheelsStates[NewIndex].nbWheelQueryResults = NumWheels;
	PVehiclesWheelsStates[NewIndex].wheelQueryResults = new PxWheelQueryResult[NumWheels];  

	SetUpBatchedSceneQuery();
}

void FPhysXVehicleManager::RemoveVehicle( TWeakObjectPtr<UWheeledVehicleMovementComponent> Vehicle )
{
	check(Vehicle != NULL);
	check(Vehicle->PVehicle);

	PxVehicleWheels* PVehicle = Vehicle->PVehicle;

	int32 RemovedIndex = Vehicles.Find(Vehicle);

	Vehicles.Remove( Vehicle );
	PVehicles.Remove( PVehicle );

	delete[] PVehiclesWheelsStates[RemovedIndex].wheelQueryResults;
	PVehiclesWheelsStates.RemoveAt(RemovedIndex); // LOC_MOD double check this
	//PVehiclesWheelsStates.Remove(PVehiclesWheelsStates[RemovedIndex]);

	if ( PVehicle == TelemetryVehicle )
	{
		TelemetryVehicle = NULL;
	}

	switch( PVehicle->getVehicleType() )
	{
	case PxVehicleTypes::eDRIVE4W:
		((PxVehicleDrive4W*)PVehicle)->free();
		break;
	case PxVehicleTypes::eDRIVETANK:
		((PxVehicleDriveTank*)PVehicle)->free();
		break;
	case PxVehicleTypes::eNODRIVE:
		((PxVehicleNoDrive*)PVehicle)->free();
		break;
	default:
		checkf( 0, TEXT("Unsupported vehicle type"));
		break;
	}
}

void FPhysXVehicleManager::Update( float DeltaTime )
{
	if ( Vehicles.Num() == 0 )
	{
		return;
	}

	if ( bUpdateTireFrictionTable )
	{
		bUpdateTireFrictionTable = false;
		UpdateTireFrictionTableInternal();
	}

	// Suspension raycasts
	{
		SCOPED_SCENE_READ_LOCK(Scene);
		PxVehicleSuspensionRaycasts( WheelRaycastBatchQuery, PVehicles.Num(), PVehicles.GetData(), WheelQueryResults.Num(), WheelQueryResults.GetData() );
	}
	

	// Tick vehicles
	for ( int32 i = Vehicles.Num() - 1; i >= 0; --i )
	{
		Vehicles[i]->TickVehicle( DeltaTime );
	}

#if PX_DEBUG_VEHICLE_ON

	if ( TelemetryVehicle != NULL )
	{
		UpdateVehiclesWithTelemetry( DeltaTime );
	}
	else
	{
		UpdateVehicles( DeltaTime );
	}

#else

	UpdateVehicles( DeltaTime );

#endif //PX_DEBUG_VEHICLE_ON
}

void FPhysXVehicleManager::PreTick( float DeltaTime )
{
	for (int32 i = 0; i < Vehicles.Num(); ++i)
	{
		Vehicles[i]->PreTick(DeltaTime);
	}
}

void FPhysXVehicleManager::UpdateVehicles( float DeltaTime )
{
	SCOPED_SCENE_WRITE_LOCK(Scene);
	PxVehicleUpdates( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, PVehicles.Num(), PVehicles.GetData(), PVehiclesWheelsStates.GetData());
}

PxVec3 FPhysXVehicleManager::GetSceneGravity_AssumesLocked()
{
	return Scene->getGravity();
}

void FPhysXVehicleManager::SetRecordTelemetry( TWeakObjectPtr<UWheeledVehicleMovementComponent> Vehicle, bool bRecord )
{
#if PX_DEBUG_VEHICLE_ON

	if ( Vehicle != NULL && Vehicle->PVehicle != NULL )
	{
		PxVehicleWheels* PVehicle = Vehicle->PVehicle;

		if ( bRecord )
		{
			int32 VehicleIndex = Vehicles.Find( Vehicle );

			if ( VehicleIndex != INDEX_NONE )
			{
				// Make sure telemetry is setup
				SetupTelemetryData();

				TelemetryVehicle = PVehicle;

				if ( VehicleIndex != 0 )
				{
					Vehicles.Swap( 0, VehicleIndex );
					PVehicles.Swap( 0, VehicleIndex );
					PVehiclesWheelsStates.Swap( 0, VehicleIndex );
				}
			}
		}
		else
		{
			if ( PVehicle == TelemetryVehicle )
			{
				TelemetryVehicle = NULL;
			}
		}
	}

#endif
}

#if PX_DEBUG_VEHICLE_ON

void FPhysXVehicleManager::SetupTelemetryData()
{
	// set up telemetry for 4 wheels
	SCOPED_SCENE_WRITE_LOCK(Scene);
	if(TelemetryData4W == NULL)
	{
		float Empty[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

		TelemetryData4W = PxVehicleTelemetryData::allocate(4);
		TelemetryData4W->setup(1.0f, 1.0f, 0.0f, 0.0f, Empty, Empty, PxVec3(0,0,0), PxVec3(0,0,0), PxVec3(0,0,0));
	}
}

void FPhysXVehicleManager::UpdateVehiclesWithTelemetry( float DeltaTime )
{
	check(TelemetryVehicle);
	check(PVehicles.Find(TelemetryVehicle) == 0);

	SCOPED_SCENE_WRITE_LOCK(Scene);
	if ( PxVehicleTelemetryData* TelemetryData = GetTelemetryData_AssumesLocked() )
	{
		PxVehicleUpdateSingleVehicleAndStoreTelemetryData( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, TelemetryVehicle, PVehiclesWheelsStates.GetData(), *TelemetryData );

		if ( PVehicles.Num() > 1 )
		{
			PxVehicleUpdates( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, PVehicles.Num() - 1, &PVehicles[1], &PVehiclesWheelsStates[1] );
		}
	}
	else
	{
		UE_LOG( LogPhysics, Warning, TEXT("Cannot record telemetry for vehicle, it does not have 4 wheels") );

		PxVehicleUpdates( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, PVehicles.Num(), PVehicles.GetData(), PVehiclesWheelsStates.GetData() );
	}
}

PxVehicleTelemetryData* FPhysXVehicleManager::GetTelemetryData_AssumesLocked()
{
	if ( TelemetryVehicle )
	{
		if ( TelemetryVehicle->mWheelsSimData.getNbWheels() == 4 )
		{
			return TelemetryData4W;
		}
	}

	return NULL;
}

#endif //PX_DEBUG_VEHICLE_ON

PxWheelQueryResult* FPhysXVehicleManager::GetWheelsStates_AssumesLocked(TWeakObjectPtr<class UWheeledVehicleMovementComponent> Vehicle)
{
	int32 Index = Vehicles.Find(Vehicle);

	if(Index != INDEX_NONE)
	{
		return PVehiclesWheelsStates[Index].wheelQueryResults;
	}
	else
	{
		return NULL;
	}
}
#endif // WITH_PHYSX
