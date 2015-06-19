// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamInfo.generated.h"

USTRUCT()
struct FPickupClaim
{
	GENERATED_USTRUCT_BODY()

	/** player doing the claiming (by Pawn intentionally; generally claim should become invalid on death) */
	UPROPERTY()
	APawn* ClaimedBy;
	UPROPERTY()
	AActor* Pickup;
	/** if set, only this player should be allowed to pick it up at all (e.g. don't let other bots take it even if closer and enemy might contest it) */
	UPROPERTY()
	bool bHardClaim;

	inline bool IsValid() const
	{
		return ClaimedBy != NULL && !ClaimedBy->bTearOff && !ClaimedBy->bPendingKillPending && Pickup != NULL && !Pickup->bPendingKillPending;
	}

	FPickupClaim()
	: ClaimedBy(NULL), Pickup(NULL), bHardClaim(false)
	{}
	FPickupClaim(APawn* InClaimedBy, AActor* InPickup, bool bInHardClaim)
	: ClaimedBy(InClaimedBy), Pickup(InPickup), bHardClaim(bInHardClaim)
	{}
};

UCLASS()
class UNREALTOURNAMENT_API AUTTeamInfo : public AInfo, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	/** team ID, set by UTTeamGameMode */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = ReceivedTeamIndex, Category = Team)
	uint8 TeamIndex;

	/** team color (e.g. for HUD, character material, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Team)
	FLinearColor TeamColor;

	/** list of default orders for bots assigned to this team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	TArray<FName> DefaultOrders;

	/** current place in DefaultOrders that we assign next bot to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	int32 DefaultOrderIndex;

	/** team claims on pickups for AI coordination */
	UPROPERTY()
	TArray<FPickupClaim> PickupClaims;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Team)
	FText TeamName;

	/** team score */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Team)
	int32 Score;

	/** For team stats. */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Team)
	AUTPlayerState* TopAttacker;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = Team)
	AUTPlayerState* TopDefender;

	UPROPERTY(BlueprintReadWrite, Replicated, Category = Team)
	AUTPlayerState* TopSupporter;

	/** Timer to update replicated team leaders. */
	virtual void UpdateTeamLeaders();

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	/** internal flag used to prevent the wrong TeamInfo from getting hooked up on clients during seamless travel, because it is possible for two sets to be on the client temporarily */
	UPROPERTY(Replicated)
	bool bFromPreviousLevel;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual void AddToTeam(AController* C);

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual void RemoveFromTeam(AController* C);

	/** returns current number of players on the team; server only */
	inline uint32 GetSize()
	{
		checkSlow(Role == ROLE_Authority);
		return TeamMembers.Num();
	}

	virtual uint8 GetTeamNum() const override
	{
		return TeamIndex;
	}
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

	UFUNCTION()
	virtual void ReceivedTeamIndex();

	const TArray<const struct FBotEnemyInfo>& GetEnemyList()
	{
		return *(TArray<const FBotEnemyInfo>*)&EnemyList;
	}
	
	/** assign to a squad with the specified orders and optional leader
	 * @return if a squad matching the requirements was found or created
	 */
	virtual bool AssignToSquad(AController* C, FName Orders, AController* Leader = NULL);

	/** removes a squad; any assigned members are properly removed */
	virtual void RemoveSquad(class AUTSquadAI* DeadSquad);

	/** assign default squad for this player
	 * this method should always assign a valid Squad
	 */
	virtual void AssignDefaultSquadFor(AController* C);

	/** updates some or all of an enemy's information in EnemyList based on the update type
	 * if the enemy is new or hasn't been updated in a long time, possibly trigger update of member bots' current enemy
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	virtual void UpdateEnemyInfo(APawn* NewEnemy, enum EAIEnemyUpdateType UpdateType);

	/** notifies AI of some game objective related event (e.g. flag taken)
	 * generally bots will get retasked if they were performing some action relating to the object that is no longer relevant
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	virtual void NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName);

	const TArray<AController*>& GetTeamMembers() { return TeamMembers; }

	FPickupClaim GetClaimForPickup(AActor* InPickup)
	{
		for (int32 i = 0; i < PickupClaims.Num(); i++)
		{
			if (!PickupClaims[i].IsValid())
			{
				PickupClaims.RemoveAt(i);
				i--;
			}
			else if (PickupClaims[i].Pickup == InPickup)
			{
				return PickupClaims[i];
			}
		}
		return FPickupClaim();
	}

	FPickupClaim GetClaimForPawn(APawn* ClaimedBy)
	{
		for (int32 i = 0; i < PickupClaims.Num(); i++)
		{
			if (!PickupClaims[i].IsValid())
			{
				PickupClaims.RemoveAt(i);
				i--;
			}
			else if (PickupClaims[i].ClaimedBy == ClaimedBy)
			{
				return PickupClaims[i];
			}
		}
		return FPickupClaim();
	}

	void ClearClaimedPickup(AActor* InPickup)
	{
		for (int32 i = 0; i < PickupClaims.Num(); i++)
		{
			if (PickupClaims[i].Pickup == InPickup)
			{
				PickupClaims.RemoveAt(i);
				return;
			}
		}
	}

	void ClearPickupClaimFor(APawn* ClaimedBy)
	{
		for (int32 i = 0; i < PickupClaims.Num(); i++)
		{
			if (PickupClaims[i].ClaimedBy == ClaimedBy)
			{
				PickupClaims.RemoveAt(i);
				return;
			}
		}
	}

	void SetPickupClaim(APawn* ClaimedBy, AActor* InPickup, bool bHardClaim = false)
	{
		for (FPickupClaim& Claim : PickupClaims)
		{
			if (Claim.ClaimedBy == ClaimedBy)
			{
				Claim.Pickup = InPickup;
				Claim.bHardClaim = bHardClaim;
				return;
			}
		}
		new(PickupClaims) FPickupClaim(ClaimedBy, InPickup, bHardClaim);
	}

	/** called to reinitialize AI squads, generally after a game intermission (new round, halftime, etc) */
	virtual void ReinitSquads();

protected:
	/** list of players on this team currently (server only) */
	UPROPERTY(BlueprintReadOnly, Category = Team)
	TArray<AController*> TeamMembers;

	/** list of squads on this team */
	UPROPERTY(BlueprintReadOnly, Category = AI)
	TArray<class AUTSquadAI*> Squads;

	/** list of known enemies for bots */
	UPROPERTY(BlueprintReadOnly, Category = AI)
	TArray<FBotEnemyInfo> EnemyList;

	/** map of additional stats used for scoring display. */
	TMap< FName, float > StatsData;

public:
	/** Last time StatsData was updated - used when replicating the data. */
	UPROPERTY()
		float LastScoreStatsUpdateTime;

	/** Accessors for StatsData. */
	float GetStatsValue(FName StatsName);
	void SetStatsValue(FName StatsName, float NewValue);
	void ModifyStatsValue(FName StatsName, float Change);

};