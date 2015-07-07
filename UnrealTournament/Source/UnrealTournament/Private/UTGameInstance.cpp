// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameInstance.h"
#include "UnrealNetwork.h"

UUTGameInstance::UUTGameInstance(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTGameInstance::StartGameInstance()
{
	Super::StartGameInstance();

	UWorld* World = GetWorld();
	if (World)
	{
		FTimerHandle UnusedHandle;
		World->GetTimerManager().SetTimer(UnusedHandle, FTimerDelegate::CreateUObject(this, &UUTGameInstance::DeferredStartGameInstance), 0.1f, false);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Failed to schedule DeferredStartGameInstance because the world was null during StartGameInstance"));
	}
}

void UUTGameInstance::DeferredStartGameInstance()
{
#if !UE_SERVER
	// Autodetect detail settings, if needed
	UUTGameUserSettings* Settings = CastChecked<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetFirstGamePlayer());
	if (LocalPlayer)
	{
		Settings->BenchmarkDetailSettingsIfNeeded(LocalPlayer);
	}
#endif
}

// @TODO FIXMESTEVE - we want open to be relative like it used to be
bool UUTGameInstance::HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	// the netcode adds implicit "?game=" to network URLs that we don't want when going from online game to local game
	if (WorldContext != NULL && !WorldContext->LastURL.IsLocalInternal())
	{
		WorldContext->LastURL.RemoveOption(TEXT("game="));
	}
	return GEngine->HandleTravelCommand(Cmd, Ar, InWorld);
}

void UUTGameInstance::HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString )
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>( GetFirstGamePlayer() );

	if ( LocalPlayer != nullptr )
	{
#if !UE_SERVER
		LocalPlayer->ReturnToMainMenu();
		LocalPlayer->ShowMessage( 
			NSLOCTEXT( "UUTGameInstance", "ReplayErrorTitle", "Replay Error" ),
			NSLOCTEXT( "UUTGameInstance", "ReplayErrorMessage", "There was an error with the replay. Returning to the main menu." ), UTDIALOG_BUTTON_OK, nullptr
		);
#endif
	}
}