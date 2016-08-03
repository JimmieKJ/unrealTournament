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
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("VictimMessage"));
	YouWereKilledByPrefix = NSLOCTEXT("UTVictimMessage","YouWereKilledByPrefix","Killed by "); 
	YouWereKilledByPostfix = NSLOCTEXT("UTVictimMessage", "YouWereKilledByPostfix", "");
	RespawnedVictimText = NSLOCTEXT("UTVictimMessage", "RespawnedVictimText", "   ");
	bDrawAsDeathMessage = true;
	bDrawAtIntermission = false;
	FontSizeIndex = 1;
}

float UUTVictimMessage::GetLifeTime(int32 Switch) const
{
	return (Switch == 2) ? 0.2 : Blueprint_GetLifeTime(Switch);
}

FLinearColor UUTVictimMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

void UUTVictimMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if ((Switch == 1) || (Switch == 2))
	{
		Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
		return;
	}
	PrefixText = YouWereKilledByPrefix;
	PostfixText = YouWereKilledByPostfix;
	EmphasisText = RelatedPlayerState_1 ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty();
	AUTPlayerState* KillerPS = Cast<AUTPlayerState>(RelatedPlayerState_1);
	EmphasisColor = (KillerPS && KillerPS->Team) ? KillerPS->Team->TeamColor : FLinearColor::Red;
}

FText UUTVictimMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
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

	return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

