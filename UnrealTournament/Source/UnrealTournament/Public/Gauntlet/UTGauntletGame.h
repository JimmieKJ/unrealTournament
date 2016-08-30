// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFRoundGame.h"
#include "UTCTFScoring.h"
#include "UTGauntletGameState.h"
#include "UTGauntletGame.generated.h"

class AUTGauntletFlagDispenser;
class AUTGauntletGameState;

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletGame : public AUTCTFRoundGame
{
	GENERATED_UCLASS_BODY()
	
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void InitFlags() override;
	virtual void ResetFlags() override;

	virtual void FlagTeamChanged(uint8 NewTeamIndex);
	
	virtual bool IsTeamStillAlive(uint8 TeamNum);
	bool CanFlagTeamSwap(uint8 NewTeamNum);

	void GameObjectiveInitialized(AUTGameObjective* Obj);

	virtual void InitRound();

	virtual int32 GetFlagCapScore() override
	{
		return 1;
	}

	virtual void PreInitializeComponents() override;

	virtual void FlagsAreReady();

	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer) override;


protected:

	UPROPERTY()
	AUTGauntletGameState* GauntletGameState;

	// How long does a flag have to sit idle for it to return to the neutral position.  Use ?FlagSwapTime=x to set.  Set to 0 to be instantly pick-up-able 
	UPROPERTY()
	int32 FlagSwapTime;
};