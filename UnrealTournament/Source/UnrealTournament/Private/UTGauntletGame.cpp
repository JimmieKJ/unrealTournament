// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_Gauntlet.h"
#include "UTGauntletGame.h"
#include "UTGauntletFlagBase.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameState.h"
#include "UTGauntletGameMessage.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTCountDownMessage.h"
#include "UTPickup.h"
#include "UTGameMessage.h"
#include "UTMutator.h"
#include "UTCTFSquadAI.h"
#include "UTWorldSettings.h"
#include "Widgets/SUTTabWidget.h"
#include "Dialogs/SUTPlayerInfoDialog.h"
#include "StatNames.h"
#include "Engine/DemoNetDriver.h"
#include "UTCTFScoreboard.h"
#include "UTShowdownRewardMessage.h"
#include "UTShowdownGameMessage.h"
#include "UTDroppedAmmoBox.h"

AUTGauntletGame::AUTGauntletGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	FlagCapScore = 1;
	GoalScore = 3;
	TimeLimit = 0;
	DisplayName = NSLOCTEXT("UTGameMode", "Gauntlet", "Gauntlet");
	IntermissionDuration = 30.f;
	GameStateClass = AUTGauntletGameState::StaticClass();
	HUDClass = AUTHUD_Gauntlet::StaticClass();
	RoundLives=32;
	bPerPlayerLives = false;
	bAsymmetricVictoryConditions = false;
	FlagSwapTime=10;
	FlagPickupDelay=0;
}

void AUTGauntletGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	FlagSwapTime = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("FlagSwapTime"), FlagSwapTime));

	// Create the team infos.
	TeamInfo.AddZeroed(2);

	for (TActorIterator<AUTGauntletFlagBase> It(GetWorld()); It; ++It)
	{
		if (It->bScoreBase)
		{
			uint8 TeamNum = It->GetTeamNum();
			if (TeamInfo.IsValidIndex(TeamNum))
			{
				TeamInfo[TeamNum].FlagBases.Add(*It);
			}
		}
		else if (FlagDispenser == nullptr)
		{
			FlagDispenser = *It;
		}
	}

	IntermissionDuration = 5.0f;

}

void AUTGauntletGame::BroadcastVictoryConditions()
{
}

void AUTGauntletGame::InitGameState()
{
	Super::InitGameState();
	GauntletGameState = Cast<AUTGauntletGameState>(UTGameState);
	if (GauntletGameState)
	{
		GauntletGameState->FlagSwapTime = FlagSwapTime;
		GauntletGameState->bWeightedCharacter = true;
	}
}


bool AUTGauntletGame::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	return Super::CheckScore_Implementation(Scorer);
}

void AUTGauntletGame::HandleFlagCapture(AUTPlayerState* Holder)
{
	Super::HandleFlagCapture(Holder);
}

void AUTGauntletGame::HandleMatchHasStarted()
{
	RoundReset();
	Super::HandleMatchHasStarted();
}

void AUTGauntletGame::RoundReset()
{
	if (FlagDispenser) FlagDispenser->RoundReset();
	for (int32 TeamIndex = 0; TeamIndex < TeamInfo.Num(); TeamIndex++)
	{
		for (int32 i = 0; i < TeamInfo[TeamIndex].FlagBases.Num(); i++)
		{
			TeamInfo[TeamIndex].FlagBases[i]->RoundReset();
		}
	}

	GauntletGameState->AttackingTeam = 255;
	GauntletGameState->SetFlagSpawnTimer(30);	// TODO: Make me an option
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGauntletGame::SpawnInitalFlag, 31.0 * GetActorTimeDilation());
}

void AUTGauntletGame::SpawnInitalFlag()
{
	if (FlagDispenser)
	{
		FlagDispenser->Activate();	
	}
}

void AUTGauntletGame::FlagTeamChanged(uint8 NewTeamIndex)
{
	GauntletGameState->AttackingTeam = NewTeamIndex;
	for (int32 TeamIndex = 0; TeamIndex < TeamInfo.Num(); TeamIndex++)
	{
		for (int32 i = 0; i < TeamInfo[TeamIndex].FlagBases.Num(); i++)
		{
			if (TeamIndex != NewTeamIndex)
			{
				TeamInfo[TeamIndex].FlagBases[i]->Deactivate();
			}
			else
			{
				TeamInfo[TeamIndex].FlagBases[i]->Activate();
			}
		}
	}

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC)
		{
			PC->ClientReceiveLocalizedMessage(UUTGauntletGameMessage::StaticClass(), PC->GetTeamNum() == NewTeamIndex ? 2 : 3, nullptr, nullptr, nullptr);
		}
	}

}

