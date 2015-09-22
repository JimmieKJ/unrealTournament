// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTSquadAI.h"
#include "UTTeamInfo.h"
#include "UTShowdownGame.h"

#include "UTShowdownSquadAI.generated.h"

UCLASS()
class AUTShowdownSquadAI : public AUTSquadAI
{
	GENERATED_BODY()
public:
	UPROPERTY()
	APlayerStart* LastChoice;

	virtual APlayerStart* PickSpawnPointFor(AUTBot* B, const TArray<APlayerStart*>& Choices) override
	{
		AUTShowdownGame* Game = GetWorld()->GetAuthGameMode<AUTShowdownGame>();
		if (Game == NULL)
		{
			return NULL;
		}
		else if (Game->LastRoundWinner == Team && Choices.Contains(LastChoice))
		{
			// it worked last time, let's do it again
			return LastChoice;
		}
		else if (Choices.Num() == 1)
		{
			return Choices[0];
		}
		else
		{
			TArray<APlayerStart*> RemainingChoices = Choices;
			RemainingChoices.Remove(LastChoice);

			// TODO: more logic (consider favorite weapon, enemy tendency so far, etc)
			return RemainingChoices[FMath::RandHelper(RemainingChoices.Num())];
		}
	}
};
