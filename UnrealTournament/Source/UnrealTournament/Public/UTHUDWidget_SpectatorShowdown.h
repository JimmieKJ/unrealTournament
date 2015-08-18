// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
		SelectingSpawnText = NSLOCTEXT("UTHUD_Showdown", "SelectingSpawn", "{0} is picking... {1}");
		RoundBeginsText = NSLOCTEXT("UTHUD_Showdown", "RoundBegins", "Round begins in... {0}");
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SelectingSpawnText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText RoundBeginsText;

	virtual FText GetSpectatorMessageText(bool &bShortMessage) override
	{
		AUTShowdownGameState* GS = Cast<AUTShowdownGameState>(UTGameState);
		if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission)
		{
			// draw whose turn it is
			if (GS->SpawnSelector != NULL)
			{
				return FText::Format(SelectingSpawnText, FText::FromString(GS->SpawnSelector->PlayerName), FText::AsNumber(GS->IntermissionStageTime));
			}
			else
			{
				return FText::Format(RoundBeginsText, FText::AsNumber(GS->IntermissionStageTime));
			}
		}
		else
		{
			return Super::GetSpectatorMessageText(bShortMessage);
		}
	}
};