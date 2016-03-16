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

		CapFlagMessage = NSLOCTEXT("CTFGameMessage", "TakeFlagToEnemy", "Get your flag to the enemy base!");
		PreventCapMessage = NSLOCTEXT("CTFGameMessage", "StopEnemyFlag", "Keep other team's flag out, and exhaust their lives");
		CapAndKillMessage = NSLOCTEXT("CTFGameMessage", "RCTFRules", "Capture or kill to win.");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CapFlagMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText PreventCapMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CapAndKillMessage;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		switch (Switch)
		{
		case 1: return CapFlagMessage; break;
		case 2: return PreventCapMessage; break;
		case 3: return CapAndKillMessage; break;
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
		return NAME_None;
	}

	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override
	{
	}
};