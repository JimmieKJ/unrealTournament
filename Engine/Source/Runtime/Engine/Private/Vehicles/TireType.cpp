// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Vehicles/TireType.h"

#if WITH_PHYSX
	#include "PhysXVehicleManager.h"
#endif

TArray<TWeakObjectPtr<UTireType>> UTireType::AllTireTypes;

UTireType::UTireType(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Property initialization
	FrictionScale = 1.0f;
}

void UTireType::SetFrictionScale( float NewFrictionScale )
{
	if ( NewFrictionScale != FrictionScale )
	{
		FrictionScale = NewFrictionScale;

		NotifyTireFrictionUpdated();
	}
}

void UTireType::PostInitProperties()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Set our TireTypeID - either by finding an available slot or creating a new one
		int32 TireTypeIndex = AllTireTypes.Find(NULL);

		if ( TireTypeIndex == INDEX_NONE )
		{
			TireTypeIndex = AllTireTypes.Add(this);
		}
		else
		{
			AllTireTypes[TireTypeIndex] = this;
		}
	
		TireTypeID = (int32)TireTypeIndex;

		NotifyTireFrictionUpdated();
	}

	Super::PostInitProperties();
}

void UTireType::BeginDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// free our TireTypeID
		check(AllTireTypes.IsValidIndex(TireTypeID));
		check(AllTireTypes[TireTypeID] == this);
		AllTireTypes[TireTypeID] = NULL;

		NotifyTireFrictionUpdated();
	}

	Super::BeginDestroy();
}

void UTireType::NotifyTireFrictionUpdated()
{
#if WITH_VEHICLE
	FPhysXVehicleManager::UpdateTireFrictionTable();
#endif // WITH_PHYSX
}

#if WITH_EDITOR

void UTireType::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	if ( PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == FName(TEXT("FrictionScale")) )
	{
		NotifyTireFrictionUpdated();
	}
}

#endif //WITH_EDITOR