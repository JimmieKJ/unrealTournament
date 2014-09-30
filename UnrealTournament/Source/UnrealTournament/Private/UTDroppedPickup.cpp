// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UTDroppedPickup.h"
#include "UnrealNetwork.h"

AUTDroppedPickup::AUTDroppedPickup(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Collision = PCIP.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Collision->SetCollisionProfileName(FName(TEXT("Pickup")));
	Collision->InitCapsuleSize(64.0f, 30.0f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTDroppedPickup::OnOverlapBegin);
	RootComponent = Collision;

	Movement = PCIP.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("Movement"));
	Movement->HitZStopSimulatingThreshold = 0.7f;
	Movement->UpdatedComponent = Collision;
	Movement->OnProjectileStop.AddDynamic(this, &AUTDroppedPickup::PhysicsStopped);

	//bCollideWhenPlacing = true; // causes too many false positives at the moment, re-evaluate later
	InitialLifeSpan = 15.0f;

	SetReplicates(true);
	bReplicateMovement = true;
	NetUpdateFrequency = 1.0f;
}

void AUTDroppedPickup::BeginPlay()
{
	Super::BeginPlay();

	if (!bPendingKillPending)
	{
		// don't allow Instigator to touch until a little time has passed so a live player throwing an item doesn't immediately pick it back up again
		GetWorld()->GetTimerManager().SetTimer(this, &AUTDroppedPickup::EnableInstigatorTouch, 1.0f, false);
	}
}

void AUTDroppedPickup::EnableInstigatorTouch()
{
	if (Instigator != NULL)
	{
		TArray<AActor*> Overlaps;
		GetOverlappingActors(Overlaps, APawn::StaticClass());
		if (Overlaps.Contains(Instigator))
		{
			FHitResult UnusedHitResult;
			OnOverlapBegin(Instigator, Instigator->GetMovementComponent()->UpdatedComponent, 0, false, UnusedHitResult);
		}
	}
}

void AUTDroppedPickup::SetInventory(AUTInventory* NewInventory)
{
	Inventory = NewInventory;
	InventoryType = (NewInventory != NULL) ? NewInventory->GetClass() : NULL;
	InventoryTypeUpdated();
}

void AUTDroppedPickup::InventoryTypeUpdated_Implementation()
{
	AUTPickupInventory::CreatePickupMesh(this, Mesh, InventoryType, 0.0f);
}

void AUTDroppedPickup::PhysicsStopped(const FHitResult& ImpactResult)
{
	// if we landed on a mover, attach to it
	if (ImpactResult.Component != NULL && ImpactResult.Component->Mobility == EComponentMobility::Movable)
	{
		Collision->AttachTo(ImpactResult.Component.Get(), NAME_None, EAttachLocation::KeepWorldPosition);
	}
	AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
	if (NavData != NULL)
	{
		NavData->AddToNavigation(this);
	}
}

void AUTDroppedPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
	if (NavData != NULL)
	{
		NavData->RemoveFromNavigation(this);
	}
}

void AUTDroppedPickup::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor != Instigator || !GetWorld()->GetTimerManager().IsTimerActive(this, &AUTDroppedPickup::EnableInstigatorTouch))
	{
		APawn* P = Cast<APawn>(OtherActor);
		if (P != NULL && !P->bTearOff)
		{
			ProcessTouch(P);
		}
	}
}

void AUTDroppedPickup::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority && TouchedBy->Controller != NULL && Cast<AUTCharacter>(TouchedBy) != NULL)
	{
		PlayTakenEffects(TouchedBy); // first allows PlayTakenEffects() to work off Inventory instead of InventoryType if it wants
		GiveTo(TouchedBy);
		Destroy();
	}
}

void AUTDroppedPickup::GiveTo_Implementation(APawn* Target)
{
	if (Inventory != NULL)
	{
		AUTCharacter* C = Cast<AUTCharacter>(Target);
		if (C != NULL)
		{
			AUTInventory* Duplicate = C->FindInventoryType<AUTInventory>(Inventory->GetClass(), true);
			if (Duplicate == NULL || !Duplicate->StackPickup(Inventory))
			{
				C->AddInventory(Inventory, true);
			}
		}
	}
}

void AUTDroppedPickup::PlayTakenEffects_Implementation(APawn* TakenBy)
{
	if (Inventory != NULL)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), Inventory->PickupSound, TakenBy, SRT_All, false, GetActorLocation());
	}
}

void AUTDroppedPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTDroppedPickup, InventoryType, COND_None);
}

float AUTDroppedPickup::BotDesireability_Implementation(APawn* Asker, float PathDistance)
{
	if (InventoryType == NULL)
	{
		return 0.0f;
	}
	else
	{
		// make sure Asker can actually get here before the pickup times out
		float LifeSpan = GetLifeSpan();
		if (LifeSpan > 0.0)
		{
			ACharacter* C = Cast<ACharacter>(Asker);
			return (C == NULL || PathDistance / C->CharacterMovement->MaxWalkSpeed > LifeSpan) ? 0.0f : InventoryType.GetDefaultObject()->BotDesireability(Asker, this, PathDistance);
		}
		else
		{
			return InventoryType.GetDefaultObject()->BotDesireability(Asker, this, PathDistance);
		}
	}
}