// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTSquadAI.h"
#include "UTTeamInfo.h"

#include "UTDuelSquadAI.generated.h"

UCLASS()
class AUTDuelSquadAI : public AUTSquadAI
{
	GENERATED_BODY()
public:
	virtual APlayerStart* PickSpawnPointFor(AUTBot* B, const TArray<APlayerStart*>& Choices) override
	{
		if (Choices.Num() < 2 || Team == NULL)
		{
			return NULL;
		}
		else
		{
			const TArray<const FBotEnemyInfo>& EnemyList = Team->GetEnemyList();
			if (EnemyList.Num() == 0)
			{
				return NULL;
			}
			else
			{
				// if enemy is low, pick closer spawn point, otherwise try to stay away
				// TODO: also should consider if nearest weapon pickup is available
				bool bWantCloser = EnemyList[0].EffectiveHealthPct < 1.0f;
				APlayerStart* Closest = NULL;
				float ClosestDist = FLT_MAX;
				for (APlayerStart* TestStart : Choices)
				{
					float Dist = (TestStart->GetActorLocation() - EnemyList[0].LastKnownLoc).Size();
					if (Dist < ClosestDist)
					{
						Closest = TestStart;
						ClosestDist = Dist;
					}
				}
				if (bWantCloser)
				{
					return Closest;
				}
				else
				{
					return (Closest == Choices[0]) ? Choices[1] : Choices[0];
				}
			}
		}
	}
};
