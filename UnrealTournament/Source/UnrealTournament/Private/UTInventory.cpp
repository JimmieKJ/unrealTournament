// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UnrealNetwork.h"
#include "UTDroppedPickup.h"

AUTInventory::AUTInventory(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	SetReplicates(true);
	bOnlyRelevantToOwner = true;
	bReplicateInstigator = true;

	RespawnTime = 30.0f;

	RootComponent = PCIP.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false, false, false);
	PickupMesh = PCIP.CreateDefaultSubobject<UStaticMeshComponent, UStaticMeshComponent>(this, TEXT("PickupMesh0"), false, false, false);
	if (PickupMesh != NULL)
	{
		PickupMesh->AttachParent = RootComponent;
		PickupMesh->bAutoRegister = false;
	}

	DroppedPickupClass = AUTDroppedPickup::StaticClass();
}

void AUTInventory::PostInitProperties()
{
	Super::PostInitProperties();
	// attempt to set defaults for event early outs based on whether the class has implemented them
	// note that this only works for blueprints, C++ classes need to manually set
	if (Cast<UBlueprintGeneratedClass>(GetClass()) != NULL)
	{
		if (!bCallModifyDamageTaken)
		{
			static FName NAME_ModifyDamageTaken(TEXT("ModifyDamageTaken"));
			UFunction* Func = FindFunction(NAME_ModifyDamageTaken);
			bCallModifyDamageTaken = (Func != NULL && Func->Script.Num() > 0);
		}
		if (!bCallOwnerEvent)
		{
			static FName NAME_OwnerEvent(TEXT("OwnerEvent"));
			UFunction* Func = FindFunction(NAME_OwnerEvent);
			bCallOwnerEvent = (Func != NULL && Func->Script.Num() > 0);
		}
	}
}

UMeshComponent* AUTInventory::GetPickupMeshTemplate_Implementation(FVector& OverrideScale) const
{
	return PickupMesh;
}

void AUTInventory::AddOverlayMaterials_Implementation(AUTGameState* GS) const
{
}

void AUTInventory::Destroyed()
{
	if (UTOwner != NULL)
	{
		UTOwner->RemoveInventory(this);
	}

	Super::Destroyed();
}

void AUTInventory::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Instigator = NewOwner;
	SetOwner(NewOwner);
	UTOwner = NewOwner;
	PrimaryActorTick.AddPrerequisite(UTOwner, UTOwner->PrimaryActorTick);
	eventGivenTo(NewOwner, bAutoActivate);
	ClientGivenTo(bAutoActivate);
}

void AUTInventory::Removed()
{
	eventRemoved();

	if (UTOwner != NULL)
	{
		PrimaryActorTick.RemovePrerequisite(UTOwner, UTOwner->PrimaryActorTick);
	}

	ClientRemoved(); // must be first, since it won't replicate after Owner is lost

	Instigator = NULL;
	SetOwner(NULL);
	UTOwner = NULL;
	NextInventory = NULL;
}

void AUTInventory::CheckPendingClientGivenTo()
{
	if (bPendingClientGivenTo && Instigator != NULL)
	{
		bPendingClientGivenTo = false;
		ClientGivenTo_Implementation(bPendingAutoActivate);
	}
}
void AUTInventory::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	CheckPendingClientGivenTo();
}

void AUTInventory::ClientGivenTo_Implementation(bool bAutoActivate)
{
	if (Instigator == NULL || !Cast<AUTCharacter>(Instigator)->IsInInventory(this))
	{
		bPendingClientGivenTo = true;
		bPendingAutoActivate = bAutoActivate;
		GetWorld()->GetTimerManager().SetTimer(this, &AUTInventory::CheckPendingClientGivenTo, 0.1f, false);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(this, &AUTInventory::CheckPendingClientGivenTo);
		bPendingClientGivenTo = false;
		ClientGivenTo_Internal(bAutoActivate);
		eventClientGivenTo(bAutoActivate);
	}
}

void AUTInventory::ClientGivenTo_Internal(bool bAutoActivate)
{
	checkSlow(Instigator != NULL);
	SetOwner(Instigator);
	UTOwner = Cast<AUTCharacter>(Instigator);
	checkSlow(UTOwner != NULL);
	PrimaryActorTick.AddPrerequisite(UTOwner, UTOwner->PrimaryActorTick);
}

void AUTInventory::ClientRemoved_Implementation()
{
	if (UTOwner != NULL)
	{
		PrimaryActorTick.RemovePrerequisite(UTOwner, UTOwner->PrimaryActorTick);
	}
	eventClientRemoved();
	SetOwner(NULL);
	UTOwner = NULL;
	Instigator = NULL;
	NextInventory = NULL;
}

void AUTInventory::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTInventory, NextInventory, COND_OwnerOnly);
}

void AUTInventory::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	if (Role == ROLE_Authority)
	{
		APawn* FormerInstigator = Instigator;

		if (UTOwner != NULL)
		{
			UTOwner->RemoveInventory(this);
		}
		Instigator = NULL;
		SetOwner(NULL);
		if (DroppedPickupClass != NULL)
		{
			FActorSpawnParameters Params;
			Params.Instigator = FormerInstigator;
			AUTDroppedPickup* Pickup = GetWorld()->SpawnActor<AUTDroppedPickup>(DroppedPickupClass, StartLocation, TossVelocity.Rotation(), Params);
			if (Pickup != NULL)
			{
				Pickup->Movement->Velocity = TossVelocity;
				Pickup->SetInventory(this);
			}
			else
			{
				Destroy();
			}
		}
		else
		{
			Destroy();
		}
	}
}

bool AUTInventory::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	return false;
}

void AUTInventory::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AController* InstigatedBy, AActor* DamageCauser)
{
}

void AUTInventory::OwnerEvent_Implementation(FName EventName)
{
}