// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTVictoryMessage.h"
#include "GameFramework/LocalMessage.h"


UUTVictoryMessage::UUTVictoryMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bIsUnique = true;
	Lifetime = 6.0f;
	MessageArea = FName(TEXT("GameMessages"));

	bIsStatusAnnouncement = true;

	YouHaveWonText = NSLOCTEXT("UTVictoryMessage","YouHaveWonText","You Have Won The Match!");
	YouHaveLostText = NSLOCTEXT("UTVictoryMessage","YouHaveLostText","You Have Lost The Match!");
	BlueTeamWinsText = NSLOCTEXT("UTVictoryMessage", "BlueTeamWinsText", "Blue Team Wins The Match!");
	RedTeamWinsText = NSLOCTEXT("UTVictoryMessage", "RedTeamWinsText", "Red Team Wins The Match!");
}

FLinearColor UUTVictoryMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::Yellow;
}

// @TODO FIXMESTEVE why not passing playerstates here too?
FName UUTVictoryMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	if (Switch == 2)
	{
		//return FName(TEXT("HumiliatingDefeat"));
	}
	else if (Switch == 3)
	{
		return FName(TEXT("FlawlessVictory"));
	}

	const AUTTeamInfo* WinningTeam = Cast<AUTTeamInfo>(OptionalObject);
	if (WinningTeam)
	{
		if (WinningTeam->TeamIndex == 0)
		{
			return FName(TEXT("RedTeamWinsMatch"));
		}
		else
		{
			return FName(TEXT("BlueTeamWinsMatch"));
		}
	}
	else
	{
		return (Switch == 1) ? FName(TEXT("WonMatch")) : FName(TEXT("LostMatch"));
	}
}

FText UUTVictoryMessage::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	AUTTeamInfo* WinningTeam = Cast<AUTTeamInfo>(OptionalObject);
	if (WinningTeam)
	{
		if (WinningTeam->TeamIndex == 0)
		{
			return GetDefault<UUTVictoryMessage>(GetClass())->RedTeamWinsText;
		}
		else
		{
			return GetDefault<UUTVictoryMessage>(GetClass())->BlueTeamWinsText;
		}
	}
	if (RelatedPlayerState_1 == RelatedPlayerState_2)
	{
		return GetDefault<UUTVictoryMessage>(GetClass())->YouHaveWonText;
	}
	else
	{
		return GetDefault<UUTVictoryMessage>(GetClass())->YouHaveLostText;
	}
}


