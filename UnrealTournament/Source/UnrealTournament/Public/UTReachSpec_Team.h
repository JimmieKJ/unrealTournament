// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"
#include "UTTeamPathBlocker.h"

#include "UTReachSpec_Team.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTReachSpec_Team : public UUTReachSpec
{
	GENERATED_BODY()
public:
	/** the Actor that decides who gets through */
	UPROPERTY()
	TWeakObjectPtr<AUTTeamPathBlocker> Arbiter;

	UUTReachSpec_Team(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		PathColor = FLinearColor(0.0f, 0.0f, 1.0f);
	}

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, const FUTReachParams& ReachParams, AController* RequestOwner, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) override
	{
		if (!Arbiter.IsValid())
		{
			return DefaultCost;
		}
		else
		{
			IUTTeamInterface* TeamOwner = Cast<IUTTeamInterface>(Asker);
			uint8 TeamNum = (TeamOwner != nullptr) ? TeamOwner->GetTeamNum() : 255;
			return Arbiter->IsAllowedThrough(TeamNum) ? DefaultCost : BLOCKED_PATH_COST;
		}
	}
};