// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFRoundGame.h"
#include "UTGauntletGame.generated.h"

class AUTGauntletFlagBase;
class AUTGauntletGameState;

USTRUCT()
struct FGauntletTeamInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<AUTPlayerState*> PlayerRespawnQue;

	UPROPERTY()
	float RespawnTimer;

	UPROPERTY()
	TArray<AUTGauntletFlagBase*> FlagBases;
};

class AUTGauntletFlagBase;

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletGame : public AUTCTFRoundGame
{
	GENERATED_UCLASS_BODY()
	
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;

	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	virtual void HandleFlagCapture(AUTPlayerState* Holder) override;

	virtual void HandleMatchHasStarted();

	virtual void FlagTeamChanged(uint8 NewTeamIndex);
	virtual void RestartPlayer(AController* aPlayer);
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;

	virtual bool IsTeamStillAlive(uint8 TeamNum);
	bool CanFlagTeamSwap(uint8 NewTeamNum);
	virtual void HandleMatchIntermission();

	virtual void DiscardInventory(APawn* Other, AController* Killer);
	virtual void BroadcastVictoryConditions();

protected:

	UPROPERTY()
	AUTGauntletGameState* GauntletGameState;

	UPROPERTY()
	int32 FlagSwapTime;

	/** Holds important information about both teams including a list of associated flag bases and the respawn que */
	UPROPERTY(BlueprintReadOnly)
	TArray<FGauntletTeamInfo> TeamInfo;

	AUTGauntletFlagBase* FlagDispenser;

	virtual void RoundReset();

	void SpawnInitalFlag();


};