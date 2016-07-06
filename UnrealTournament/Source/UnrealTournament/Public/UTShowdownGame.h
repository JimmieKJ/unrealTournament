// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTDuelGame.h"

#include "UTShowdownGame.generated.h"

USTRUCT()
struct FTimedPlayerPing
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY()
		AUTCharacter* Char;
	UPROPERTY()
		uint8 VisibleTeamNum;
	UPROPERTY()
		float TimeRemaining;

	FTimedPlayerPing()
		: Char(nullptr), VisibleTeamNum(255), TimeRemaining(0.0f)
	{}
	FTimedPlayerPing(AUTCharacter* InChar, uint8 InTeam, float InTimeRemaining)
		: Char(InChar), VisibleTeamNum(InTeam), TimeRemaining(InTimeRemaining)
	{}
};

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

	virtual void BeginPlay() override;
	virtual void HandleMatchHasStarted() override;
	virtual void StartNewRound();
	virtual void HandleCountdownToBegin() override;
	virtual void UpdateSkillRating() override;
	virtual FString GetRankedLeagueName() override;

	// experimental tiebreaker options
	// will be removed once we decide on a path
	UPROPERTY(config)
	bool bXRayBreaker; // both players see others through walls after 60 seconds
	UPROPERTY(config)
	bool bPowerupBreaker; // spawn super powerup at 60 seconds

	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTPickupInventory"))
	FStringClassReference PowerupBreakerPickupClass;
	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTTimedPowerup"))
	FStringClassReference PowerupBreakerItemClass;
	/** item that replaces superweapons (Redeemer, etc) */
	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTPickupInventory"))
	FStringClassReference SuperweaponReplacementPickupClass;
	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTInventory"))
	FStringClassReference SuperweaponReplacementItemClass;

	UPROPERTY()
	AUTPickupInventory* BreakerPickup;

	// true if map has UTPlayerStarts with AssociatedPickup filled in; if not we use slow fallback to try to figure out spawn auto select mechanics
	bool bAssociatedPickupsSet;
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

	/** elapsed time in the current round */
	UPROPERTY(BlueprintReadOnly)
	int32 RoundElapsedTime;

	/** team that won last round */
	UPROPERTY(BlueprintReadOnly)
	AUTTeamInfo* LastRoundWinner;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual bool CheckRelevance_Implementation(AActor* Other) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void ScoreDamage_Implementation(int32 DamageAmount, AUTPlayerState* Victim, AUTPlayerState* Attacker) override;
	virtual void ScoreTeamKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override
	{
		// we need to treat this the same way we treat normal kills
		ScoreKill(Killer, Other, KilledPawn, DamageType);
	}
	virtual void RestartPlayer(AController* aPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual float RatePlayerStart(APlayerStart* P, AController* Player) override;

	virtual void CheckGameTime() override;
	/** return player/team that wins if the game time expires
	 * this function is safe to call prior to the game actually ending (to show who 'would' win)
	 */
	virtual AInfo* GetTiebreakWinner(FName* WinReason = NULL) const;
	/** score round that ended by timelimit instead of by kill */
	virtual void ScoreExpiredRoundTime();
	virtual void CallMatchStateChangeNotify() override;
	virtual void DefaultTimer() override;
	class AUTPickupInventory* GetNearestItemToStart(APlayerStart* Start) const;
	/** select a PlayerStart during spawn selection for a player that failed to choose themselves */
	virtual APlayerStart* AutoSelectPlayerStart(AController* Requestor);

	virtual uint8 GetNumMatchesFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual int32 GetEloFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual void SetEloFor(AUTPlayerState* PS, bool bRankedSession, int32 NewEloValue, bool bIncrementMatchCount) override;

	// Creates the URL options for custom games
	virtual void CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps);

#if !UE_SERVER
	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps, int32 MinimumPlayers) override;
#endif

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = Game)
	void AddDamagePing(AUTCharacter* Char, uint8 VisibleToTeam, float Lifetime = 2.0f);

	/** used to show outlines of players for a short time */
	UPROPERTY()
	TArray<FTimedPlayerPing> DamagePings;
};
