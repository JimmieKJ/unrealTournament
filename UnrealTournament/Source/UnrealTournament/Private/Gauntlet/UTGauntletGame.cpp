// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTCTFGameMode.h"
#include "UTGauntletGame.h"
#include "UTGauntletFlagDispenser.h"
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
#include "UTDroppedLife.h"
#include "UTTimedPowerup.h"
#include "UTGauntletHUD.h"

AUTGauntletGame::AUTGauntletGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	FlagCapScore = 1;
	GoalScore = 3;
	TimeLimit = 0;
	InitialBoostCount = 0;
	DisplayName = NSLOCTEXT("UTGauntletGame", "GauntletDisplayName", "Gauntlet");
	
	IntermissionDuration = 30.f;
	GameStateClass = AUTGauntletGameState::StaticClass();
	HUDClass = AUTGauntletHUD::StaticClass();
	SquadType = AUTCTFSquadAI::StaticClass();
	RoundLives=0;
	bPerPlayerLives = false;
	FlagSwapTime=10;
	FlagPickupDelay=30;
	MapPrefix = TEXT("CTF");
	bHideInUI = true;
	bAttackerLivesLimited = false;
	bDefenderLivesLimited = false;
	bRollingAttackerSpawns = false;
	bWeaponStayActive = false;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	UnlimitedRespawnWaitTime = 3.0f;
	bGameHasTranslocator = false;
	bSitOutDuringRound = false;

}

void AUTGauntletGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	bForceRespawn = false;
	GoalScore = 3;	
	FlagSwapTime = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("FlagSwapTime"), FlagSwapTime));
}

void AUTGauntletGame::InitGameState()
{
	Super::InitGameState();
	GauntletGameState = Cast<AUTGauntletGameState>(UTGameState);
	GauntletGameState->FlagSwapTime = FlagSwapTime;
}

void AUTGauntletGame::ResetFlags() 
{
	Super::ResetFlags();
	if (GauntletGameState->FlagDispenser)
	{
		GauntletGameState->FlagDispenser->Reset();
	}
} 

void AUTGauntletGame::InitRound()
{
	Super::InitRound();

	if (GauntletGameState->FlagDispenser)
	{
		GauntletGameState->FlagDispenser->CreateFlag();
	}
}

void AUTGauntletGame::InitFlags() 
{
	if (GauntletGameState->FlagDispenser)
	{
		AUTCarriedObject* Flag = GauntletGameState->FlagDispenser->MyFlag;
		Flag->AutoReturnTime = 8.f;
		Flag->bGradualAutoReturn = true;
		Flag->bDisplayHolderTrail = true;
		Flag->bShouldPingFlag = true;
		Flag->bSlowsMovement = bSlowFlagCarrier;
		Flag->bSendHomeOnScore = false;
		Flag->SetActorHiddenInGame(false);
		Flag->bAnyoneCanPickup = true;
		Flag->bTeamPickupSendsHome = false;
		Flag->bEnemyPickupSendsHome = false;

		// check for flag carrier already here waiting
		TArray<AActor*> Overlapping;
		Flag->GetOverlappingActors(Overlapping, AUTCharacter::StaticClass());
		for (AActor* A : Overlapping)
		{
			AUTCharacter* Character = Cast<AUTCharacter>(A);
			if (Character != NULL)
			{
				if (!GetWorld()->LineTraceTestByChannel(Character->GetActorLocation(), Flag->GetActorLocation(), ECC_Pawn, FCollisionQueryParams(), WorldResponseParams))
				{
					Flag->TryPickup(Character);
				}
			}
		}
	}
}


void AUTGauntletGame::FlagTeamChanged(uint8 NewTeamIndex)
{
	if (GauntletGameState->Flag->GetTeamNum() != 255)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC)
			{
				PC->ClientReceiveLocalizedMessage(UUTGauntletGameMessage::StaticClass(), PC->GetTeamNum() == NewTeamIndex ? 2 : 3, nullptr, nullptr, nullptr);
			}
		}
	}

}

