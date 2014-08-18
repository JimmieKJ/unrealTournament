// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameEngine.h"
#include "UTAnalytics.h"

UUTGameEngine::UUTGameEngine(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bFirstRun = true;
	ReadEULACaption = NSLOCTEXT("UTGameEngine", "ReadEULACaption", "READ ME FIRST");
	ReadEULAText = NSLOCTEXT("UTGameEngine", "ReadEULAText", "Before playing this game you must agree to the terms and conditions of the end user license agreement located at: http://epic.gm/eula\n\nDo you acknowledge this agreement?");
	GameNetworkVersion = 8000;
}

void UUTGameEngine::Init(IEngineLoop* InEngineLoop)
{
	if (GEngineNetVersion == 0)
	{
		// @TODO FIXMESTEVE temp hack for network compatibility code
		GEngineNetVersion = GameNetworkVersion;
		UE_LOG(UT, Warning, TEXT("************************************Set Net Version %d"), GEngineNetVersion);
	}

	if (bFirstRun)
	{
		if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *ReadEULAText.ToString(), *ReadEULACaption.ToString()) != EAppReturnType::Yes)
		{
			FPlatformMisc::RequestExit(true);
		}
		bFirstRun = false;
		SaveConfig();
		GConfig->Flush(false);
	}

	FUTAnalytics::Initialize();
	Super::Init(InEngineLoop);
}

void UUTGameEngine::PreExit()
{
	Super::PreExit();
	FUTAnalytics::Shutdown();
}
