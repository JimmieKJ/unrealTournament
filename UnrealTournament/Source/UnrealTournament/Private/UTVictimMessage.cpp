// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTVictimMessage.h"
#include "GameFramework/DamageType.h"

UUTVictimMessage::UUTVictimMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bIsUnique = true;
	Lifetime = 6.0f;
	DrawColor = FColor(255, 0, 0, 255);

	FontSize = 2;
	MessageArea = 1;

	YouWereKilledByText = NSLOCTEXT("UTVictimMessage","YouWereKilledByText","You were killed by {Player1Name} with {WeaponName}");
}

FText UUTVictimMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	return GetDefault<UUTVictimMessage>()->YouWereKilledByText;
}

