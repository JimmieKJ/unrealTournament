// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTTeamGameMode.h"
#include "UTCTFBaseGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFBaseGame : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
	AUTCTFGameState* CTFGameState;

	/** Class of GameState associated with this GameMode. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadWrite, Category = Classes)
		TSubclassOf<class AUTCTFScoring> CTFScoringClass;

	/** Handles individual player scoring */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		AUTCTFScoring* CTFScoring;

	TAssetSubclassOf<AUTWeapon> TranslocatorObject;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PreInitializeComponents() override;
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);

	virtual void EndGame(AUTPlayerState* Winner, FName Reason);
	virtual void SetEndGameFocus(AUTPlayerState* Winner);
	virtual void PlacePlayersAroundFlagBase(int32 TeamNum);

	UFUNCTION(exec)
		void CheatScore();

	/** Admin control for restarting competitive matches with appropriate status. */
	UFUNCTION(exec)
		void SetRedScore(int32 NewScore);

	UFUNCTION(exec)
		void SetBlueScore(int32 NewScore);

	void AddCaptureEventToReplay(AUTPlayerState* Holder, AUTTeamInfo* Team);
	void AddReturnEventToReplay(AUTPlayerState* Returner, AUTTeamInfo* Team);
	void AddDeniedEventToReplay(APlayerState* KillerPlayerState, AUTPlayerState* Holder, AUTTeamInfo* Team);

protected:

	virtual void ScoreDamage_Implementation(int32 DamageAmount, AController* Victim, AController* Attacker) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;

	virtual void UpdateSkillRating() override;

#if !UE_SERVER
public:
	virtual void BuildScoreInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList);
#endif
};