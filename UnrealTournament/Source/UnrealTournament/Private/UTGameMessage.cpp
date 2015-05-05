// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMessage.h"
#include "GameFramework/LocalMessage.h"


UUTGameMessage::UUTGameMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("GameMessages"));

	GameBeginsMessage = NSLOCTEXT("UTGameMessage","GameBeginsMessage","START!");
	OvertimeMessage = NSLOCTEXT("UTGameMessage","OvertimeMessage","OVERTIME");
	SuddenDeathMessage = NSLOCTEXT("UTGameMessage", "SuddenDeathMessage", "SUDDEN DEATH");
	CantBeSpectator = NSLOCTEXT("UTGameMessage", "CantBeSpectator", "You can not become a spectator!");
	CantBePlayer = NSLOCTEXT("UTGameMessage","CantBePlayer","Sorry, you can not become a player!");
	SwitchLevelMessage = NSLOCTEXT("UTGameMessage","SwitchLevelMessage","Loading....");
	NoNameChange = NSLOCTEXT("UTGameMessage","NoNameChange","You can not change your name.");
	BecameSpectator = NSLOCTEXT("UTGameMessage","BecameSpectator","You are now a spectator.");
	DidntMakeTheCut= NSLOCTEXT("UTGameMessage","DidntMakeTheCut","You didn't make the cut");
	YouAreOnBlue = NSLOCTEXT("UTGameMessage", "YouAreOnBlue", "You are on BLUE.");
	YouAreOnRed = NSLOCTEXT("UTGameMessage", "YouAreOnRed", "You are on RED.");
	Coronation = NSLOCTEXT("UTGameMessage", "Coronation","Coronation.");
	GameChanger = NSLOCTEXT("UTGameMessage", "GameChanger", "Game Changer!");


	bIsStatusAnnouncement = true;
}

bool UUTGameMessage::UseLargeFont(int32 MessageIndex) const
{
	return (MessageIndex == 0) || (MessageIndex == 1) || (MessageIndex == 7) || (MessageIndex == 9) || (MessageIndex == 10);
}

float UUTGameMessage::GetScaleInSize(int32 MessageIndex) const
{
	if ((MessageIndex >= 2) && (MessageIndex <= 6))
	{
		return 1.f;
	}
	return UseLargeFont(MessageIndex) ? 3.f : 4.f;
}

FLinearColor UUTGameMessage::GetMessageColor(int32 MessageIndex) const
{
	if (MessageIndex == 9)
	{
		return FLinearColor::Red;
	}
	else if (MessageIndex == 10)
	{
		return FLinearColor::Blue;
	}
	return FLinearColor::Yellow;
}

FText UUTGameMessage::GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const
{
	switch (Switch)
	{
		case 0:
			return GetDefault<UUTGameMessage>(GetClass())->GameBeginsMessage;
			break;
		case 1:
			return GetDefault<UUTGameMessage>(GetClass())->OvertimeMessage;
			break;
		case 2:
			return GetDefault<UUTGameMessage>(GetClass())->CantBeSpectator;
			break;
		case 3:
			return GetDefault<UUTGameMessage>(GetClass())->CantBePlayer;
			break;
		case 4:
			return GetDefault<UUTGameMessage>(GetClass())->SwitchLevelMessage;
			break;
		case 5:
			return GetDefault<UUTGameMessage>(GetClass())->NoNameChange;
			break;
		case 6:
			return GetDefault<UUTGameMessage>(GetClass())->BecameSpectator;
			break;
		case 7:
			return GetDefault<UUTGameMessage>(GetClass())->SuddenDeathMessage;
			break;
		case 8:
			return GetDefault<UUTGameMessage>(GetClass())->DidntMakeTheCut;
			break;
		case 9:
			return GetDefault<UUTGameMessage>(GetClass())->YouAreOnRed;
			break;
		case 10:
			return GetDefault<UUTGameMessage>(GetClass())->YouAreOnBlue;
			break;
		case 11:
			return GetDefault<UUTGameMessage>(GetClass())->Coronation;
			break;
		case 12:
			return GetDefault<UUTGameMessage>(GetClass())->GameChanger;
			break;
		default:
			return FText::GetEmpty();
	}
}

FName UUTGameMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	switch (Switch)
	{
		case 1: return TEXT("Overtime"); break;
		case 7: return TEXT("SuddenDeath"); break;
		case 9: return TEXT("YouAreOnRedTeam"); break;
		case 10: return TEXT("YouAreOnBlueTeam"); break;
	}
	return NAME_None;
}
