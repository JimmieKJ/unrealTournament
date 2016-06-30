// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTRedeemerLaunchAnnounce.h"
#include "GameFramework/LocalMessage.h"

UUTRedeemerLaunchAnnounce::UUTRedeemerLaunchAnnounce(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsSpecial = true;
	bIsUnique = true;
	bIsConsoleMessage = false;
	bIsStatusAnnouncement = false;
	Lifetime = 2.f;
	AnnouncementDelay = 0.f;
}

FName UUTRedeemerLaunchAnnounce::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch(Switch)
	{
	case 0: return TEXT("RZE_MissileIncoming"); break;
	case 1: return TEXT("RZE_LaunchConfirmed"); break;
	}
	return NAME_None;
}

void UUTRedeemerLaunchAnnounce::PrecacheAnnouncements_Implementation(class UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i < 2; i++)
	{
		FName SoundName = GetAnnouncementName(i, NULL, NULL, NULL);
		if (SoundName != NAME_None)
		{
			Announcer->PrecacheAnnouncement(SoundName);
		}
	}
}

bool UUTRedeemerLaunchAnnounce::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return true;
}