// Looks to see if a given team has a chance to keep playing
bool AUTGauntletGame::IsTeamStillAlive(uint8 TeamNum)
{
/*
	// Look to see if anyone else is alive on this team...
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerState* PlayerState = Cast<AUTPlayerState>((*Iterator)->PlayerState);
		if (PlayerState && PlayerState->GetTeamNum() == TeamNum)
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>((*Iterator)->GetPawn());
			if (!PlayerState->bOutOfLives || (UTChar && !UTChar->IsDead()))
			{
				return true;
			}
		}
	}
*/
	return true;
}

bool AUTGauntletGame::CanFlagTeamSwap(uint8 NewTeamNum)
{
	return IsTeamStillAlive(NewTeamNum);
}


void AUTGauntletGame::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	// Convert all existing non-dispensers in to score only bases
	AUTGauntletFlagDispenser* FlagDispenser = Cast<AUTGauntletFlagDispenser>(Obj);
	if (FlagDispenser == nullptr)
	{
		AUTCTFFlagBase* FlagBase = Cast<AUTCTFFlagBase>(Obj);
		if (FlagBase)
		{
			FlagBase->bScoreOnlyBase = true;
		}
	}
	else
	{
		GauntletGameState->FlagDispenser = FlagDispenser;
	}

	Super::GameObjectiveInitialized(Obj);
}

void AUTGauntletGame::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	
	if (GauntletGameState->FlagDispenser == nullptr)
	{
		AUTCTFFlagBase* RedBase = nullptr;
		AUTCTFFlagBase* BlueBase = nullptr;

		for (int32 i=0; i < GauntletGameState->FlagBases.Num(); i++)
		{
			if (GauntletGameState->FlagBases[i])
			{
				if (GauntletGameState->FlagBases[i]->GetTeamNum() == 0 ) RedBase = GauntletGameState->FlagBases[i];
				else if (GauntletGameState->FlagBases[i]->GetTeamNum() == 1) BlueBase = GauntletGameState->FlagBases[i];
			}
		}

		if (RedBase && BlueBase)
		{
			// Find the mid point between the bases..
			FVector Direction = RedBase->GetActorLocation() - BlueBase->GetActorLocation();
			FVector MidPoint = BlueBase->GetActorLocation() + Direction.GetSafeNormal() * (Direction.Size() * 0.5);

			// Now find the powerup closest to the mid point.

			float BestDist = 0.0f;
			AUTPickupInventory* BestInventory = nullptr;

			// Find the powerup closest to the middle of the playing field
			for (TActorIterator<AUTPickupInventory> ObjIt(GetWorld()); ObjIt; ++ObjIt)
			{
				float Dist = (ObjIt->GetActorLocation() - MidPoint).SizeSquared();
				if (BestInventory == nullptr || Dist < BestDist)
				{
					BestInventory = *ObjIt;
					BestDist = Dist;
				}
			}
			if (BestInventory)
			{
				FVector SpawnLocation = BestInventory->GetActorLocation();
				static FName NAME_PlacementTrace(TEXT("PlacementTrace"));
				FCollisionQueryParams QueryParams(NAME_PlacementTrace, false, BestInventory);
				QueryParams.bTraceAsyncScene = true;
				FHitResult Hit(1.f);
				bool bHitWorld = GetWorld()->LineTraceSingleByChannel(Hit, SpawnLocation, SpawnLocation + FVector(0,0,-200),ECollisionChannel::ECC_Pawn, QueryParams);
				if (bHitWorld)
				{
					SpawnLocation = Hit.Location;
				}

				AUTGauntletFlagDispenser* FlagDispenser = GetWorld()->SpawnActor<AUTGauntletFlagDispenser>(AUTGauntletFlagDispenser::StaticClass(), SpawnLocation, Direction.Rotation());
				if (FlagDispenser != nullptr)
				{
					GauntletGameState->FlagDispenser = FlagDispenser;
					BestInventory->Destroy();
				}
			}
			else
			{
				UE_LOG(UT, Error, TEXT("Gauntlet needs at least a powerup to use as an anchor for the dispenser."));
			}
		}
		else
		{
			UE_LOG(UT,Error, TEXT("There has to be both a red and a blue base in Gauntlet."));
		}
	}
}

void AUTGauntletGame::FlagsAreReady()
{
	// Send out own message here.
	InitFlags();
}

bool AUTGauntletGame::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	// Skip the round based version as we use goal score
	return AUTCTFBaseGame::CheckScore_Implementation(Scorer);
}
