// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UnrealNetwork.h"

AUTInventory::AUTInventory(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bOnlyRelevantToOwner = true;
	bReplicateInstigator = true;
}

void AUTInventory::Destroyed()
{
	AUTCharacter* C = Cast<AUTCharacter>(Instigator);
	if (C != NULL)
	{
		C->RemoveInventory(this);
	}

	Super::Destroyed();
}

void AUTInventory::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Instigator = NewOwner;
	SetOwner(NewOwner);
	UTOwner = NewOwner;
	eventGivenTo(NewOwner, bAutoActivate);
	ClientGivenTo(bAutoActivate);
}

void AUTInventory::Removed()
{
	ClientRemoved(); // must be first, since it won't replicate after Owner is lost

	Instigator = NULL;
	SetOwner(NULL);
	UTOwner = NULL;
	eventRemoved();
}

void AUTInventory::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	if (bPendingClientGivenTo && Instigator != NULL)
	{
		bPendingClientGivenTo = false;
		ClientGivenTo_Implementation(bPendingAutoActivate);
	}
}

void AUTInventory::ClientGivenTo_Implementation(bool bAutoActivate)
{
	if (Instigator == NULL)
	{
		bPendingClientGivenTo = true;
		bPendingAutoActivate = bAutoActivate;
	}
	else
	{
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
}

void AUTInventory::ClientRemoved_Implementation()
{
	eventClientRemoved();
}

void AUTInventory::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME_CONDITION(AUTInventory, NextInventory, COND_OwnerOnly);
}