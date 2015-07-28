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
	return (TauntMessages.Num() > Switch) ? TauntMessages[Switch].SpeechText : FText::GetEmpty();
}

FName UUTCharacterVoice::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return NAME_Custom;
}

USoundBase* UUTCharacterVoice::GetAnnouncementSound_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return (TauntMessages.Num() > Switch) ? TauntMessages[Switch].SpeechSound : NULL;
}


