// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Player.cpp: Unreal player implementation.
=============================================================================*/
 
#include "EnginePrivate.h"

#include "SubtitleManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/GameInstance.h"

#include "RenderCore.h"
#include "ColorList.h"
#include "SlateBasics.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameState.h"
#include "GameFramework/GameMode.h"

//////////////////////////////////////////////////////////////////////////
// UPlayer

UPlayer::UPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPlayer::Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(PlayerController)
	{
		// Since UGameViewportClient calls Exec on UWorld, we only need to explicitly
		// call UWorld::Exec if we either have a null GEngine or a null ViewportClient
		UWorld* World = PlayerController->GetWorld();
		check(World);
		check(InWorld == NULL || InWorld == World);
		const bool bWorldNeedsExec = GEngine == NULL || Cast<ULocalPlayer>(this) == NULL || static_cast<ULocalPlayer*>(this)->ViewportClient == NULL;
		APawn* PCPawn = PlayerController->GetPawnOrSpectator();
		if( bWorldNeedsExec && World->Exec(World, Cmd,Ar) )
		{
			return true;
		}
		else if( PlayerController->PlayerInput && PlayerController->PlayerInput->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PlayerController->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PCPawn && PCPawn->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PlayerController->MyHUD && PlayerController->MyHUD->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( World->GetGameInstance() && World->GetGameInstance()->ProcessConsoleExec(Cmd, Ar, PCPawn) )
		{
			return true;
		}
		else if( World->GetAuthGameMode() && World->GetAuthGameMode()->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PlayerController->CheatManager && PlayerController->CheatManager->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if (World->GameState && World->GameState->ProcessConsoleExec(Cmd, Ar, PCPawn))
		{
			return true;
		}
		else if (PlayerController->PlayerCameraManager && PlayerController->PlayerCameraManager->ProcessConsoleExec(Cmd, Ar, PCPawn))
		{
			return true;
		}
	}
	return false;
}

void UPlayer::SwitchController(class APlayerController* PC)
{
	// Detach old player.
	if( this->PlayerController )
	{
		this->PlayerController->Player = NULL;
	}

	// Set the viewport.
	PC->Player = this;
	this->PlayerController = PC;
}
