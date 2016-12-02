// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMessage.h"
#include "GameFramework/LocalMessage.h"


UUTGameMessage::UUTGameMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("GameMessages"));
	GameBeginsMessage = NSLOCTEXT("UTGameMessage","GameBeginsMessage","START!");
	OvertimeMessage = NSLOCTEXT("UTGameMessage","OvertimeMessage","OVERTIME");
	CantBeSpectator = NSLOCTEXT("UTGameMessage", "CantBeSpectator", "You can not become a spectator!");
	CantBePlayer = NSLOCTEXT("UTGameMessage","CantBePlayer","Sorry, you can not become a player!");
	SwitchLevelMessage = NSLOCTEXT("UTGameMessage","SwitchLevelMessage","Loading....");
	NoNameChange = NSLOCTEXT("UTGameMessage","NoNameChange","You can not change your name.");
	BecameSpectator = NSLOCTEXT("UTGameMessage","BecameSpectator","You are now a spectator.");
	DidntMakeTheCut= NSLOCTEXT("UTGameMessage","DidntMakeTheCut","You didn't make the cut");
	YouAreOn = NSLOCTEXT("UTGameMessage", "YouAreOn", "You are on ");
	RedTeamName = NSLOCTEXT("UTGameMessage", "RedTeamName", "RED");
	BlueTeamName = NSLOCTEXT("UTGameMessage", "BlueTeamName", "BLUE");
	KickVote = NSLOCTEXT("UTGameMessage", "KickVote", "{Player1Name} voted to kick {Player2Name}");
	NotEnoughMoney = NSLOCTEXT("UTGameMessage", "NotEnoughMoney", "{Player1Name}, you lack the funds to buy it.");
	PotentialSpeedHack = NSLOCTEXT("UTGameMessage", "Speedhack", "Server or network hitching.");
	OnDeck = NSLOCTEXT("UTGameMessage", "MatchStarting", "The match is starting.");
	WeaponLocked = NSLOCTEXT("UTGameMessage", "WeaponLocked", "Cannot unequip this weapon until it is fired.");
	bIsStatusAnnouncement = true;
	bPlayDuringInstantReplay = false;
}

int32 UUTGameMessage::GetFontSizeIndex(int32 MessageIndex) const
{
	return ((MessageIndex == 0) || (MessageIndex == 1) || (MessageIndex == 7) || (MessageIndex == 9) || (MessageIndex == 10) || (MessageIndex == 16)) ? 2 : 1;
}

float UUTGameMessage::GetScaleInSize_Implementation(int32 MessageIndex) const
{
	if (((MessageIndex >= 2) && (MessageIndex <= 6)) || (MessageIndex == 99))
	{
		return 1.f;
	}
	return (GetFontSizeIndex(MessageIndex) > 1) ? 3.f : 4.f;
}

FLinearColor UUTGameMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

float UUTGameMessage::GetLifeTime(int32 MessageIndex) const
{
	if ((MessageIndex >= 9) && (MessageIndex <= 10))
	{
		return 8.f;
	}
	return GetDefault<UUTLocalMessage>(GetClass())->Lifetime;
}

void UUTGameMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if ((Switch == 9) || (Switch == 10))
	{
		PrefixText = YouAreOn;
		PostfixText = FText::GetEmpty();
		EmphasisText = (Switch== 9) ? RedTeamName : BlueTeamName;
		EmphasisColor = (Switch == 9) ? FLinearColor::Red : FLinearColor::Blue;
		return;
	}
	Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
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
		case 8:
			return GetDefault<UUTGameMessage>(GetClass())->DidntMakeTheCut;
			break;
		case 9:
			return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
			break;
		case 10:
			return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
			break;
		case 13:
			return GetDefault<UUTGameMessage>(GetClass())->KickVote;
		case 14:
			return GetDefault<UUTGameMessage>(GetClass())->NotEnoughMoney;
		case 15:
			return GetDefault<UUTGameMessage>(GetClass())->PotentialSpeedHack;
		case 16:
			return GetDefault<UUTGameMessage>(GetClass())->OnDeck;
		case 99:
			return GetDefault<UUTGameMessage>(GetClass())->WeaponLocked;
		default:
			return FText::GetEmpty();
	}
}

FName UUTGameMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch (Switch)
	{
		case 1: return TEXT("Overtime"); break;
		case 9: return TEXT("YouAreOnRedTeam"); break;
		case 10: return TEXT("YouAreOnBlueTeam"); break;
		case 16: return TEXT("TheMatchIsStarting"); break;
	}
	return NAME_None;
}
