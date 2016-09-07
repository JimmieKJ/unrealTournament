// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGauntletGame.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameMessage.h"
#include "UTCountDownMessage.h"
#include "Net/UnrealNetwork.h"

AUTGauntletGameState::AUTGauntletGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bFirstRoundInitialized = false;
}

void AUTGauntletGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGauntletGameState, FlagSwapTime);
	DOREPLIFETIME(AUTGauntletGameState, Flag);
	DOREPLIFETIME(AUTGauntletGameState, FlagDispenser);
	DOREPLIFETIME(AUTGauntletGameState, bFirstRoundInitialized);
	
}

void AUTGauntletGameState::DefaultTimer()
{
	Super::DefaultTimer();
	AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();
	if (Role == ROLE_Authority && RemainingPickupDelay > 0)
	{
		if (RemainingPickupDelay == 11)
		{
			Game->BroadcastLocalized(this, UUTGauntletGameMessage::StaticClass(), 1, NULL, NULL, NULL);
		}
		if (RemainingPickupDelay <= 10 && RemainingPickupDelay > 0)
		{
			Game->BroadcastLocalized(this, UUTCountDownMessage::StaticClass(), RemainingPickupDelay, NULL, NULL, NULL);
		}
	}

}

FName AUTGauntletGameState::GetFlagState(uint8 TeamNum)
{
	return Flag ? Flag->ObjectState : CarriedObjectState::Home;
}

AUTPlayerState* AUTGauntletGameState::GetFlagHolder(uint8 TeamNum)
{
	return Flag ? Flag->Holder : nullptr;
}

AUTCTFFlagBase* AUTGauntletGameState::GetFlagBase(uint8 TeamNum)
{
	if (FlagBases.IsValidIndex(TeamNum))
	{
		return FlagBases[TeamNum];
	}

	return nullptr;
}

void AUTGauntletGameState::GetImportantFlag(int32 TeamNum, TArray<AUTCTFFlag*>& ImportantFlags)
{
	if (Flag)
	{
		ImportantFlags.Add(Flag);
	}
}

void AUTGauntletGameState::GetImportantFlagBase(int32 InTeamNum, TArray<AUTCTFFlagBase*>& ImportantBases)
{
	if (Flag == nullptr)
	{
		ImportantBases.Add(FlagDispenser);
	}
	else
	{
		uint8 TeamNum = Flag->GetTeamNum();
		if (FlagBases.IsValidIndex(TeamNum))
		{
			ImportantBases.Add(FlagBases[TeamNum]);
		}
	}
}