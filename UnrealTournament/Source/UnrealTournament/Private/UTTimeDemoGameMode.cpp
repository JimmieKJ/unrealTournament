// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTTimeDemoGameMode.h"

AUTTimeDemoGameMode::AUTTimeDemoGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TimeLeft = -1;
	bStartedDemo = false;
}

bool AUTTimeDemoGameMode::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (IsTemplate() || IsPendingKill())
	{
		return false;
	}

	if (FParse::Command(&Cmd, TEXT("TIMEDEMOSTART")))
	{
		if (bStartedDemo)
		{
			return true;
		}

		int32 RunningTime = FCString::Atoi(Cmd);
		if (RunningTime > 0)
		{
			TimeLeft = RunningTime;
		}
		else
		{
			TimeLeft = -1;
		}

		ExecuteStartTimeDemoCommands(InWorld);
		StartTimeDemo();
		bStartedDemo = true;

		return true;
	}

	if (FParse::Command(&Cmd, TEXT("TIMEDEMOSTOP")))
	{
		ExecuteStopTimeDemoCommands(InWorld);
		StopTimeDemo();
		bStartedDemo = false;
		return true;
	}

	return false;
}

void AUTTimeDemoGameMode::Tick(float DeltaTime)
{
	if (TimeLeft > 0)
	{
		TimeLeft -= DeltaTime;
		if (TimeLeft < 0)
		{
			ExecuteStopTimeDemoCommands(GetWorld());
			StopTimeDemo();
			bStartedDemo = false;
		}
	}
}

void AUTTimeDemoGameMode::ExecuteStartTimeDemoCommands(UWorld* InWorld)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(InWorld));
	if (PC)
	{
		for (const FString& Command : CommandsToRunAtStart)
		{
			PC->ConsoleCommand(Command);
		}
	}
}

void AUTTimeDemoGameMode::ExecuteStopTimeDemoCommands(UWorld* InWorld)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(InWorld));
	if (PC)
	{
		for (const FString& Command : CommandsToRunAtStop)
		{
			PC->ConsoleCommand(Command);
		}
	}
}