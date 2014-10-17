// Squad AI contains gametype and role specific AI for bots
// for example, an attacker in CTF gets a different Squad than a defender in CTF
// which is different from a defender in Warfare
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"
#include "UTTeamInfo.h"

#include "UTSquadAI.generated.h"

UCLASS(NotPlaceable)
class AUTSquadAI : public AInfo, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	friend class AUTTeamInfo;

	/** team this squad is on (may be NULL) */
	UPROPERTY(BlueprintReadOnly, Category = Squad)
	AUTTeamInfo* Team;
	/** squad role/orders */
	UPROPERTY(BlueprintReadOnly, Category = Squad)
	FName Orders;
	/** squad objective (target to attack, defend, etc) - may be NULL */
	UPROPERTY(BlueprintReadOnly, Category = Squad)
	AActor* Objective;
protected:
	/** cached pointer to navigation data */
	AUTRecastNavMesh* NavData;

	/** list of squad members */
	UPROPERTY(BlueprintReadOnly, Category = Squad)
	TArray<AController*> Members;
	/** leader (prefer to stay near this player when objectives permit)*/
	UPROPERTY(BlueprintReadOnly, Category = Squad)
	AController* Leader;
public:

	inline AController* GetLeader() const
	{
		return Leader;
	}
	inline int32 GetSize() const
	{
		return Members.Num();
	}

	virtual uint8 GetTeamNum() const override
	{
		return (Team != NULL) ? Team->TeamIndex : 255;
	}

	virtual void BeginPlay() override
	{
		NavData = GetUTNavData(GetWorld());
		Super::BeginPlay();
	}

	virtual void AddMember(AController* C);
	virtual void RemoveMember(AController* C);
	virtual void SetLeader(AController* NewLeader);

	/** @return if enemy is important to track for as long as possible (e.g. threatening game objective) */
	virtual bool MustKeepEnemy(APawn* TheEnemy)
	{
		return false;
	}

	/** @return modified rating for enemy, taking into account objectives */
	virtual float ModifyEnemyRating(float CurrentRating, const FBotEnemyInfo& EnemyInfo, AUTBot* B)
	{
		return CurrentRating;
	}

	/** modify the bot's attack aggressiveness, generally in response to its target's relevance to game objectives */
	virtual void ModifyAggression(AUTBot* B, float& Aggressiveness)
	{
		if (MustKeepEnemy(B->GetEnemy()))
		{
			Aggressiveness += 0.5f;
		}
	}

	/** called by bot during its decision logic to see if there's an action relating to the game's objectives it should take
	 * @return if an action was assigned
	 */
	virtual bool CheckSquadObjectives(AUTBot* B);

	/** called in bot fighting logic to ask for destination when bot wants to retreat
	 * should set MoveTarget to desired destination but don't set action
	 */
	virtual bool PickRetreatDestination(AUTBot* B);
};