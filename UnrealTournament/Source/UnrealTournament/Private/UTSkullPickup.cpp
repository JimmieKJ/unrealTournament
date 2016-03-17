// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTSkullPickup.h"
#include "UTWorldSettings.h"

AUTSkullPickup::AUTSkullPickup(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	BotDesireability = 1.f;
	InitialLifeSpan = 20.0f;
	NetUpdateFrequency = 1.0f;
}

void AUTSkullPickup::SetInventory(AUTInventory* NewInventory)
{
}

bool AUTSkullPickup::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	return Super::AllowPickupBy_Implementation(Other, bDefaultAllowPickup);
}

void AUTSkullPickup::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(Target);
	if (UTChar)
	{
		// need skull pickup message
		/*
		if (Cast<APlayerController>(Target->GetController())
		{
			Cast<APlayerController>(Target->GetController())->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, NULL, NULL, Inventory->GetClass());
		}
		*/
		if (SkullTeamIndex == 1)
		{
			UTChar->BlueSkullCount++;
		}
		else
		{
			UTChar->RedSkullCount++;
		}
	}
}

USoundBase* AUTSkullPickup::GetPickupSound_Implementation() const
{
	return SkullPickupSound;
}

void AUTSkullPickup::PlayTakenEffects_Implementation(APawn* TakenBy)
{
	USoundBase* PickupSound = GetPickupSound();
	if (PickupSound != NULL)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), PickupSound, TakenBy, SRT_All, false, GetActorLocation(), NULL, NULL, false);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (SkullPickupEffect != NULL)
		{
			AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
			if (WS == NULL || WS->EffectIsRelevant(this, GetActorLocation(), true, false, 10000.0f, 1000.0f))
			{
				UGameplayStatics::SpawnEmitterAtLocation(this, SkullPickupEffect, GetActorLocation(), GetActorRotation());
			}
		}
	}
}

void AUTSkullPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

float AUTSkullPickup::BotDesireability_Implementation(APawn* Asker, float PathDistance)
{
	// make sure Asker can actually get here before the pickup times out
	float LifeSpan = GetLifeSpan();
	if (LifeSpan > 0.f)
	{
		ACharacter* C = Cast<ACharacter>(Asker);
		if ((C == nullptr) || (PathDistance / C->GetCharacterMovement()->MaxWalkSpeed > LifeSpan))
		{
			return 0.f;
		}
	}
	return BotDesireability;
}

float AUTSkullPickup::DetourWeight_Implementation(APawn* Asker, float PathDistance)
{
	return (PathDistance < 2000.0f) ? BotDesireability_Implementation(Asker, PathDistance) : 0.f;
}