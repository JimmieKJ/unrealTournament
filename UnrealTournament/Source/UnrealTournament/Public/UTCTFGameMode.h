// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTTeamGameMode.h"
#include "UTCTFGameMode.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category=CTF)
	AUTCTFGameState* CTFGameState;

	/** Class of GameState associated with this GameMode. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadWrite, Category = Classes)
		TSubclassOf<class AUTCTFScoring> CTFScoringClass;

	/** Handles individual player scoring */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
	AUTCTFScoring* CTFScoring;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	int32 HalftimeDuration;

	/**Holds the amount of time to give a flag carrier who has the flag out going in to half-time*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
	int32 AdvantageDuration;

	TAssetSubclassOf<AUTWeapon> TranslocatorObject;

	UFUNCTION(exec)
	void CheatScore();

	/** Admin control for restarting competitive matches with appropriate status. */
	UFUNCTION(exec)
		void SetRedScore(int32 NewScore);

	UFUNCTION(exec)
		void SetBlueScore(int32 NewScore);

	UFUNCTION(exec)
		void SetRemainingTime(int32 RemainingSeconds);

	virtual void InitGameState();
	virtual void PreInitializeComponents();
	virtual void InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage );
	virtual void ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	virtual void CheckGameTime();
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);
	virtual void DefaultTimer() override;

	virtual void CallMatchStateChangeNotify() override;
	virtual float GetTravelDelay() override;

	virtual void HandleEnteringHalftime();
	virtual void HandleHalftime();
	virtual void HandleExitingHalftime();
	virtual void HandleEnteringOvertime();
	virtual void HandleMatchInOvertime() override;

	virtual void PlacePlayersAroundFlagBase(int32 TeamNum);

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player);

	virtual void EndGame(AUTPlayerState* Winner, FName Reason);
	virtual void SetEndGameFocus(AUTPlayerState* Winner);
	void BuildServerResponseRules(FString& OutRules);

	void AddCaptureEventToReplay(AUTPlayerState* Holder, AUTTeamInfo* Team);
	void AddReturnEventToReplay(AUTPlayerState* Returner, AUTTeamInfo* Team);
	void AddDeniedEventToReplay(APlayerState* KillerPlayerState, AUTPlayerState* Holder, AUTTeamInfo* Team);

	virtual void GetGood() override;

protected:

	virtual void HandleMatchHasStarted();

	UFUNCTION()
	virtual void HalftimeIsOver();

	virtual void ScoreDamage_Implementation(int32 DamageAmount, AController* Victim, AController* Attacker) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;

	// returns the team index of a team with advatage or < 0 if no team has one
	virtual uint8 TeamWithAdvantage();

	// Look to see if the team that had advantage still has it
	virtual bool CheckAdvantage();

	
	int32 RemainingAdvantageTime;

	virtual void EndOfHalf();

	virtual void UpdateSkillRating() override;

#if !UE_SERVER
public:
	virtual void BuildScoreInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList);
#endif


};


