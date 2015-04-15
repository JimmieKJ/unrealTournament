// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamDMGameMode.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTTeamDMGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** whether to reduce team score for team kills */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = TeamDM)
	bool bScoreTeamKills;
	/** whether to reduce team score for suicides */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = TeamDM)
	bool bScoreSuicides;

	virtual void ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual AUTPlayerState* IsThereAWinner(uint32& bTied);
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void UpdateSkillRating() override;
};
