// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamInterface.h"
#include "Stat.h"
#include "UTPlayerState.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPlayerState : public APlayerState, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

private:

	/** Instance of a stat manager that tracks gameplay/reward stats for this Player Controller */
	UPROPERTY()
	class UStatManager *StatManager;

public:

	/** player's team if we're playing a team game */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = NotifyTeamChanged, Category = PlayerState)
	class AUTTeamInfo* Team;

	/** Whether this player is waiting to enter match */
	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	uint32 bWaitingPlayer:1;

	/** Whether this player has confirmed ready to play */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 bReadyToPlay:1;

	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	float LastKillTime;
	/** current multikill level (1 = double, 2 = multi, etc)
	 * note that the value isn't reset until a new kill comes in
	 */
	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	int32 MultiKillLevel;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = PlayerState)
	int32 Spree;

	/** Kills by this player.  Not replicated but calculated client-side */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	int32 Kills;

	/** Can't respawn once out of lives */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 bOutOfLives:1;

	/** How many times associated player has died */
	UPROPERTY(BlueprintReadOnly, replicated, ReplicatedUsing = OnDeathsReceived, Category = PlayerState)
	int32 Deaths;

	/** How many times has the player captured the flag */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 FlagCaptures;

	/** How many times has the player returned the flag */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 FlagReturns;

	/** How many times has the player captured the flag */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 Assists;

	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	AUTPlayerState* LastKillerPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	bool bIsRconAdmin;

	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	bool bHasHighScore;

	// Player Stats 

	/** This is the unique ID for stats generation*/
	FString StatsID;

	// How long until this player can repawn.  It's not directly replicated to the clients instead it's set
	// locally via OnDeathsReceived.  It will be set to the value of "GameState.RespawnWaitTime"

	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	float RespawnTime;

	/** The currently held object */
	UPROPERTY(BlueprintReadOnly, replicated, ReplicatedUsing = OnCarriedObjectChanged, Category = PlayerState)
	class AUTCarriedObject* CarriedObject;

	UFUNCTION()
	void OnDeathsReceived();

	UFUNCTION(BlueprintNativeEvent)
	void NotifyTeamChanged();

	UFUNCTION()
	void OnCarriedObjectChanged();

	UFUNCTION()
	virtual void SetCarriedObject(AUTCarriedObject* NewCarriedObject);

	UFUNCTION()
	virtual void ClearCarriedObject(AUTCarriedObject* OldCarriedObject);


	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void SetWaitingPlayer(bool B);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void IncrementKills(TSubclassOf<UDamageType> DamageType, bool bEnemyKill);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void IncrementDeaths(TSubclassOf<UDamageType> DamageType, AUTPlayerState* KillerPlayerState);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void AdjustScore(int32 ScoreAdjustment);

	virtual void Tick(float DeltaTime) override;

	inline bool IsFemale()
	{
		return false; // TODO
	}

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRequestChangeTeam(uint8 NewTeamIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRecieveStatsID(const FString& NewStatsID);

	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	// Returns the team number of the team that owns this object
	UFUNCTION()
	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	virtual	bool ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType);

	
	// Where should any chat go.  NOTE: some commands like Say and TeamSay ignore this value
	UPROPERTY(replicated)
	FName ChatDestination;

	UFUNCTION(server, reliable, withvalidation)
	virtual void ServerNextChatDestination();
};



