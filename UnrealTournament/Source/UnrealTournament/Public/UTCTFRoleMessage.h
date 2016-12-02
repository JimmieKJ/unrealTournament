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
		bIsUnique = false;
		bIsStatusAnnouncement = true;
		bIsPartiallyUnique = true;
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("MultiKill"));
		Lifetime = 8.f;

		EnemyTeamSpecialEarned = NSLOCTEXT("CTFGameMessage", "EnemyEarnedSpecialMove", "Enemy team has earned their power up!");
		BoostAvailable = NSLOCTEXT("CTFGameMessage", "BoostAvailable", "Your power up is available!");
		FontSizeIndex = 1;
		AttackingName = TEXT("YouAreAttacking");
		DefendingName = TEXT("YouAreDefending");
		ChoosePowerupName = TEXT("SelectYourPowerupThisRound");

		static ConstructorHelpers::FObjectFinder<USoundBase> EarnedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/EnemyBoostAvailable.EnemyBoostAvailable'"));
		EnemyEarnedBoostSound = EarnedSoundFinder.Object;
		bPlayDuringInstantReplay = false;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EarnedSpecialMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText EnemyTeamSpecialEarned;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BoostAvailable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName AttackingName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName DefendingName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName ChoosePowerupName;

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

	bool InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const override
	{
		if (GetClass() == OtherAnnouncementInfo.MessageClass)
		{
			return (AnnouncementInfo.Switch != 3);
		}

		return Super::InterruptAnnouncement(AnnouncementInfo, OtherAnnouncementInfo);
	}

	bool CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override
	{
		return (GetClass() == OtherMessageClass) && (Switch == OtherSwitch);
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
			case 1: return AttackingName; break;
			case 2: return DefendingName; break;
			case 3: return ChoosePowerupName; break;
		}
		return NAME_None;
	}

	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override
	{
		Announcer->PrecacheAnnouncement(AttackingName);
		Announcer->PrecacheAnnouncement(DefendingName);
		Announcer->PrecacheAnnouncement(ChoosePowerupName);
	}

	virtual float GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		return 0.1f;
	}
};