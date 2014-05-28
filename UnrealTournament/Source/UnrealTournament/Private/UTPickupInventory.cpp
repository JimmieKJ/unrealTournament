// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UnrealNetwork.h"

void AUTPickupInventory::BeginPlay()
{
	Super::BeginPlay();

	SetInventoryType(InventoryType);
}

void AUTPickupInventory::SetInventoryType(TSubclassOf<AUTInventory> NewType)
{
	InventoryType = NewType;
	if (InventoryType != NULL)
	{
		RespawnTime = InventoryType.GetDefaultObject()->RespawnTime;
		bDelayedSpawn = InventoryType.GetDefaultObject()->bDelayedSpawn;
	}
	else
	{
		RespawnTime = 0.0f;
	}
	InventoryTypeUpdated();
	if (InventoryType == NULL || bDelayedSpawn)
	{
		StartSleeping();
	}
	else
	{
		WakeUp();
	}
}

void AUTPickupInventory::InventoryTypeUpdated_Implementation()
{
	if (InventoryType == NULL)
	{
		if (Mesh != NULL)
		{
			Mesh->DetachFromParent();
			Mesh->UnregisterComponent();
			Mesh = NULL;
		}
	}
	else
	{
		USkeletalMeshComponent* NewMesh = InventoryType.GetDefaultObject()->GetPickupMeshTemplate();
		if (NewMesh == NULL)
		{
			if (Mesh != NULL)
			{
				Mesh->DetachFromParent();
				Mesh->UnregisterComponent();
				Mesh = NULL;
			}
		}
		else if (Mesh == NULL || Mesh->SkeletalMesh != NewMesh->SkeletalMesh || Mesh->Materials != NewMesh->Materials)
		{
			if (Mesh != NULL)
			{
				Mesh->DetachFromParent();
				Mesh->UnregisterComponent();
			}
			Mesh = ConstructObject<USkeletalMeshComponent>(NewMesh->GetClass(), this, NAME_None, RF_NoFlags, NewMesh);
			Mesh->AttachParent = NULL;
			Mesh->AttachChildren.Empty();
			Mesh->RelativeRotation = FRotator::ZeroRotator;
			Mesh->bForceRefpose = true;
			Mesh->RegisterComponent();
			Mesh->AttachTo(RootComponent);
		}
	}
}

void AUTPickupInventory::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Cast<AUTCharacter>(TouchedBy) != NULL)
	{
		Super::ProcessTouch_Implementation(TouchedBy);
	}
	// TODO: vehicle consideration
}

void AUTPickupInventory::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL && InventoryType != NULL)
	{
		FActorSpawnParameters Params;
		Params.bNoCollisionFail = true;
		Params.Instigator = P;
		P->AddInventory(GetWorld()->SpawnActor<AUTInventory>(InventoryType, GetActorLocation(), GetActorRotation(), Params), true);
	}
}

void AUTPickupInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTPickupInventory, InventoryType, COND_None);
}