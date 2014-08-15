// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameEngine.h"

UUTGameEngine::UUTGameEngine(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bFirstRun = true;
	ReadEULACaption = NSLOCTEXT("UTGameEngine", "ReadEULACaption", "READ ME FIRST");
	ReadEULAText = NSLOCTEXT("UTGameEngine", "ReadEULAText", "Before playing this game you must agree to the terms and conditions of the end user license agreement located at: http://epic.gm/eula\n\nDo you acknowledge this agreement?");
}

void UUTGameEngine::Init(IEngineLoop* InEngineLoop)
{
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

	Super::Init(InEngineLoop);
}