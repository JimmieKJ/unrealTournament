// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamInfo.generated.h"

UCLASS()
class AUTTeamInfo : public AInfo, public IUTTeamInterface
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Team)
	FText TeamName;

	/** team score */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Team)
	int32 Score;

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
};