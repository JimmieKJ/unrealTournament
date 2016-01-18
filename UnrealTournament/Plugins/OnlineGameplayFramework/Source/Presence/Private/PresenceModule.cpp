// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PresencePrivatePCH.h"
#include "PresenceModule.h"

IMPLEMENT_MODULE(FPresenceModule, Presence);

DEFINE_LOG_CATEGORY(LogPresence);

#if STATS
DEFINE_STAT(STAT_PresenceStat1);
#endif

void FPresenceModule::StartupModule()
{	
}

void FPresenceModule::ShutdownModule()
{
}

bool FPresenceModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Presence
	if (FParse::Command(&Cmd, TEXT("Presence")))
	{
		return false;
	}

	return false;
}


