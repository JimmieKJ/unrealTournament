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
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("DeathMessage"));
	StyleTag = FName(TEXT("Killer"));
	YouKilledPrefixText = NSLOCTEXT("UTKillerMessage","YouKilledPrefixText","You killed ");
	YouKilledPostfixText = NSLOCTEXT("UTKillerMessage", "YouKilledPostfixText", "");
	SpecKilledText = NSLOCTEXT("UTKillerMessage", "SpecKilledText", "{Player1Name} killed {Player2Name}");
	bDrawAsDeathMessage = true;
	bDrawAtIntermission = false;
	FontSizeIndex = 1;
}

FLinearColor UUTKillerMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

void UUTKillerMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if ((Switch == 1) || (Switch == 2))
	{
		Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
		return;
	}
	PrefixText = YouKilledPrefixText;
	PostfixText = YouKilledPostfixText;
	EmphasisText = RelatedPlayerState_2 ? FText::FromString(RelatedPlayerState_2->PlayerName) : FText::GetEmpty();
	AUTPlayerState* VictimPS = Cast<AUTPlayerState>(RelatedPlayerState_2);
	EmphasisColor = (VictimPS && VictimPS->Team) ? VictimPS->Team->TeamColor : FLinearColor::Red;
}

FText UUTKillerMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	if (Switch == -99)
	{
		return GetDefault<UUTKillerMessage>(GetClass())->SpecKilledText;
	}
	return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}
