// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicalMaterial.cpp
=============================================================================*/ 

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "Vehicles/TireType.h"
#include "PhysicsEngine/PhysicsSettings.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
	#include "../Vehicles/PhysXVehicleManager.h"
#endif // WITH_PHYSX
#include "PhysicalMaterials/PhysicalMaterial.h"

UDEPRECATED_PhysicalMaterialPropertyBase::UDEPRECATED_PhysicalMaterialPropertyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPhysicalMaterial::UPhysicalMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Friction = 0.7f;
	Restitution = 0.3f;
	RaiseMassToPower = 0.75f;
	Density = 1.0f;
	DestructibleDamageThresholdScale = 1.0f;
	TireFrictionScale = 1.0f;
	bOverrideFrictionCombineMode = false;
#if WITH_PHYSX
	PhysxUserData = FPhysxUserData(this);
#endif
}

#if WITH_EDITOR
void UPhysicalMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Update PhysX material last so we have a valid Parent
	UpdatePhysXMaterial();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UPhysicalMaterial::RebuildPhysicalMaterials()
{
	for (FObjectIterator Iter(UPhysicalMaterial::StaticClass()); Iter; ++Iter)
	{
		if (UPhysicalMaterial * PhysicalMaterial = Cast<UPhysicalMaterial>(*Iter))
		{
			PhysicalMaterial->UpdatePhysXMaterial();
		}
	}
}

#endif // WITH_EDITOR

void UPhysicalMaterial::PostLoad()
{
	Super::PostLoad();

	// we're removing physical material property, so convert to Material type
	if (GetLinkerUE4Version() < VER_UE4_REMOVE_PHYSICALMATERIALPROPERTY)
	{
		if (PhysicalMaterialProperty)
		{
			SurfaceType = PhysicalMaterialProperty->ConvertToSurfaceType();
		}
	}
}

void UPhysicalMaterial::FinishDestroy()
{
#if WITH_PHYSX
	if(PMaterial != NULL)
	{
		GPhysXPendingKillMaterial.Add(PMaterial);
		PMaterial->userData = NULL;
		PMaterial = NULL;
	}
#endif // WITH_PHYSX

	Super::FinishDestroy();
}


void UPhysicalMaterial::UpdatePhysXMaterial()
{
#if WITH_PHYSX
	if(PMaterial != NULL)
	{
		PMaterial->setStaticFriction(Friction);
		PMaterial->setDynamicFriction(Friction);
		const uint32 UseFrictionCombineMode = (bOverrideFrictionCombineMode ? FrictionCombineMode.GetValue() : UPhysicsSettings::Get()->FrictionCombineMode.GetValue());
		PMaterial->setFrictionCombineMode(static_cast<physx::PxCombineMode::Enum>(UseFrictionCombineMode));

		PMaterial->setRestitution(Restitution);
		const uint32 UseRestitutionCombineMode = (bOverrideRestitutionCombineMode ? RestitutionCombineMode.GetValue() : UPhysicsSettings::Get()->RestitutionCombineMode.GetValue());
		PMaterial->setRestitutionCombineMode(static_cast<physx::PxCombineMode::Enum>(UseRestitutionCombineMode));

#if WITH_VEHICLE
		FPhysXVehicleManager::UpdateTireFrictionTable();
#endif
	}
#endif	// WITH_PHYSX
}


#if WITH_PHYSX
PxMaterial* UPhysicalMaterial::GetPhysXMaterial()
{
	if((PMaterial == NULL) && GPhysXSDK)
	{
		PMaterial = GPhysXSDK->createMaterial(Friction, Friction, Restitution);
		const uint32 UseFrictionCombineMode = (bOverrideFrictionCombineMode ? FrictionCombineMode.GetValue() : UPhysicsSettings::Get()->FrictionCombineMode.GetValue());
		PMaterial->setFrictionCombineMode(static_cast<physx::PxCombineMode::Enum>(UseFrictionCombineMode));
		
		const uint32 UseRestitutionCombineMode = (bOverrideRestitutionCombineMode ? RestitutionCombineMode.GetValue() : UPhysicsSettings::Get()->RestitutionCombineMode.GetValue());
		PMaterial->setRestitutionCombineMode(static_cast<physx::PxCombineMode::Enum>(UseRestitutionCombineMode));

		PMaterial->userData = &PhysxUserData;

		UpdatePhysXMaterial();
	}

	return PMaterial;
}
#endif // WITH_PHYSX

EPhysicalSurface UPhysicalMaterial::DetermineSurfaceType(UPhysicalMaterial const* PhysicalMaterial)
{
	if (PhysicalMaterial == NULL)
	{
		PhysicalMaterial = GEngine->DefaultPhysMaterial;
	}
	
	return PhysicalMaterial->SurfaceType;
}

float UPhysicalMaterial::GetTireFrictionScale( TWeakObjectPtr<UTireType> TireType )
{
	float Scale = (TireType != NULL) ? TireType->GetFrictionScale() : 1.0f;

	Scale *= TireFrictionScale;

	for ( int32 i = TireFrictionScales.Num() - 1; i >= 0; --i )
	{
		if ( TireFrictionScales[i].TireType == TireType.Get() )
		{
			Scale *= TireFrictionScales[i].FrictionScale;
			break;
		}
	}

	return Scale;
}

