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
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("MultiKill"));
		Lifetime = 8.f;

		CapFlagMessage = NSLOCTEXT("CTFGameMessage", "TakeFlagToEnemy", "Get your flag to the enemy base");
		PreventCapMessage = NSLOCTEXT("CTFGameMessage", "StopEnemyFlag", "Keep other team's flag out");
		ExhaustLivesMessage = NSLOCTEXT("CTFGameMessage", "ExhaustLivesMessage", "Get your flag to the enemy base or exhaust their lives.");
		KeepLivesMessage = NSLOCTEXT("CTFGameMessage", "KeepLivesMessage", "Keep other team's flag out and don't run out of lives.");
		CapAndKillMessage = NSLOCTEXT("CTFGameMessage", "RCTFRules", "Capture or kill to win.");
		EnemyTeamSpecialEarned = NSLOCTEXT("CTFGameMessage", "EnemyEarnedSpecialMove", "Enemy team has earned their power up!");
		FontSizeIndex = 1;

		static ConstructorHelpers::FObjectFinder<USoundBase> EarnedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/EnemyBoostAvailable.EnemyBoostAvailable'"));
		EnemyEarnedBoostSound = EarnedSoundFinder.Object;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CapFlagMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText PreventCapMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CapAndKillMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText ExhaustLivesMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText KeepLivesMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EarnedSpecialMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EnemyTeamSpecialEarned;

	/** sound played when enemy team boost is earned */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		USoundBase* EnemyEarnedBoostSound;

	virtual void ClientReceive(const FClientReceiveData& ClientData) const override
	{
		Super::ClientReceive(ClientData);
		if (EnemyEarnedBoostSound && (ClientData.MessageIndex == 7))
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
			if (PC != NULL)
			{
				PC->UTClientPlaySound(EnemyEarnedBoostSound);
			}
		}
	}

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		switch (Switch)
		{
			case 1: return ExhaustLivesMessage; break;
			case 2: return CapFlagMessage; break;
			case 3: return KeepLivesMessage; break;
			case 4: return PreventCapMessage; break;
			case 5: return CapAndKillMessage; break;
			case 7: return EnemyTeamSpecialEarned; break;
			case 11: return ExhaustLivesMessage; break;
			case 12: return CapFlagMessage; break;
			default:
				return FText();
		}
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override
	{
		switch (Switch)
		{
			case 1: return TEXT("RedTeamOnOffense"); break;
			case 2: return TEXT("RedTeamOnOffense"); break;
			case 3: return TEXT("YourTeamIsNowDefending_01"); break;
			case 4: return TEXT("YourTeamIsNowDefending_01"); break;
			case 11: return TEXT("BlueTeamOffence"); break;
			case 12: return TEXT("BlueTeamOffence"); break;
		}
		return NAME_None;
	}

	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override
	{
		Announcer->PrecacheAnnouncement(TEXT("RedTeamOnOffense"));
		Announcer->PrecacheAnnouncement(TEXT("BlueTeamOffence"));
		Announcer->PrecacheAnnouncement(TEXT("YourTeamIsNowDefending_01"));
	}

	virtual float GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		return 0.1f;
	}
};