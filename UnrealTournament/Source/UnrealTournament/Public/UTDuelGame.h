// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamDMGameMode.h"

#include "UTDuelGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTDuelGame : public AUTTeamDMGameMode
{
	GENERATED_UCLASS_BODY()

	virtual bool CheckRelevance_Implementation(AActor* Other) override;
	virtual void InitGameState() override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PlayEndOfMatchMessage() override;
	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps) override;

	/** How long powerups last in Duel */
	UPROPERTY()
	float PowerupDuration;
};
