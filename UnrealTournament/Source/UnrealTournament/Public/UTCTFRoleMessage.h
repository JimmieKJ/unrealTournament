// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTAnnouncer.h"
#include "UTCTFRoleMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTCTFRoleMessage : public UUTLocalMessage
{
	GENERATED_BODY()
public:
	UUTCTFRoleMessage(const FObjectInitializer& OI)
		: Super(OI)
	{
		bIsStatusAnnouncement = true;
		bIsPartiallyUnique = true;
		MessageArea = FName(TEXT("DeathMessage"));
		Lifetime = 8.f;

		CapFlagMessage = NSLOCTEXT("CTFGameMessage", "TakeFlagToEnemy", "Get your flag to the enemy base!");
		PreventCapMessage = NSLOCTEXT("CTFGameMessage", "StopEnemyFlag", "Keep other team's flag out, and exhaust their lives");
		CapAndKillMessage = NSLOCTEXT("CTFGameMessage", "RCTFRules", "Capture or kill to win.");
		EarnedSpecialMessage = NSLOCTEXT("CTFGameMessage", "EarnedSpecialMove", "{Player1Name} earned a special move for your team!");
		EnemyTeamSpecialEarned = NSLOCTEXT("CTFGameMessage", "EnemyEarnedSpecialMove", "The enemy team has earned a special move thanks to {Player1Name}!");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CapFlagMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText PreventCapMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CapAndKillMessage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EarnedSpecialMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EnemyTeamSpecialEarned;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		switch (Switch)
		{
			case 1: return CapFlagMessage; break;
			case 2: return CapFlagMessage; break;
			case 3: return PreventCapMessage; break;
			case 4: return PreventCapMessage; break;
			case 5: return CapAndKillMessage; break;
			case 6: return EarnedSpecialMessage; break;
			case 7: return EnemyTeamSpecialEarned; break;
			default:
				return FText();
		}
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}

	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return true;
	}

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		switch (Switch)
		{
			case 1: return TEXT("RedTeamOnOffense"); break;
			case 2: return TEXT("BlueTeamOffence"); break;
			case 3: return TEXT("RedTeamOnOffense"); break;
			case 4: return TEXT("BlueTeamOffence"); break;
		}
		return NAME_None;
	}

	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override
	{
		Announcer->PrecacheAnnouncement(TEXT("RedTeamOnOffense"));
		Announcer->PrecacheAnnouncement(TEXT("BlueTeamOffence"));
	}

	virtual float GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		return 0.1f;
	}

};