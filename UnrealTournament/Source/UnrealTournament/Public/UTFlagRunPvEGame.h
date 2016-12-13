// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTFlagRunGame.h"
#include "UTMonster.h"

#include "UTFlagRunPvEGame.generated.h"

UCLASS()
class AUTFlagRunPvEGame : public AUTFlagRunGame
{
	GENERATED_BODY()
public:
	AUTFlagRunPvEGame(const FObjectInitializer& OI);

	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTMonster"))
	TArray<FStringClassReference> EditableMonsterTypes;
	UPROPERTY(BlueprintReadWrite)
	TArray<TSubclassOf<AUTMonster>> MonsterTypes;

	// temp vial replacement for energy pickups
	UPROPERTY()
	FStringClassReference VialReplacement;

	/** number of kills to gain an extra life (multiplied by number of lives gained so far) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 BaseKillsForExtraLife;

	/** amount of points available to spawn monsters */
	UPROPERTY(BlueprintReadWrite)
	int32 MonsterPointsRemaining;
	/** most expensive monster that can spawn at this point in the round */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 MonsterCostLimit;

	UPROPERTY(EditDefaultsOnly, Meta = (MetaClass = "UTInventory"))
	TArray<FStringClassReference> BoostPowerupTypes;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool CheckRelevance_Implementation(AActor* Other) override;
	virtual void StartMatch() override;

	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual bool ShouldBalanceTeams(bool bInitialTeam) const override
	{
		return false;
	}
	virtual bool CheckForWinner(AUTTeamInfo* ScoringTeam) override;
	virtual AUTBotPlayer* AddBot(uint8 TeamNum) override;
	virtual bool ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast) override;
	virtual void GiveDefaultInventory(APawn* PlayerPawn) override;
	virtual void AddMonsters(int32 MaxNum);
	UFUNCTION()
	virtual void EscalateMonsters();
	virtual bool ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void NotifyFirstPickup(AUTCarriedObject* Flag) override
	{}
	virtual void FindAndMarkHighScorer() override;
	virtual void HandleRollingAttackerRespawn(AUTPlayerState* OtherPS) override;
};