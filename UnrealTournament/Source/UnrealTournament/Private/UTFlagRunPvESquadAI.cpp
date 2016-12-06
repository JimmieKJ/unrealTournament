// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunPvESquadAI.h"

bool AUTFlagRunPvESquadAI::ShouldStartRally(AUTBot* B)
{
	if (GetWorld()->TimeSeconds - LastRallyTime <= 15.0f)
	{
		return false;
	}
	// since there are a lot of monsters, start rally even if there are a couple allies near unless low difficulty
	else if (B->Skill < 3.0f)
	{
		return Super::ShouldStartRally(B);
	}
	else
	{
		int32 Count = 0;
		for (AController* C : Team->GetTeamMembers())
		{
			if (C != B && C->GetPawn() != nullptr && (C->GetPawn()->GetActorLocation() - B->GetPawn()->GetActorLocation()).Size() < 2500.0f)
			{
				Count++;
			}
		}
		return (Count < FMath::Min<int32>(3, Team->GetSize() / 3));
	}
}