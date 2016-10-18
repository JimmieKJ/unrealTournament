// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LobbyPrivatePCH.h"
#include "LobbyModule.h"

IMPLEMENT_MODULE(FLobbyModule, Lobby);

DEFINE_LOG_CATEGORY(LogLobby);

#if STATS
//DEFINE_STAT(STAT_LobbyStat1);
#endif

void FLobbyModule::StartupModule()
{	
}

void FLobbyModule::ShutdownModule()
{
}

bool FLobbyModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Lobby
	if (FParse::Command(&Cmd, TEXT("Lobby")))
	{
		return false;
	}

	return false;
}


