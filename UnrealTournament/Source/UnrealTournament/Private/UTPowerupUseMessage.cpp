// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTPowerupUseMessage.h"
#include "GameFramework/LocalMessage.h"
#include "UTFlagRunGameState.h"

UUTPowerupUseMessage::UUTPowerupUseMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsUnique = true;
	bIsConsoleMessage = false;
	bIsStatusAnnouncement = false;
	bPlayDuringIntermission = false;
	Lifetime = 2.f;
	AnnouncementDelay = 0.f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));
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
	return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FName UUTPowerupUseMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	const AUTPlayerState* UTPS = Cast<AUTPlayerState>(RelatedPlayerState_1);
	if (UTPS)
	{
		AUTInventory* Powerup = UTPS->BoostClass->GetDefaultObject<AUTInventory>();
		if (Powerup)
		{
			return Powerup->AnnouncementName;
		}
	}

	return NAME_None;
}

void UUTPowerupUseMessage::PrecacheAnnouncements_Implementation(class UUTAnnouncer* Announcer) const 
{
	if (Announcer && Announcer->GetWorld())
	{
		AUTFlagRunGameState* UTGS = Cast<AUTFlagRunGameState>(Announcer->GetWorld()->GetGameState());
		if (UTGS)
		{
			UTGS->PrecacheAllPowerupAnnouncements(Announcer);
		}
	}
}

bool UUTPowerupUseMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return true;
}

float UUTPowerupUseMessage::GetAnnouncementDelay(int32 Switch)
{
	return 0.f;
}

bool UUTPowerupUseMessage::InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	return (GetClass() == OtherMessageClass);
}

float UUTPowerupUseMessage::GetAnnouncementPriority(int32 Switch) const
{
	return (0.5f);
}