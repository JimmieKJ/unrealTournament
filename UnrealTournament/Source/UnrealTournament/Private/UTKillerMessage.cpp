// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTKillerMessage.h"


UUTKillerMessage::UUTKillerMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bIsSpecial = true;
	bIsUnique = true;
	DrawColor = FColor(255, 255, 255, 255);

	FontSize = 2;
	MessageArea = 1;

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
