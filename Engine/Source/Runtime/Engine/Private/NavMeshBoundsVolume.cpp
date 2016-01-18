// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"

ANavMeshBoundsVolume::ANavMeshBoundsVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->Mobility = EComponentMobility::Static;

	BrushColor = FColor(200, 200, 200, 255);
	SupportedAgents.MarkInitialized();

	bColored = true;
}

#if WITH_EDITOR

void ANavMeshBoundsVolume::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (GIsEditor && NavSys)
	{
		if (PropertyChangedEvent.Property == NULL ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ABrush, BrushBuilder) ||
			(PropertyChangedEvent.MemberProperty && 
			 PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ANavMeshBoundsVolume, SupportedAgents)))
		{
			NavSys->OnNavigationBoundsUpdated(this);
		}
	}
}

#endif // WITH_EDITOR

void ANavMeshBoundsVolume::PostRegisterAllComponents() 
{
	Super::PostRegisterAllComponents();
	
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys && Role == ROLE_Authority)
	{
		NavSys->OnNavigationBoundsAdded(this);
	}
}

void ANavMeshBoundsVolume::PostUnregisterAllComponents() 
{
	Super::PostUnregisterAllComponents();
	
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys && Role == ROLE_Authority)
	{
		NavSys->OnNavigationBoundsRemoved(this);
	}
}
