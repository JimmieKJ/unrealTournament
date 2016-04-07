// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTSCTFGame.h"
#include "UTSCTFGameState.h"
#include "UTSCTFFlag.h"
#include "UTSCTFGameMessage.h"
#include "UTCountDownMessage.h"
#include "Net/UnrealNetwork.h"

AUTSCTFGameState::AUTSCTFGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AttackingTeam = 255;
}

void AUTSCTFGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTSCTFGameState, AttackingTeam);
	DOREPLIFETIME(AUTSCTFGameState, FlagSpawnTimer);
	DOREPLIFETIME(AUTSCTFGameState, FlagSwapTime);
	DOREPLIFETIME(AUTSCTFGameState, Flag);
	
}

void AUTSCTFGameState::DefaultTimer()
{
	Super::DefaultTimer();
	AUTSCTFGame* Game = GetWorld()->GetAuthGameMode<AUTSCTFGame>();
	if (Role == ROLE_Authority && FlagSpawnTimer > 0)
	{
		FlagSpawnTimer--;
		if (FlagSpawnTimer == 11)
		{
			Game->BroadcastLocalized(this, UUTSCTFGameMessage::StaticClass(), 1, NULL, NULL, NULL);
		}
		if (FlagSpawnTimer <= 10 && FlagSpawnTimer > 0)
		{
			Game->BroadcastLocalized(this, UUTCountDownMessage::StaticClass(), FlagSpawnTimer, NULL, NULL, NULL);
		}
	}

}

void AUTSCTFGameState::AttackingTeamChanged()
{
	// Update hud/etc.
}

void AUTSCTFGameState::SetFlagSpawnTimer(int32 NewValue)
{
	AUTSCTFGame* Game = GetWorld()->GetAuthGameMode<AUTSCTFGame>();

	FlagSpawnTimer = NewValue;
	if (FlagSpawnTimer > 0)
	{
		Game->BroadcastLocalized(this, UUTSCTFGameMessage::StaticClass(), 0, NULL, NULL, NULL);
	}
}

FName AUTSCTFGameState::GetFlagState(uint8 TeamNum)
{
	return Flag ? Flag->ObjectState : CarriedObjectState::Home;
}

AUTPlayerState* AUTSCTFGameState::GetFlagHolder(uint8 TeamNum)
{
	return Flag ? Flag->Holder : nullptr;
}

AUTCTFFlagBase* AUTSCTFGameState::GetFlagBase(uint8 TeamNum)
{
	if (Flag && Flag->GetTeamNum() == TeamNum && FlagBases.IsValidIndex(TeamNum))
	{
		return FlagBases[TeamNum];
	}

	return nullptr;
}