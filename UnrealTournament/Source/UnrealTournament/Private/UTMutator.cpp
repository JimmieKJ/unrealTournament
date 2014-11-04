// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTMutator.h"

AUTMutator::AUTMutator(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.DoNotCreateDefaultSubobject("Sprite"))
{
}

void AUTMutator::Init_Implementation(const FString& Options)
{
}

void AUTMutator::ModifyLogin_Implementation(FString& Portal, FString& Options)
{
	if (NextMutator != NULL)
	{
		NextMutator->ModifyLogin(Portal, Options);
	}
}

void AUTMutator::PostPlayerInit_Implementation(AController* C)
{
	if (NextMutator != NULL)
	{
		NextMutator->PostPlayerInit(C);
	}
}

void AUTMutator::NotifyLogout_Implementation(AController* C)
{
	if (NextMutator != NULL)
	{
		NextMutator->NotifyLogout(C);
	}
}

bool AUTMutator::AlwaysKeep_Implementation(AActor* Other, bool& bPreventModify)
{
	return (NextMutator != NULL && NextMutator->AlwaysKeep(Other, bPreventModify));
}

bool AUTMutator::CheckRelevance_Implementation(AActor* Other)
{
	return (NextMutator == NULL || NextMutator->CheckRelevance(Other));
}

void AUTMutator::ModifyPlayer_Implementation(APawn* Other)
{
	if (NextMutator != NULL)
	{
		NextMutator->ModifyPlayer(Other);
	}
}

void AUTMutator::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	if (NextMutator != NULL)
	{
		NextMutator->ModifyDamage(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser, DamageType);
	}
}