// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTTeamGameMode.h"
#include "UTCTFGameMode.generated.h"

namespace MatchState
{
	extern UNREALTOURNAMENT_API const FName MatchEnteringHalftime;		// Entering Halftime
	extern UNREALTOURNAMENT_API const FName MatchIsAtHalftime;			// The match has entered halftime
	extern UNREALTOURNAMENT_API const FName MatchExitingHalftime;		// Exiting Halftime
	extern UNREALTOURNAMENT_API const FName MatchEnteringSuddenDeath;	// The match is entering sudden death
	extern UNREALTOURNAMENT_API const FName MatchIsInSuddenDeath;		// The match is in sudden death
} 

UCLASS()
class UNREALTOURNAMENT_API AUTCTFGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** If true, CTF allow for a sudden death after overtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	uint32 bSuddenDeath:1;	

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	int32 OvertimeDuration;

	/**Holds the amount of time to give a flag carrier who has the flag out going in to half-time*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
	int32 AdvantageDuration;

	UFUNCTION(exec)
	void CheatScore();

	virtual void InitGameState();
	virtual void PreInitializeComponents();
	virtual void InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage );
	virtual void ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	virtual void CheckGameTime();
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);

	virtual void CallMatchStateChangeNotify() override;

	virtual void HandleEnteringHalftime();
	virtual void HandleHalftime();
	virtual void HandleExitingHalftime();
	virtual void HandleEnteringSuddenDeath();
	virtual void HandleSuddenDeath();
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
protected:

	virtual void HandleMatchHasStarted();

	UFUNCTION()
	virtual void HalftimeIsOver();

	UFUNCTION()
	virtual bool IsMatchInSuddenDeath();

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


