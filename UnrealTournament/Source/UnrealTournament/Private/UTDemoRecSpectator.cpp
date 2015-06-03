// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTDemoRecSpectator.h"
#include "UTDemoNetDriver.h"

AUTDemoRecSpectator::AUTDemoRecSpectator(const FObjectInitializer& OI)
: Super(OI)
{
}

void AUTDemoRecSpectator::ViewPlayerState(APlayerState* PS)
{
	// we have to redirect back to the Pawn because engine hardcoded FTViewTarget code will reject a PlayerState with NULL owner
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (It->IsValid() && It->Get()->PlayerState == PS)
		{
			SetViewTarget(It->Get());
		}
	}
}

void AUTDemoRecSpectator::ViewSelf(FViewTargetTransitionParams TransitionParams)
{
	ServerViewSelf_Implementation(TransitionParams);
}

void AUTDemoRecSpectator::ViewAPlayer(int32 dir)
{
	BehindView(bSpectateBehindView);

	APlayerState* const PS = GetNextViewablePlayer(dir);
	if (PlayerState != NULL)
	{
		ViewPlayerState(PS);
	}
}
APlayerState* AUTDemoRecSpectator::GetNextViewablePlayer(int32 dir)
{
	int32 CurrentIndex = -1;
	if (PlayerCameraManager->ViewTarget.GetTargetPawn() != NULL)
	{
		APlayerState* TestPS = PlayerCameraManager->ViewTarget.GetTargetPawn()->PlayerState;
		// Find index of current viewtarget's PlayerState
		for (int32 i = 0; i < GetWorld()->GameState->PlayerArray.Num(); i++)
		{
			if (TestPS == GetWorld()->GameState->PlayerArray[i])
			{
				CurrentIndex = i;
				break;
			}
		}
	}

	// Find next valid viewtarget in appropriate direction
	int32 NewIndex;
	for (NewIndex = CurrentIndex + dir; (NewIndex >= 0) && (NewIndex < GetWorld()->GameState->PlayerArray.Num()); NewIndex = NewIndex + dir)
	{
		APlayerState* const PlayerState = GetWorld()->GameState->PlayerArray[NewIndex];
		if (PlayerState != NULL && !PlayerState->bOnlySpectator)
		{
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				if (It->IsValid() && It->Get()->PlayerState == PlayerState)
				{
					return PlayerState;
				}
			}
		}
	}

	// wrap around
	CurrentIndex = (NewIndex < 0) ? GetWorld()->GameState->PlayerArray.Num() : -1;
	for (NewIndex = CurrentIndex + dir; (NewIndex >= 0) && (NewIndex < GetWorld()->GameState->PlayerArray.Num()); NewIndex = NewIndex + dir)
	{
		APlayerState* const PlayerState = GetWorld()->GameState->PlayerArray[NewIndex];
		if (PlayerState != NULL && !PlayerState->bOnlySpectator)
		{
			for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
			{
				if (It->IsValid() && It->Get()->PlayerState == PlayerState)
				{
					return PlayerState;
				}
			}
		}
	}

	return NULL;
}

void AUTDemoRecSpectator::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	if (GetWorld()->IsServer())
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game != NULL)
		{
			TSubclassOf<UUTScoreboard> ScoreboardClass = LoadClass<UUTScoreboard>(NULL, *Game->ScoreboardClassName.AssetLongPathname, NULL, LOAD_None, NULL);
			ClientSetHUDAndScoreboard(Game->HUDClass, ScoreboardClass);
		}
	}
}

bool AUTDemoRecSpectator::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	// if we're the demo server, record into the demo
	UNetDriver* NetDriver = GetWorld()->DemoNetDriver;
	if (NetDriver != NULL && NetDriver->ServerConnection == NULL)
	{
		// HACK: due to engine issues it's not really a UUTDemoNetDriver and this is an evil hack to access the protected InternalProcessRemoteFunction()
		//NetDriver->ProcessRemoteFunction(this, Function, Parameters, OutParms, Stack, NULL);
		((UUTDemoNetDriver*)NetDriver)->HackProcessRemoteFunction(this, Function, Parameters, OutParms, Stack, NULL);
		return true;
	}
	else
	{
		return false;
	}
}