// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDuelGame.h"

#include "UTShowdownGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTShowdownGame : public AUTDuelGame
{
	GENERATED_BODY()
protected:
	/** used to deny RestartPlayer() except for our forced spawn at round start */
	bool bAllowPlayerRespawns;

	virtual void HandleMatchIntermission();
	UFUNCTION()
	virtual void StartIntermission();

	virtual void HandleMatchHasStarted() override;
	virtual void StartNewRound();

public:
	AUTShowdownGame(const FObjectInitializer& OI);

	/** extra health added to players */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 ExtraHealth;

	/** time in seconds for players to choose spawn points */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 SpawnSelectionTime;

	/** players that still need to pick their spawn points after current player */
	UPROPERTY(BlueprintReadOnly)
	TArray<AUTPlayerState*> RemainingPicks;

	/** team that won last round */
	UPROPERTY(BlueprintReadOnly)
	AUTTeamInfo* LastRoundWinner;

	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void RestartPlayer(AController* aPlayer) override
	{
		if (bAllowPlayerRespawns)
		{
			Super::RestartPlayer(aPlayer);
		}
	}
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void CheckGameTime() override;
	virtual void CallMatchStateChangeNotify() override;
	virtual void DefaultTimer() override;
};
