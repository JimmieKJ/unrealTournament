// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTBaseScoring.h"
#include "UTTeamGameMode.h"
#include "UTCTFBaseGame.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTCTFBaseGame : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** Cached reference to the CTF game state */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
	AUTCTFGameState* CTFGameState;

	/** Class of GameState associated with this GameMode. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadWrite, Category = Classes)
		TSubclassOf<class AUTBaseScoring> CTFScoringClass;

	/** Handles individual player scoring */
	UPROPERTY(BlueprintReadOnly, Category = CTF)
		AUTBaseScoring* CTFScoring;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
		int32 IntermissionDuration;

	UPROPERTY(BlueprintReadOnly, Category = CTF)
		AUTTeamInfo* LastTeamToScore;

	/**Amount of score to give team for flag capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
		int32 FlagCapScore;

	virtual int32 GetFlagCapScore();

	UPROPERTY(transient)
		bool bPlacingPlayersAtIntermission;

	virtual int32 IntermissionTeamToView(AUTPlayerController* PC);

	TAssetSubclassOf<AUTWeapon> TranslocatorObject;

	UPROPERTY()
		TSubclassOf<AUTWeapon> TranslocatorClass;

	UPROPERTY(EditDefaultsOnly, Category = CTF)
		bool bGameHasTranslocator;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PreInitializeComponents() override;
	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);

	virtual void EndGame(AUTPlayerState* Winner, FName Reason);
	virtual void SetEndGameFocus(AUTPlayerState* Winner);
	virtual void PlacePlayersAroundFlagBase(int32 TeamNum, int32 FlagTeamNum);
	virtual bool SkipPlacement(AUTCharacter* UTChar);
	virtual void BroadcastCTFScore(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore = 0);

	UFUNCTION(exec)
		virtual void CheatScore();

	virtual int32 PickCheatWinTeam();

	/** Admin control for restarting competitive matches with appropriate status. */
	UFUNCTION(exec)
		void SetRedScore(int32 NewScore);

	UFUNCTION(exec)
		void SetBlueScore(int32 NewScore);

	void AddCaptureEventToReplay(AUTPlayerState* Holder, AUTTeamInfo* Team);
	void AddReturnEventToReplay(AUTPlayerState* Returner, AUTTeamInfo* Team);
	void AddDeniedEventToReplay(APlayerState* KillerPlayerState, AUTPlayerState* Holder, AUTTeamInfo* Team);
	virtual void ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason) override;
	virtual void CallMatchStateChangeNotify() override;
	virtual void HandleMatchIntermission();
	virtual void HandleExitingIntermission();
	virtual void CheckGameTime() override;
	virtual void HandleFlagCapture(AUTCharacter* HolderPawn, AUTPlayerState* Holder);
	virtual void RestartPlayer(AController* aPlayer) override;

	virtual uint8 GetNumMatchesFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual int32 GetEloFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual void SetEloFor(AUTPlayerState* PS, bool bRankedSession, int32 NewELoValue, bool bIncrementMatchCount) override;

protected:

	virtual void ScoreDamage_Implementation(int32 DamageAmount, AUTPlayerState* Victim, AUTPlayerState* Attacker) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;

	virtual void UpdateSkillRating() override;
	virtual FString GetRankedLeagueName() override;

#if !UE_SERVER
public:
	virtual void BuildScoreInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList);
#endif

public:
	virtual int32 GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* Instigator, UWorld* World) override;

};