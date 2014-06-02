// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTKillerMessage.h"


UUTKillerMessage::UUTKillerMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	Lifetime=3.0;
	bIsSpecial = true;
	bIsUnique = true;
	MessageArea = FName(TEXT("DeathMessage"));
	YouKilledText = NSLOCTEXT("UTKillerMessage","YouKilledText","You killed {Player2Name}");
}

FText UUTKillerMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	return GetDefault<UUTKillerMessage>(GetClass())->YouKilledText;	
}
