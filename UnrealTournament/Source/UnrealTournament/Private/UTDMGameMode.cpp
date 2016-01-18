// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD_DM.h"
#include "StatNames.h"
#include "UTDMGameMode.h"


AUTDMGameMode::AUTDMGameMode(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HUDClass = AUTHUD_DM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "DM", "Deathmatch");
	XPMultiplier = 3.0f;
}


void AUTDMGameMode::UpdateSkillRating()
{
	for (int32 PlayerIdx = 0; PlayerIdx < UTGameState->PlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateIndividualSkillRating(NAME_DMSkillRating, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}

	for (int32 PlayerIdx = 0; PlayerIdx < InactivePlayerArray.Num(); PlayerIdx++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[PlayerIdx]);
		if (PS && !PS->bOnlySpectator)
		{
			PS->UpdateIndividualSkillRating(NAME_DMSkillRating, &UTGameState->PlayerArray, &InactivePlayerArray);
		}
	}
}

int32 AUTDMGameMode::GetEloFor(AUTPlayerState* PS) const
{
	return PS ? PS->DMRank : Super::GetEloFor(PS);
}