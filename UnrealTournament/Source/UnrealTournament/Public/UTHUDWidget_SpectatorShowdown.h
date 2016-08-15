// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_Spectator.h"
#include "UTShowdownGameState.h"

#include "UTHUDWidget_SpectatorShowdown.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_SpectatorShowdown : public UUTHUDWidget_Spectator
{
	GENERATED_BODY()
public:
	UUTHUDWidget_SpectatorShowdown(const FObjectInitializer& OI)
		: Super(OI)
	{
		RoundBeginsText = NSLOCTEXT("UTHUD_Showdown", "RoundBegins", "Round begins in... {0}");
		PickingStartsText = NSLOCTEXT("UTHUD_Showdown", "PickingStarts", "Spawn selection begins in... {0}");
		SelectingSpawnText = NSLOCTEXT("UTHUD_Showdown", "SelectingSpawn", "Click on the spawn point you want to use.");
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SelectingSpawnText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText RoundBeginsText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText PickingStartsText;

	virtual FLinearColor GetMessageColor() const override
	{
		return FLinearColor::White;
	}

	virtual FText GetSpectatorMessageText(FText& ShortMessage) override
	{
		ShortMessage = FText::GetEmpty();
		AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(UTGameState);
		if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission)
		{
			if ((GS->SpawnSelector == NULL) && !GS->bFinalIntermissionDelay)
			{
				return FText::Format(PickingStartsText, FText::AsNumber(GS->IntermissionStageTime));
			}
			else if (UTHUDOwner && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && GS->SpawnSelector == UTHUDOwner->UTPlayerOwner->UTPlayerState)
			{
				return SelectingSpawnText;
			}
			else if (GS->bStartedSpawnSelection && GS->bFinalIntermissionDelay)
			{
				return FText::Format(RoundBeginsText, FText::AsNumber(GS->IntermissionStageTime));
			}
		}
		else
		{
			return Super::GetSpectatorMessageText(ShortMessage);
		}
		return FText::GetEmpty();
	}
};