void AUTGauntletGame::RestartPlayer(AController* aPlayer)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(aPlayer->PlayerState);

	if (PS && PS->bSpawnCostLives)
	{
		int32 TeamLivesRemaining = PS->GetTeamNum() == 0 ? GauntletGameState->RedLivesRemaining : GauntletGameState->BlueLivesRemaining;
		if (TeamLivesRemaining > 0)
		{
			// Allow the player to spawn.  Skip Steve's code as we suplant it.
			AUTCTFBaseGame::RestartPlayer(aPlayer);
		
			if (PS->GetTeamNum() == 0)
			{
				GauntletGameState->RedLivesRemaining--;
			}
			else
			{
				GauntletGameState->BlueLivesRemaining--;
			}

			// Check if we are == 5 lives remaining and if we are tell the team they are close to death
			TeamLivesRemaining = PS->GetTeamNum() == 0 ? GauntletGameState->RedLivesRemaining : GauntletGameState->BlueLivesRemaining;
			if (TeamLivesRemaining == 5)
			{
				for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
					if (PC->GetTeamNum() == PS->GetTeamNum())
					{
						PC->ClientReceiveLocalizedMessage(UUTShowdownGameMessage::StaticClass(), 7);
					}
				}
			}
		}
	}
	else
	{
		if (PS) PS->bSpawnCostLives = true;
		AUTCTFBaseGame::RestartPlayer(aPlayer);
	}
}

void AUTGauntletGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);

	AUTPlayerState* PS = Cast<AUTPlayerState>(Other->PlayerState);
	AUTPlayerState* KillerPS = Cast<AUTPlayerState>(Killer->PlayerState);

	UE_LOG(UT,Log,TEXT("Adding Currency"));
	if (PS && KillerPS && PS->GetTeamNum() != KillerPS->GetTeamNum())
	{
		float NewCurrency = 250.0f + (PS->GetAvailableCurrency() * 0.25f);
		KillerPS->AdjustCurrency(NewCurrency);
		for (int32 i=0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* Teammate = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (Teammate && Teammate->GetTeamNum() == KillerPS->GetTeamNum() && Teammate != KillerPS)
			{
				Teammate->AdjustCurrency(NewCurrency * 0.25f);
			}
		}
	}

	if ( PS && !IsTeamStillAlive(PS->GetTeamNum()) )
	{
		uint8 OtherTeamNum = 1 - PS->GetTeamNum();
		if (IsTeamStillAlive(OtherTeamNum))
		{
			// Tell the other team to finish it...
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC->GetTeamNum() == OtherTeamNum)
				{
					PC->ClientReceiveLocalizedMessage(UUTShowdownRewardMessage::StaticClass(), 0);
				}
			}

			// If the flag isn't associated with the current team, then swap it..
			if (FlagDispenser->MyFlag && FlagDispenser->MyFlag->GetTeamNum() != OtherTeamNum)
			{
				AUTGauntletFlag* GauntletFlag = Cast<AUTGauntletFlag>(FlagDispenser->MyFlag);
				if (GauntletFlag)
				{
					GauntletFlag->TeamSwap();
				}
			}
		}
		// TODO: Handle possible odd tie case
	}
}

// Looks to see if a given team has a chance to keep playing
bool AUTGauntletGame::IsTeamStillAlive(uint8 TeamNum)
{
	if (GauntletGameState == nullptr) return false;

	// First get the current # of lives
	int32 TeamLivesRemaining = TeamNum == 0 ? GauntletGameState->RedLivesRemaining : GauntletGameState->BlueLivesRemaining;

	// If there are no lives remaining, look for a living player
	if (TeamLivesRemaining <= 0)
	{
		// Look to see if anyone else is alive on this team...
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>((*Iterator)->PlayerState);
			if (PlayerState && PlayerState->GetTeamNum() == TeamNum)
			{
				AUTCharacter* UTChar = Cast<AUTCharacter>((*Iterator)->GetPawn());
				if (UTChar && !UTChar->IsDead())
				{
					return true;
				}
			}
		}
	}
	return true;
}

bool AUTGauntletGame::CanFlagTeamSwap(uint8 NewTeamNum)
{
	return IsTeamStillAlive(NewTeamNum);
}

void AUTGauntletGame::HandleMatchIntermission()
{
	Super::HandleMatchIntermission();
	for (int32 i=0; i < GauntletGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(GauntletGameState->PlayerArray[i]);
		if (PS) PS->bSpawnCostLives	= false;
	}
}

void AUTGauntletGame::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(Other);
	if (UTC != NULL)
	{
		// spawn ammo box with any unassigned ammo
		if (UTC->SavedAmmo.Num() > 0)
		{
			FVector FinalDir = FMath::VRand();
			FinalDir.Z = 0.0f;
			FinalDir.Normalize();
			FActorSpawnParameters Params;
			Params.Instigator = UTC;
			AUTDroppedAmmoBox* Pickup = GetWorld()->SpawnActor<AUTDroppedAmmoBox>(AUTDroppedAmmoBox::StaticClass(), UTC->GetActorLocation(), FinalDir.Rotation(), Params);
			if (Pickup != NULL)
			{
				Pickup->Movement->Velocity = FinalDir * 1000.0f + FVector(0.0f, 0.0f, 250.0f);
				Pickup->Ammo = UTC->SavedAmmo;
			}
		}
	}
	Super::DiscardInventory(Other, Killer);
}
