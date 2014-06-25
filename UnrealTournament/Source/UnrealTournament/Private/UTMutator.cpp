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