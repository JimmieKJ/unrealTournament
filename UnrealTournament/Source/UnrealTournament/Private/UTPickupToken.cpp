// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameplayStatics.h"
#include "UTPickupToken.h"
#include "UObjectToken.h"
#include "MessageLog.h"
#include "MapErrors.h"

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

void AUTPickupToken::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		TokenUniqueID = FName(*FGuid::NewGuid().ToString());
	}
}

void AUTPickupToken::CheckForErrors()
{
	Super::CheckForErrors();

	for (TActorIterator<AUTPickupToken> It(GetWorld(), GetClass()); It; ++It)
	{
		if (*It != this && It->TokenUniqueID == TokenUniqueID)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("UniqueId"), FText::FromString(TokenUniqueID.ToString()));
			Arguments.Add(TEXT("OtherActorName"), FText::FromString(It->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("UTPickupToken", "TerminalVelocityWarning", "{ActorName} : Has same ID {UniqueId} as {OtherActorName}!"), Arguments)))
				->AddToken(FMapErrorToken::Create(FName(TEXT("PickupTokenNotUnique"))));
		}
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