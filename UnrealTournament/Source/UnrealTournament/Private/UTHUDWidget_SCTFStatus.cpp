// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_SCTFStatus.h"
#include "UTSCTFGameState.h"
#include "UTSCTFFlag.h"

UUTHUDWidget_SCTFStatus::UUTHUDWidget_SCTFStatus(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	OffensePrimaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnOffenseA","!! YOU ARE ON OFFENSE !!");
	OffenseSecondaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnOffenseB","Assault the enemy base!");
	DefensePrimaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnDefenseA","!! YOU ARE ON DEFENSE !!");
	DefenseSecondaryMessage	= NSLOCTEXT("SCTFGame","YouAreOnDefenseB","Protect your base from the enemy!");

	FlagDownPrimary						= NSLOCTEXT("SCTFGame","FlagDownA","!! THE FLAG IS DOWN !!");
	FlagDownOffenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagDownOB","You have {0} seconds to get the flag!  Hurry!");
	FlagDownDefenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagDownDB","Defend the flag for {0} seconds to gain control!");	

	FlagHomePrimary						= NSLOCTEXT("SCTFGame","FlagHomeA","!! THE FLAG HAS BEEN RESET !!");
	FlagHomeOffenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagHomeOB","Get the flag and assault the base!");
	FlagHomeDefenseSecondaryMessage		= NSLOCTEXT("SCTFGame","FlagHomeDB","Get ready to defend your base!");	
}

void UUTHUDWidget_SCTFStatus::DrawStatusMessage(float DeltaTime)
{
	AUTSCTFGameState* GGameState = Cast<AUTSCTFGameState>(UTGameState);
	if (GGameState && GGameState->Flag && GGameState->AttackingTeam != 255)
	{
		PrimaryMessage.bHidden = false;
		SecondaryMessage.bHidden = false;
		bool bOnOffense = GGameState->AttackingTeam == UTPlayerOwner->GetTeamNum();

		if ( GGameState->Flag->ObjectState == CarriedObjectState::Held )
		{
			PrimaryMessage.Text = bOnOffense ? OffensePrimaryMessage : DefensePrimaryMessage;
			SecondaryMessage.Text = bOnOffense ? OffenseSecondaryMessage : DefenseSecondaryMessage;
		}
		else if ( GGameState->Flag->ObjectState == CarriedObjectState::Dropped )
		{
			PrimaryMessage.Text = FlagDownPrimary;
			SecondaryMessage.Text = FText::Format((bOnOffense ? FlagDownOffenseSecondaryMessage: FlagDownDefenseSecondaryMessage), FText::AsNumber(int32(GGameState->Flag->SwapTimer))); 
		}
		else
		{
			PrimaryMessage.Text = FlagHomePrimary;
			SecondaryMessage.Text = bOnOffense ? FlagHomeOffenseSecondaryMessage: FlagHomeDefenseSecondaryMessage; 
		}

		float Scale = 0.85 + (0.3f * FMath::Abs<float>((FMath::Sin(UTHUDOwner->GetWorld()->GetTimeSeconds() * 2))));
		PrimaryMessage.TextScale = Scale;
	}
	else
	{
		PrimaryMessage.bHidden = true;
		SecondaryMessage.bHidden = true;
	}

	RenderObj_Text(PrimaryMessage);
	RenderObj_Text(SecondaryMessage);
}

void UUTHUDWidget_SCTFStatus::DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation)
{
	AUTSCTFGameState* GGameState = Cast<AUTSCTFGameState>(GameState);
	if (GGameState)
	{
		// Flag is there and in the world.
		if (GGameState->Flag && GGameState->Flag->ObjectState != CarriedObjectState::Home)
		{
			uint8 TeamNum = GGameState->Flag->GetTeamNum();
			AUTTeamInfo* TeamInfo = TeamNum != 255 && GameState->Teams.IsValidIndex(TeamNum) ? GameState->Teams[TeamNum] : nullptr;
			AUTCTFFlagBase* FlagBase = TeamNum != 255 && GameState->FlagBases.IsValidIndex(TeamNum) ? GameState->FlagBases[TeamNum] : nullptr;
			DrawFlagStatus(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FVector2D(0.0f,0.0f), FlagBase, GGameState->Flag, GGameState->Flag->Holder);
			DrawFlagWorld(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FlagBase, GGameState->Flag, GGameState->Flag->Holder);
			DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, TeamNum, FlagBase, GGameState->Flag, GGameState->Flag->Holder);
		}
		else
		{
			DrawFlagBaseWorld(GameState, PlayerViewPoint, PlayerViewRotation, 255, GGameState->FlagDispenser, GGameState->Flag, GGameState->Flag ? GGameState->Flag->Holder : nullptr);
		}
	}
}

FText UUTHUDWidget_SCTFStatus::GetFlagReturnTime(AUTCTFFlag* Flag)
{	
	AUTSCTFGameState* GGameState = Cast<AUTSCTFGameState>(UTGameState);
	if (GGameState)
	{
		if (GGameState->Flag->ObjectState == CarriedObjectState::Dropped)
		{
			return FText::AsNumber(int32(GGameState->Flag->SwapTimer));
		}
	}

	return FText::GetEmpty();	
}

FText UUTHUDWidget_SCTFStatus::GetBaseMessage(AUTCTFFlagBase* Base, AUTCTFFlag* Flag)
{
	if (Base)
	{
		AUTSCTFFlagBase* SFlagBase = Cast<AUTSCTFFlagBase>(Base);
		AUTSCTFGameState* GGameState = Cast<AUTSCTFGameState>(UTGameState);
		if (SFlagBase && !SFlagBase->bScoreBase && Flag == nullptr && GGameState && GGameState->HasMatchStarted())
		{
			return FText::Format(NSLOCTEXT("SCTFStatus","FlagSpawnFormat","Flag Spawns in {0}"), FText::AsNumber(GGameState->FlagSpawnTimer));
		}
	}

	return FText::GetEmpty();
}

bool UUTHUDWidget_SCTFStatus::ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag)
{
	return (Flag->ObjectState == CarriedObjectState::Dropped) || Flag->bCurrentlyPinged || !bIsEnemyFlag;
}
