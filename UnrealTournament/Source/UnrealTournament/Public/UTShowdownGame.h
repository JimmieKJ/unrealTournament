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

	// experimental tiebreaker options
	// will be removed once we decide on a path
	UPROPERTY(config)
	bool bAlternateScoring; // 3 for win by kill, 2 and 1 for time expired
	UPROPERTY(config)
	bool bLowHealthRegen; // player low on health regenerates
	UPROPERTY(config)
	bool bXRayBreaker; // both players see others through walls after 60 seconds
	UPROPERTY(config)
	bool bPowerupBreaker; // spawn super powerup at 60 seconds
	UPROPERTY(config)
	bool bBroadcastPlayerHealth; // show both players' health on HUD at all times

	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTPickupInventory"))
	FStringClassReference PowerupBreakerPickupClass;
	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTTimedPowerup"))
	FStringClassReference PowerupBreakerItemClass;

	UPROPERTY()
	AUTPickupInventory* BreakerPickup;
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

#if !UE_SERVER
	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps) override;
#endif
};
