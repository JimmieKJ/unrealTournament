// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/CullDistanceVolume.h"
#include "Components/BrushComponent.h"

ACullDistanceVolume::ACullDistanceVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->bAlwaysCreatePhysicsState = true;

	CullDistances.Add(FCullDistanceSizePair(0,0));
	CullDistances.Add(FCullDistanceSizePair(10000,0));

	bEnabled = true;
}

#if WITH_EDITOR
void ACullDistanceVolume::Destroyed()
{
	Super::Destroyed();

	UWorld* World = GetWorld();
	if (GIsEditor && World && World->IsGameWorld())
	{
		World->bDoDelayedUpdateCullDistanceVolumes = true;
	}
}

void ACullDistanceVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	GetWorld()->UpdateCullDistanceVolumes();
}

void ACullDistanceVolume::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if( bFinished )
	{
		GetWorld()->UpdateCullDistanceVolumes();
	}
}
#endif // WITH_EDITOR

bool ACullDistanceVolume::CanBeAffectedByVolumes( UPrimitiveComponent* PrimitiveComponent )
{
	AActor* Owner = PrimitiveComponent ? PrimitiveComponent->GetOwner() : NULL;

	// Require an owner so we can use its location
	if(	Owner
	// Disregard dynamic actors
	&& (PrimitiveComponent->Mobility == EComponentMobility::Static)
	// Require cull distance volume support to be enabled.
	&&	PrimitiveComponent->bAllowCullDistanceVolume 
	// Skip primitives that is hidden set as we don't want to cull out brush rendering or other helper objects.
	&&	PrimitiveComponent->IsVisible()
	// Disregard prefabs.
	&& !PrimitiveComponent->IsTemplate()
	// Only operate on primitives attached to the owners world.			
	&&	PrimitiveComponent->GetScene() == Owner->GetWorld()->Scene)
	{
		return true;
	}
	else
	{
		return false;
	}	
}

void ACullDistanceVolume::GetPrimitiveMaxDrawDistances(TMap<UPrimitiveComponent*,float>& OutCullDistances)
{
	// Nothing to do if there is no brush component or no cull distances are set
	if (GetBrushComponent() && CullDistances.Num() > 0 && bEnabled)
	{
		for (auto It(OutCullDistances.CreateIterator()); It; ++It)
		{
			UPrimitiveComponent* PrimitiveComponent = It.Key();

			// Check whether primitive can be affected by cull distance volumes.
			if( ACullDistanceVolume::CanBeAffectedByVolumes( PrimitiveComponent ) )
			{
				// Check whether primitive supports cull distance volumes and its center point is being encompassed by this volume.
				if( EncompassesPoint( PrimitiveComponent->GetComponentLocation() ) )
				{		
					// Find best match in CullDistances array.
					float PrimitiveSize			= PrimitiveComponent->Bounds.SphereRadius * 2;
					float CurrentError			= FLT_MAX;
					float CurrentCullDistance	= 0;
					for( int32 CullDistanceIndex=0; CullDistanceIndex<CullDistances.Num(); CullDistanceIndex++ )
					{
						const FCullDistanceSizePair& CullDistancePair = CullDistances[CullDistanceIndex];
						if( FMath::Abs( PrimitiveSize - CullDistancePair.Size ) < CurrentError )
						{
							CurrentError		= FMath::Abs( PrimitiveSize - CullDistancePair.Size );
							CurrentCullDistance = CullDistancePair.CullDistance;
						}
					}

					float& CullDistance = It.Value();

					// LD or other volume specified cull distance, use minimum of current and one used for this volume.
					if (CullDistance > 0)
					{
						CullDistance = FMath::Min(CullDistance, CurrentCullDistance);
					}
					// LD didn't specify cull distance, use current setting directly.
					else
					{
						CullDistance = CurrentCullDistance;
					}
				}
			}
		}
	}
}
