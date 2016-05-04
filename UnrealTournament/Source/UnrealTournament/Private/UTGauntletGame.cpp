// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_Gauntlet.h"
#include "UTCTFGameMode.h"
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
#include "SUTGauntletSpawnWindow.h"
#include "UTDroppedLife.h"

AUTGauntletGame::AUTGauntletGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MapPrefix = TEXT("CTF");
	HUDClass = AUTHUD_Gauntlet::StaticClass();
	GameStateClass = AUTGauntletGameState::StaticClass();

	GoalScore = 3;
	bHideInUI = true;
}

void AUTGauntletGame::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	AUTPlayerState* PlayerState = Cast<AUTPlayerState>(C->PlayerState);
	if (PlayerState && AvailableLoadout.Num() > 0 && AvailableLoadout[0].ItemClass != nullptr )
	{
		PlayerState->BoostClass = AvailableLoadout[0].ItemClass;
	}
}


void AUTGauntletGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);
	
	// If this wasn't self inflected or environmental

	if (Killer && Killer != Other)
	{
		AUTPlayerState* KillerPlayerState = Cast<AUTPlayerState>(Killer->PlayerState);
		AUTPlayerState* VictimPlayerState = Cast<AUTPlayerState>(Other->PlayerState);

		if (KillerPlayerState && VictimPlayerState)
		{
			FActorSpawnParameters Params;
			Params.Instigator = KilledPawn;
			AUTDroppedLife* LifeSkull = GetWorld()->SpawnActor<AUTDroppedLife>(AUTDroppedLife::StaticClass(), KilledPawn->GetActorLocation(), KilledPawn->GetActorRotation(), Params);
			if (LifeSkull != NULL)
			{
				LifeSkull->Init(VictimPlayerState, KillerPlayerState, 300.0f);
			}
		}
	}
}


bool AUTGauntletGame::CanBoost(AUTPlayerController* Who)
{
	if (Who && Who->UTPlayerState && Who->UTPlayerState->BoostClass)
	{
		AUTReplicatedLoadoutInfo* LoadoutInfo = nullptr;
		for (int32 i = 0; i < UTGameState->AvailableLoadout.Num(); i++)
		{
			if (Who->UTPlayerState->BoostClass == UTGameState->AvailableLoadout[i]->ItemClass)
			{
				LoadoutInfo = UTGameState->AvailableLoadout[i];
				break;
			}
		}
	
		if (LoadoutInfo)
		{
			if (LoadoutInfo->CurrentCost <= Who->UTPlayerState->GetAvailableCurrency())
			{
				return true;
			}
		}
	}

	return false;
}

bool AUTGauntletGame::AttemptBoost(AUTPlayerController* Who)
{
	if (Who && Who->UTPlayerState && Who->UTPlayerState->BoostClass)
	{
		AUTReplicatedLoadoutInfo* LoadoutInfo = nullptr;
		for (int32 i = 0; i < UTGameState->AvailableLoadout.Num(); i++)
		{
			if (Who->UTPlayerState->BoostClass == UTGameState->AvailableLoadout[i]->ItemClass)
			{
				LoadoutInfo = UTGameState->AvailableLoadout[i];
				break;
			}
		}
	
		if (LoadoutInfo && LoadoutInfo->CurrentCost <= Who->UTPlayerState->GetAvailableCurrency())
		{
			Who->UTPlayerState->AdjustCurrency(LoadoutInfo->CurrentCost * -1.0f);
			return true;
		}
	}
	return false;
}