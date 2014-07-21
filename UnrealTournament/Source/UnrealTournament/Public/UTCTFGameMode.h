// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFGameMode.generated.h"

UCLASS()
class AUTCTFGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** If true, CTF will use old school CTF end-game rules where ScoreLimit needs to be hit and there is just 1 half */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 bOldSchool:1;	

	/** Holds the # of teams expected for this version of CTF.  This isn't a command line switch rather a var that can be subclassed or blueprinted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint8 MaxNumberOfTeams;

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category=CTF)
	AUTCTFGameState* CTFGameState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 HalftimeDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 OvertimeDuration;


	virtual void InitGameState();

	virtual void InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage );
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason);
	virtual bool CheckScore(AUTPlayerState* Scorer);
	virtual bool IsAWinner(AUTPlayerController* PC);
	virtual void CheckGameTime();
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);
	virtual void StartHalftime();
	virtual void FreezePlayers();
	virtual void FocusOnBestPlayer();
	virtual void RestartPlayer(AController* aPlayer);

protected:

	UFUNCTION()
	virtual void HalftimeIsOver();

};


