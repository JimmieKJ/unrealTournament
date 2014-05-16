// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTVictoryMessage.h"
#include "GameFramework/LocalMessage.h"


UUTVictoryMessage::UUTVictoryMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	YouHaveWonText = NSLOCTEXT("UTVictoryMessage","YouHaveWonText","You Have Won The Match!");
	YouHaveLostText = NSLOCTEXT("UTVictoryMessage","YouHaveLostText","You Have Lost The Match!");
}

FText UUTVictoryMessage::GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const
{
	switch (Switch)
	{
		case 0:
			return GetDefault<UUTVictoryMessage>(GetClass())->YouHaveWonText;
			break;
		case 1:
			return GetDefault<UUTVictoryMessage>(GetClass())->YouHaveLostText;
			break;
		default:
			return FText::GetEmpty();
	}
}
