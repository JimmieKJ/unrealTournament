// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/HUD.h"
#include "UTLobbyHUD.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyPC.h"


AUTLobbyHUD::AUTLobbyHUD(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void AUTLobbyHUD::PostRender()
{
	AHUD::PostRender();
	return;
}