// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "OnlineSessionSettings.h"
#include "UTOnlineGameSearchBase.h"

FUTOnlineGameSearchBase::FUTOnlineGameSearchBase(bool bSearchForLANGames)
{
	bIsLanQuery = bSearchForLANGames;
}
