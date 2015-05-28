// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/BrushComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "PhysicsEngine/PhysicsSettings.h"

APhysicsVolume::APhysicsVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static FName CollisionProfileName(TEXT("OverlapAllDynamic"));
	GetBrushComponent()->SetCollisionProfileName(CollisionProfileName);

	FluidFriction = UPhysicsSettings::Get()->DefaultFluidFriction;
	TerminalVelocity = UPhysicsSettings::Get()->DefaultTerminalVelocity;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 0.1f;
	bReplicateMovement = false;
}

#if WITH_EDITOR
void APhysicsVolume::LoadedFromAnotherClass(const FName& OldClassName)
{
	Super::LoadedFromAnotherClass(OldClassName);

	if(GetLinkerUE4Version() < VER_UE4_REMOVE_DYNAMIC_VOLUME_CLASSES)
	{
		static FName DynamicPhysicsVolume_NAME(TEXT("DynamicPhysicsVolume"));

		if(OldClassName == DynamicPhysicsVolume_NAME)
		{
			GetBrushComponent()->Mobility = EComponentMobility::Movable;
		}
	}
}
#endif

void APhysicsVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	GetWorld()->AddPhysicsVolume(this);
}

void APhysicsVolume::Destroyed()
{
	UWorld* MyWorld = GetWorld();
	if (MyWorld)
	{
		MyWorld->RemovePhysicsVolume(this);
	}
	Super::Destroyed();
}

void APhysicsVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* MyWorld = GetWorld();
	if (MyWorld)
	{
		MyWorld->RemovePhysicsVolume(this);
	}
	Super::EndPlay(EndPlayReason);
}


bool APhysicsVolume::IsOverlapInVolume(const class USceneComponent& TestComponent) const
{
	bool bInsideVolume = true;
	if (!bPhysicsOnContact)
	{
		FVector ClosestPoint(0.f);
		UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetRootComponent());
		const float DistToCollision = RootPrimitive ? RootPrimitive->GetDistanceToCollision(TestComponent.GetComponentLocation(), ClosestPoint) : 0.f;
		bInsideVolume = (DistToCollision == 0.f);
	}

	return bInsideVolume;
}

float APhysicsVolume::GetGravityZ() const
{
	return GetWorld()->GetGravityZ();
}

void APhysicsVolume::ActorEnteredVolume(AActor* Other) {}

void APhysicsVolume::ActorLeavingVolume(AActor* Other) {}

