// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTKillerMessage.h"


UUTKillerMessage::UUTKillerMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	Lifetime=2.0;
	bIsSpecial = true;
	bIsUnique = true;
	MessageArea = FName(TEXT("DeathMessage"));
	YouKilledText = NSLOCTEXT("UTKillerMessage","YouKilledText","You killed {Player2Name}");
}

FText UUTKillerMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	if (RelatedPlayerState_2 == NULL || RelatedPlayerState_2->PlayerName == TEXT(""))
	{
		return FText::GetEmpty();
	}

	return GetDefault<UUTKillerMessage>(GetClass())->YouKilledText;
}
