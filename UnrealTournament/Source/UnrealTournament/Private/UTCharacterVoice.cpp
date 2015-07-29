// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacterVoice.h"
#include "UTAnnouncer.h"

UUTCharacterVoice::UUTCharacterVoice(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	bIsStatusAnnouncement = false;
}

FText UUTCharacterVoice::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if (TauntMessages.Num() > Switch)
	{
		FFormatNamedArguments Args;
		Args.Add("PlayerName", FText::AsCultureInvariant(RelatedPlayerState_1->PlayerName));
		Args.Add("TauntMessage", TauntMessages[Switch].SpeechText);
		return FText::Format(NSLOCTEXT("UTCharacterVoice", "Taunt", "{PlayerName}: {TauntMessage}"), Args);
	}
	return FText::GetEmpty();
}

FName UUTCharacterVoice::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return NAME_Custom;
}

USoundBase* UUTCharacterVoice::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return (TauntMessages.Num() > Switch) ? TauntMessages[Switch].SpeechSound : NULL;
}

void UUTCharacterVoice::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
}

bool UUTCharacterVoice::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return true;
}

