// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTVictimMessage.h"
#include "UTDamageType.h"

UUTVictimMessage::UUTVictimMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsUnique = true;
	Lifetime = 2.6f;
	MessageArea = FName(TEXT("DeathMessage"));
	StyleTag = FName(TEXT("Victim"));
	YouWereKilledByText = NSLOCTEXT("UTVictimMessage","YouWereKilledByText","Killed by {Player1Name}"); 
	RespawnedVictimText = NSLOCTEXT("UTVictimMessage", "RespawnedVictimText", "   ");
}

bool UUTVictimMessage::UseLargeFont(int32 MessageIndex) const
{
	return false;
}

FLinearColor UUTVictimMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

FText UUTVictimMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
/*	if (RelatedPlayerState_2 && Cast<AController>(RelatedPlayerState_2) && Cast<AController>(RelatedPlayerState_2)->GetPawn())
	{
		return FText::GetEmpty();
	}*/
	if (Switch == 1)
	{
		UClass* DamageTypeClass = Cast<UClass>(OptionalObject);
		const UUTDamageType* DmgType = DamageTypeClass ? Cast<UUTDamageType>(DamageTypeClass->GetDefaultObject()) : NULL;
		if (!DmgType)
		{
			DmgType = Cast<UUTDamageType>(UUTDamageType::StaticClass()->GetDefaultObject());
		}
		return DmgType->SelfVictimMessage;
	}
	else if (Switch == 2)
	{
		return RespawnedVictimText;
	}
	return GetDefault<UUTVictimMessage>()->YouWereKilledByText;
}

