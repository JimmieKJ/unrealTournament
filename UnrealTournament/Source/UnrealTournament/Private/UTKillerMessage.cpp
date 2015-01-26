// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTKillerMessage.h"

UUTKillerMessage::UUTKillerMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Lifetime=3.0;
	bIsSpecial = true;
	bIsUnique = true;
	MessageArea = FName(TEXT("DeathMessage"));
	StyleTag = FName(TEXT("Killer"));
	YouKilledText = NSLOCTEXT("UTKillerMessage","YouKilledText","You killed {Player2Name}");
}

bool UUTKillerMessage::UseLargeFont(int32 MessageIndex) const
{
	return false;
}

FLinearColor UUTKillerMessage::GetMessageColor(int32 MessageIndex) const
{
	return FLinearColor::Red;
}

FText UUTKillerMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	return GetDefault<UUTKillerMessage>(GetClass())->YouKilledText;	
}
