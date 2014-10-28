// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDMGameMode.h"

#include "UTDuelGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTDuelGame : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	virtual bool CheckRelevance_Implementation(AActor* Other) override;
	virtual void InitGameState() override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PlayEndOfMatchMessage() override;

	/** How long powerups last in Duel */
	UPROPERTY()
	float PowerupDuration;
};
