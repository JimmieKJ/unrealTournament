// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameplayStatics.h"
#include "UTPickupToken.h"

AUTPickupToken::AUTPickupToken(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void AUTPickupToken::PostLoad()
{
	Super::PostLoad();

	if (TokenUniqueID == NAME_None)
	{
		TokenUniqueID = FName(*FGuid::NewGuid().ToString());
	}
}

#if WITH_EDITOR
void AUTPickupToken::PostActorCreated()
{
	Super::PostActorCreated();

	if (TokenUniqueID == NAME_None)
	{
		TokenUniqueID = FName(*FGuid::NewGuid().ToString());
	}
}
#endif

bool AUTPickupToken::HasBeenPickedUpBefore_Implementation()
{
	if (TokenUniqueID != NAME_None)
	{
		return UUTGameplayStatics::HasTokenBeenPickedUpBefore(GetWorld(), TokenUniqueID);
	}

	return false;
}

void AUTPickupToken::PickedUp_Implementation()
{
	if (TokenUniqueID != NAME_None)
	{
		return UUTGameplayStatics::TokenPickedUp(GetWorld(), TokenUniqueID);
	}
}

void AUTPickupToken::Revoke_Implementation()
{
	if (TokenUniqueID != NAME_None)
	{
		return UUTGameplayStatics::TokenRevoke(GetWorld(), TokenUniqueID);
	}
}