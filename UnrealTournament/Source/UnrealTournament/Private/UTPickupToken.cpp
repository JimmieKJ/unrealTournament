// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameplayStatics.h"
#include "UTPickupToken.h"

AUTPickupToken::AUTPickupToken(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// This was the only way I could make clientside actors that I could destroy
	bReplicates = true;
	bNetTemporary = true;
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

bool AUTPickupToken::HasBeenPickedUpBefore()
{
	if (TokenUniqueID != NAME_None)
	{
		return UUTGameplayStatics::HasTokenBeenPickedUpBefore(GetWorld(), TokenUniqueID);
	}

	return false;
}

void AUTPickupToken::PickedUp()
{
	if (TokenUniqueID != NAME_None)
	{
		UUTGameplayStatics::TokenPickedUp(GetWorld(), TokenUniqueID);
	}
}

void AUTPickupToken::Revoke()
{
	if (TokenUniqueID != NAME_None)
	{
		UUTGameplayStatics::TokenRevoke(GetWorld(), TokenUniqueID);
	}
}