// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTPowerupUseMessage.h"
#include "GameFramework/LocalMessage.h"

UUTPowerupUseMessage::UUTPowerupUseMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UUTPowerupUseMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(RelatedPlayerState_1);
	if (UTPS)
	{
		AUTInventory* Powerup = UTPS->BoostClass->GetDefaultObject<AUTInventory>();
		if (Powerup)
		{
			PrefixText = FText::GetEmpty();
			EmphasisText = UTPS ? FText::FromString(UTPS->PlayerName) : FText::GetEmpty();
			EmphasisColor = UTPS  && UTPS->Team && (UTPS->Team->TeamIndex == 1) ? FLinearColor::Blue : FLinearColor::Red;
			PostfixText = Powerup->NotifyMessage;
			return;
		}
	}

	
	Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FText UUTPowerupUseMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch (Switch)
	{
		case 21:
			return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}
	
	return Super::GetText(Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}


