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

	Movement = PCIP.CreateDefaultSubobject<UProjectileMovementComponent>(this, TEXT("Movement"));
	Movement->UpdatedComponent = Collision;

	InitialLifeSpan = 15.0f;

	SetReplicates(true);
	bReplicateMovement = true;
}

void AUTDroppedPickup::BeginPlay()
{
	Super::BeginPlay();

	// don't allow Instigator to touch until a little time has passed so a live player throwing an item doesn't immediately pick it back up again
	GetWorld()->GetTimerManager().SetTimer(this, &AUTDroppedPickup::EnableInstigatorTouch, 1.0f, false);
}

void AUTDroppedPickup::EnableInstigatorTouch()
{
	if (Instigator != NULL)
	{
		TArray<AActor*> Overlaps;
		GetOverlappingActors(Overlaps, APawn::StaticClass());
		if (Overlaps.Contains(Instigator))
		{
			OnOverlapBegin(Instigator, Instigator->GetMovementComponent()->UpdatedComponent, 0);
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

void AUTDroppedPickup::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
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