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

void AUTPickupInventory::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// we have a special class for weapons so don't allow setting them here
	if (PropertyChangedEvent.Property == NULL || PropertyChangedEvent.Property->GetFName() == FName(TEXT("InventoryType")))
	{
		if (InventoryType->IsChildOf(AUTWeapon::StaticClass()))
		{
			InventoryType = NULL;
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Use UTPickupWeapon for weapon pickups.")));
		}
	}
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
		UMeshComponent* NewMesh = InventoryType.GetDefaultObject()->GetPickupMeshTemplate();
		if (NewMesh == NULL)
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
			USkeletalMeshComponent* SkelTemplate = Cast<USkeletalMeshComponent>(NewMesh);
			UStaticMeshComponent* StaticTemplate = Cast<UStaticMeshComponent>(NewMesh);
			if ( Mesh == NULL || Mesh->GetClass() != NewMesh->GetClass() || Mesh->Materials != NewMesh->Materials ||
				(SkelTemplate != NULL && ((USkeletalMeshComponent*)Mesh)->SkeletalMesh != SkelTemplate->SkeletalMesh) ||
				(StaticTemplate != NULL && ((UStaticMeshComponent*)Mesh)->StaticMesh != StaticTemplate->StaticMesh) )
			{
				if (Mesh != NULL)
				{
					Mesh->DetachFromParent();
					Mesh->UnregisterComponent();
				}
				Mesh = ConstructObject<UMeshComponent>(NewMesh->GetClass(), this, NAME_None, RF_NoFlags, NewMesh);
				Mesh->AttachParent = NULL;
				Mesh->AttachChildren.Empty();
				Mesh->RelativeRotation = FRotator::ZeroRotator;
				if (SkelTemplate != NULL)
				{
					((USkeletalMeshComponent*)Mesh)->bForceRefpose = true;
				}
				Mesh->RegisterComponent();
				Mesh->AttachTo(RootComponent);
			}
		}
	}
}

void AUTPickupInventory::SetPickupHidden(bool bNowHidden)
{
	if (Mesh != NULL)
	{
		Mesh->SetHiddenInGame(bNowHidden);
	}
	else
	{
		Super::SetPickupHidden(bNowHidden);
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
		AUTInventory* Existing = P->FindInventoryType(InventoryType, true);
		if (Existing == NULL || !Existing->StackPickup(NULL))
		{
			FActorSpawnParameters Params;
			Params.bNoCollisionFail = true;
			Params.Instigator = P;
			P->AddInventory(GetWorld()->SpawnActor<AUTInventory>(InventoryType, GetActorLocation(), GetActorRotation(), Params), true);
		}
	}
}

void AUTPickupInventory::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTPickupInventory, InventoryType, COND_None);
}