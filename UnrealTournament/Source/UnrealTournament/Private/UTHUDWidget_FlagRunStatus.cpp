// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_FlagRunStatus.h"
#include "UTSCTFGameState.h"
#include "UTSCTFFlag.h"

UUTHUDWidget_FlagRunStatus::UUTHUDWidget_FlagRunStatus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTHUDWidget_FlagRunStatus::DrawStatusMessage(float DeltaTime)
{
}

void UUTHUDWidget_FlagRunStatus::DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation)
{
	if (GameState)
	{
		uint8 OwnerTeam = UTHUDOwner->UTPlayerOwner->GetTeamNum();

		TArray<AUTCTFFlag*> Flags;
		GameState->GetImportantFlag(OwnerTeam, Flags);

		TArray<AUTCTFFlagBase*> FlagBases;
		GameState->GetImportantFlagBase(OwnerTeam, FlagBases);

		bool bDrawBasesInWorld = true;
		// Flag is there and in the world.
		if (Flags.Num() > 0)
		{
			AUTCTFFlag* Flag = Flags[0];
			AUTCTFFlagBase* FlagBase = FlagBases.Num() > 0 ? FlagBases[0] : nullptr;

			if (Flag->ObjectState != CarriedObjectState::Home)
			{
				uint8 TeamNum = Flag->GetTeamNum();
				DrawFlagStatus(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FVector2D(0.0f, 0.0f), FlagBase, Flag, Flag->Holder);
				DrawFlagWorld(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FlagBase, Flag, Flag->Holder);
				if (FlagBase)
				{
					DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, FlagBase->GetTeamNum(), FlagBase, Flag, Flag->Holder);
					bDrawBasesInWorld = false;
				}
			}
		}

		if (FlagBases.Num() > 0 && bDrawBasesInWorld)
		{
			for (int32 i = 0; i < FlagBases.Num(); i++)
			{
				DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, FlagBases[i]->GetTeamNum(), FlagBases[i], nullptr, nullptr);
			}
		}
	}
}

FText UUTHUDWidget_FlagRunStatus::GetFlagReturnTime(AUTCTFFlag* Flag)
{
	return Super::GetFlagReturnTime(Flag);
}


bool UUTHUDWidget_FlagRunStatus::ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag)
{
	return (Flag->ObjectState == CarriedObjectState::Dropped) || Flag->bCurrentlyPinged || !bIsEnemyFlag;
}
