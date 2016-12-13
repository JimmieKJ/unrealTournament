// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTAnnouncer.h"
#include "UTPvEGameMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTPvEGameMessage : public UUTLocalMessage
{
	GENERATED_BODY()
public:
	UUTPvEGameMessage(const FObjectInitializer& OI)
		: Super(OI)
	{
		bIsStatusAnnouncement = true;
		bIsPartiallyUnique = true;
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("GameMessages"));

		ReinforcementsMsg = NSLOCTEXT("PvEMessage", "Reinforcements", "Enemy Reinforcements Have Arrived!");
		ExtraLifeMsg = NSLOCTEXT("PvEMessage", "ExtraLife", "Extra Life Earned!");

		static ConstructorHelpers::FObjectFinder<USoundBase> ReinforcementsSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/EnemyRally.EnemyRally'"));
		ReinforcementsSound = ReinforcementsSoundFinder.Object;
		static ConstructorHelpers::FObjectFinder<USoundBase> ExtraLifeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_CTF_CaptureSound01.A_Gameplay_CTF_CaptureSound01'"));
		ExtraLifeSound = ExtraLifeSoundFinder.Object;
	}

	UPROPERTY(EditDefaultsOnly)
	FText ReinforcementsMsg;
	UPROPERTY(EditDefaultsOnly)
	FText ExtraLifeMsg;
	UPROPERTY(EditDefaultsOnly)
	USoundBase* ReinforcementsSound;
	UPROPERTY(EditDefaultsOnly)
	USoundBase* ExtraLifeSound;

	virtual void ClientReceive(const FClientReceiveData& ClientData) const
	{
		Super::ClientReceive(ClientData);
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != nullptr)
		{
			switch (ClientData.MessageIndex)
			{
				case 0:
					PC->UTClientPlaySound(ReinforcementsSound);
					break;
				case 1:
					PC->UTClientPlaySound(ExtraLifeSound);
					break;
				default:
					break;
			}
		}
	}

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		switch (Switch)
		{
			case 0:
				return ReinforcementsMsg;
			default:
				return FText();
		}
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}
};