// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTVictimMessage.h"
#include "GameFramework/DamageType.h"

UUTVictimMessage::UUTVictimMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsUnique = true;
	Lifetime = 3.0f;
	MessageArea = FName(TEXT("DeathMessage"));
	StyleTag = FName(TEXT("Victim"));
	YouWereKilledByText = NSLOCTEXT("UTVictimMessage","YouWereKilledByText","You were killed by {Player1Name}"); //  with {WeaponName} -- Removed for now

	SuicideTexts.Add(NSLOCTEXT("UTKillerMessage","Suicide","You killed yourself."));
}


FLinearColor UUTVictimMessage::GetMessageColor(int32 MessageIndex) const
{
	return FLinearColor::White;
}

FText UUTVictimMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	switch(Switch)
	{
		case 0:	
			return GetDefault<UUTVictimMessage>()->YouWereKilledByText;

		case 1:
			const UUTVictimMessage* DefaultMessage = GetDefault<UUTVictimMessage>(GetClass());
			int Idx = int(float(DefaultMessage->SuicideTexts.Num()) * FMath::FRand());
			return DefaultMessage->SuicideTexts[Idx];
	}

	return FText::GetEmpty();
}

