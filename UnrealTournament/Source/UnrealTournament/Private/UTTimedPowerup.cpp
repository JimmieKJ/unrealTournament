// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTimedPowerup.h"
#include "UnrealNetwork.h"

AUTTimedPowerup::AUTTimedPowerup(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	TimeRemaining = 30.0f;
	RespawnTime = 90.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
}

void AUTTimedPowerup::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	ClientSetTimeRemaining(TimeRemaining);
	if (OverlayMaterial != NULL)
	{
		NewOwner->SetWeaponOverlay(OverlayMaterial, true);
	}
}
void AUTTimedPowerup::Removed()
{
	if (OverlayMaterial != NULL)
	{
		GetUTOwner()->SetWeaponOverlay(OverlayMaterial, false);
	}
	Super::Removed();
}

void AUTTimedPowerup::TimeExpired_Implementation()
{
	if (Role == ROLE_Authority)
	{
		if (GetUTOwner() != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), PowerupOverSound, GetUTOwner());
		}

		Destroy();
	}
}

bool AUTTimedPowerup::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	if (ContainedInv != NULL)
	{
		TimeRemaining += Cast<AUTTimedPowerup>(ContainedInv)->TimeRemaining;
	}
	else
	{
		TimeRemaining += GetClass()->GetDefaultObject<AUTTimedPowerup>()->TimeRemaining;
	}
	ClientSetTimeRemaining(TimeRemaining);
	return true;
}

void AUTTimedPowerup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetUTOwner() != NULL && TimeRemaining > 0.0f)
	{
		TimeRemaining -= DeltaTime;
		if (TimeRemaining <= 0.0f)
		{
			TimeExpired();
		}
	}
}

void AUTTimedPowerup::ClientSetTimeRemaining_Implementation(float InTimeRemaining)
{
	TimeRemaining = InTimeRemaining;
}