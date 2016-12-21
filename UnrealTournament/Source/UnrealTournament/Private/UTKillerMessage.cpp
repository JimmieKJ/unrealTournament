// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTKillerMessage.h"
#include "UTATypes.h"

UUTKillerMessage::UUTKillerMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Lifetime = 3.0;
	bIsUnique = true;
	bCombineEmphasisText = true;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("DeathMessage"));
	YouKilledPrefixText = NSLOCTEXT("UTKillerMessage", "YouKilledPrefixText", "You killed ");
	YouKilledPostfixText = NSLOCTEXT("UTKillerMessage", "YouKilledPostfixText", "");
	KillAssistedPrefixText = NSLOCTEXT("UTKillerMessage", "KillAssistedPrefixText", "Kill assist ");
	KillAssistedPostfixText = NSLOCTEXT("UTKillerMessage", "KillAssisted", "");
	SpecKilledText = NSLOCTEXT("UTKillerMessage", "SpecKilledText", "{Player1Name} killed ");
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
	if (Switch == -99)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Player1Name"), RelatedPlayerState_1 ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty());
		PrefixText = FText::Format(SpecKilledText, Args);
		PostfixText = FText::GetEmpty();
	}
	else
	{
		PrefixText = (Switch == 2) ? KillAssistedPrefixText : YouKilledPrefixText;
		PostfixText = (Switch == 2) ? KillAssistedPostfixText : YouKilledPostfixText;
	}
	EmphasisText = RelatedPlayerState_2 ? FText::FromString(RelatedPlayerState_2->PlayerName) : FText::GetEmpty();
	AUTPlayerState* VictimPS = Cast<AUTPlayerState>(RelatedPlayerState_2);
	EmphasisColor = (VictimPS && VictimPS->Team) ? VictimPS->Team->TeamColor : FLinearColor::Red;
}

FText UUTKillerMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FText UUTKillerMessage::CombineEmphasisText(int32 CombinedMessageIndex, FText CombinedEmphasisText, FText OriginalEmphasisText) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("FirstText"), CombinedEmphasisText);
	Args.Add(TEXT("SecondText"), OriginalEmphasisText);
	return FText::Format(NSLOCTEXT("UTLocalMessage", "CombinedEmphasisText", "{FirstText} & {SecondText}"), Args);
}

FText UUTKillerMessage::CombinePrefixText(int32 CombinedMessageIndex, FText OriginalPrefixText) const
{
	if (CombinedMessageIndex == -99)
	{
		return OriginalPrefixText;
	}
	return (CombinedMessageIndex != 2) ? YouKilledPrefixText : OriginalPrefixText;
}

FText UUTKillerMessage::CombineText(int32 CombinedMessageIndex, FText CombinedEmphasisText, FText OriginalCombinedText) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("FirstText"), CombinedEmphasisText);
	Args.Add(TEXT("FullText"), OriginalCombinedText);
	return FText::Format(NSLOCTEXT("UTLocalMessage", "CombinedFullText", "{FullText} & {FirstText}"), Args);
}

