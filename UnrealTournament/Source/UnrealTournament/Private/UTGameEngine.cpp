// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameEngine.h"

UUTGameEngine::UUTGameEngine(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bFirstRun = true;
	ReadEULACaption = NSLOCTEXT("UTGameEngine", "ReadEULACaption", "READ ME FIRST");
	ReadEULAText = NSLOCTEXT("UTGameEngine", "ReadEULAText", "By playing this game you agree to the EULA located at: http://www.unrealengine.com\n\nDo you wish to continue?");
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