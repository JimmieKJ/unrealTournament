// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFRoundGame.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTCountDownMessage.h"
#include "UTPickup.h"
#include "UTGameMessage.h"
#include "UTMutator.h"
#include "UTCTFSquadAI.h"
#include "UTWorldSettings.h"
#include "Slate/Widgets/SUTTabWidget.h"
#include "Slate/SUWPlayerInfoDialog.h"
#include "StatNames.h"
#include "Engine/DemoNetDriver.h"
#include "UTCTFScoreboard.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GoalScore = 3;
}