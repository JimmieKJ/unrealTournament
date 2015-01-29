// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHat.h"


AUTHat::AUTHat(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRootComponent(ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneRootComp")));

	GetRootComponent()->Mobility = EComponentMobility::Movable;

	bReplicates = false;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	
	HeadshotRotationRate.Yaw = 900;
	HeadshotRotationTime = 0.8f;
}

void AUTHat::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (GetRootComponent())
	{
		bool bRootNotPrimitiveComponent = true;

		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		if (PrimComponent)
		{
			PrimComponent->bReceivesDecals = false;
			PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			bRootNotPrimitiveComponent = false;
		}

		TArray<USceneComponent*> Children;
		GetRootComponent()->GetChildrenComponents(true, Children);
		for (auto Child : Children)
		{
			PrimComponent = Cast<UPrimitiveComponent>(Child);
			if (PrimComponent)
			{
				if (bRootNotPrimitiveComponent)
				{
					SetRootComponent(PrimComponent);
					bRootNotPrimitiveComponent = false;
				}
				PrimComponent->bReceivesDecals = false;
				PrimComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			}
		}
	}
}

void AUTHat::SetBodiesToSimulatePhysics()
{
	if (GetRootComponent())
	{
		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		if (PrimComponent)
		{
			PrimComponent->BodyInstance.bSimulatePhysics = true;
			PrimComponent->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
			PrimComponent->SetCollisionObjectType(ECC_PhysicsBody);
			PrimComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
			PrimComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			PrimComponent->SetPhysicsLinearVelocity(FVector(0, 0, 1));
			PrimComponent->SetNotifyRigidBodyCollision(true);
		}
	}
}

void AUTHat::OnWearerHeadshot_Implementation()
{
	bHeadshotRotating = true;
	GetWorldTimerManager().SetTimer(this, &AUTHat::HeadshotRotationComplete, HeadshotRotationTime, false);
}

void AUTHat::HeadshotRotationComplete()
{
	bHeadshotRotating = false;
	SetBodiesToSimulatePhysics();
}

void AUTHat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHeadshotRotating)
	{
		GetRootComponent()->AddLocalRotation(DeltaSeconds * HeadshotRotationRate);
	}
}