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
	GameNetworkVersion = 8001;
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

// @TODO FIXMESTEVE - we want open to be relative like it used to be
bool UUTGameEngine::HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld)
{
	return HandleTravelCommand(Cmd, Ar, InWorld);
}

bool UUTGameEngine::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out)
{
	if (FParse::Command(&Cmd, TEXT("START")))
	{
		FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
		FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Absolute);
		// make sure the file exists if we are opening a local file
		if (TestURL.IsLocalInternal() && !MakeSureMapNameIsValid(TestURL.Map))
		{
			Out.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
			return true;
		}
		else
		{
			SetClientTravel(InWorld, Cmd, TRAVEL_Absolute);
			return true;
		}
	}
	else
	{
		return Super::Exec(InWorld, Cmd, Out);
	}
}

void UUTGameEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	// HACK: make sure our default URL options are in all travel URLs since FURL code to do this was removed
	for (int32 WorldIdx = 0; WorldIdx < WorldList.Num(); ++WorldIdx)
	{
		FWorldContext& Context = WorldList[WorldIdx];
		if (!Context.TravelURL.IsEmpty())
		{
			FURL DefaultURL;
			DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
			FURL NewURL(&DefaultURL, *Context.TravelURL, TRAVEL_Absolute);
			for (int32 i = 0; i < DefaultURL.Op.Num(); i++)
			{
				FString OpKey;
				DefaultURL.Op[i].Split(TEXT("="), &OpKey, NULL);
				if (!NewURL.HasOption(*OpKey))
				{
					new(NewURL.Op) FString(DefaultURL.Op[i]);
				}
			}
			Context.TravelURL = NewURL.ToString();
		}
	}

	Super::Tick(DeltaSeconds, bIdleMode);
}