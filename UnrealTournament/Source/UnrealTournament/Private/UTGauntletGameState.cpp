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
	AttackingTeam = 255;
}

