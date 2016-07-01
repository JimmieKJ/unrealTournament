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

		EnemyTeamSpecialEarned = NSLOCTEXT("CTFGameMessage", "EnemyEarnedSpecialMove", "Enemy team has earned their power up!");
		BoostAvailable = NSLOCTEXT("CTFGameMessage", "BoostAvailable", "Your power up is available!");
		FontSizeIndex = 1;

		static ConstructorHelpers::FObjectFinder<USoundBase> EarnedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/EnemyBoostAvailable.EnemyBoostAvailable'"));
		EnemyEarnedBoostSound = EarnedSoundFinder.Object;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EarnedSpecialMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EnemyTeamSpecialEarned;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BoostAvailable;

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
			case 7: return EnemyTeamSpecialEarned; break;
			case 20: return BoostAvailable; break;
			default:
				return FText();
		}
	}

	virtual float GetLifeTime(int32 Switch) const override
	{
		if ((Switch == 7) || (Switch == 20))
		{
			return 1.f;
		}
		return Blueprint_GetLifeTime(Switch);

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