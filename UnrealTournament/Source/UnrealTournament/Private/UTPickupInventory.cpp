// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UnrealNetwork.h"

AUTPickupInventory::AUTPickupInventory(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	FloatHeight = 50.0f;
}

void AUTPickupInventory::BeginPlay()
{
	AActor::BeginPlay(); // skip UTPickup as SetInventoryType() will handle delayed spawn

	SetInventoryType(InventoryType);
}

#if WITH_EDITOR
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
#endif

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

void AUTPickupInventory::CreatePickupMesh(AActor* Pickup, UMeshComponent*& PickupMesh, TSubclassOf<AUTInventory> PickupInventoryType, float MeshFloatHeight)
{
	if (PickupInventoryType == NULL)
	{
		if (PickupMesh != NULL)
		{
			PickupMesh->DetachFromParent();
			PickupMesh->UnregisterComponent();
			PickupMesh = NULL;
		}
	}
	else
	{
		FVector OverrideScale(FVector::ZeroVector);
		UMeshComponent* NewMesh = PickupInventoryType.GetDefaultObject()->GetPickupMeshTemplate(OverrideScale);
		if (NewMesh == NULL)
		{
			if (PickupMesh != NULL)
			{
				PickupMesh->DetachFromParent();
				PickupMesh->UnregisterComponent();
				PickupMesh = NULL;
			}
		}
		else
		{
			USkeletalMeshComponent* SkelTemplate = Cast<USkeletalMeshComponent>(NewMesh);
			UStaticMeshComponent* StaticTemplate = Cast<UStaticMeshComponent>(NewMesh);
			if (PickupMesh == NULL || PickupMesh->GetClass() != NewMesh->GetClass() || PickupMesh->Materials != NewMesh->Materials ||
				(SkelTemplate != NULL && ((USkeletalMeshComponent*)PickupMesh)->SkeletalMesh != SkelTemplate->SkeletalMesh) ||
				(StaticTemplate != NULL && ((UStaticMeshComponent*)PickupMesh)->StaticMesh != StaticTemplate->StaticMesh))
			{
				if (PickupMesh != NULL)
				{
					PickupMesh->DetachFromParent();
					PickupMesh->UnregisterComponent();
				}
				PickupMesh = ConstructObject<UMeshComponent>(NewMesh->GetClass(), Pickup, NAME_None, RF_NoFlags, NewMesh);
				PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PickupMesh->AttachParent = NULL;
				PickupMesh->AttachChildren.Empty();
				PickupMesh->RelativeRotation = FRotator::ZeroRotator;
				PickupMesh->RelativeLocation.Z += MeshFloatHeight;
				if (!OverrideScale.IsZero())
				{
					PickupMesh->SetWorldScale3D(OverrideScale);
				}
				if (SkelTemplate != NULL)
				{
					((USkeletalMeshComponent*)PickupMesh)->bForceRefpose = true;
				}
				PickupMesh->RegisterComponent();
				PickupMesh->AttachTo(Pickup->GetRootComponent());
				PickupMesh->SetRelativeLocation(PickupMesh->GetRelativeTransform().GetLocation() - (PickupMesh->Bounds.Origin - PickupMesh->GetComponentToWorld().GetLocation()));
				// if there's a rotation component, set it up to rotate the pickup mesh
				TArray<URotatingMovementComponent*> RotationComps;
				Pickup->GetComponents<URotatingMovementComponent>(RotationComps);
				if (RotationComps.Num() > 0)
				{
					RotationComps[0]->SetUpdatedComponent(PickupMesh);
					RotationComps[0]->PivotTranslation = PickupMesh->Bounds.Origin - PickupMesh->GetComponentToWorld().GetLocation();
				}
			}
		}
	}
}

void AUTPickupInventory::InventoryTypeUpdated_Implementation()
{
	CreatePickupMesh(this, Mesh, InventoryType, FloatHeight);

	TakenSound = (InventoryType != NULL) ? TakenSound = InventoryType.GetDefaultObject()->PickupSound : GetClass()->GetDefaultObject<AUTPickupInventory>()->TakenSound;
}

void AUTPickupInventory::SetPickupHidden(bool bNowHidden)
{
	if (Mesh != NULL)
	{
		Mesh->SetHiddenInGame(bNowHidden);
		// if previously there was no InventoryType or no Mesh then the whole Actor might have been hidden
		SetActorHiddenInGame(false);
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