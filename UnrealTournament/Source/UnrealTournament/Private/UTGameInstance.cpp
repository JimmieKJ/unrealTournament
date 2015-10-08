// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameInstance.h"
#include "UnrealNetwork.h"
#include "Slate/SUWRedirectDialog.h"
#include "UTDemoNetDriver.h"

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
	// skip this if the "failure" is waiting for redirects
	if (FailureType != EDemoPlayFailure::Generic || LastTriedDemo.IsEmpty() || bRetriedDemoAfterRedirects)
	{
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetFirstGamePlayer());

		if (LocalPlayer != nullptr)
		{
#if !UE_SERVER
			LocalPlayer->ReturnToMainMenu();
			LocalPlayer->ShowMessage(
				NSLOCTEXT("UUTGameInstance", "ReplayErrorTitle", "Replay Error"),
				NSLOCTEXT("UUTGameInstance", "ReplayErrorMessage", "There was an error with the replay. Returning to the main menu."), UTDIALOG_BUTTON_OK, nullptr
				);
#endif
		}
	}
}

bool UUTGameInstance::IsAutoDownloadingContent()
{
#if !UE_SERVER
	for (int32 i = ActiveRedirectDialogs.Num() - 1; i >= 0; i--)
	{
		if (!ActiveRedirectDialogs[i].IsValid())
		{
			ActiveRedirectDialogs.RemoveAt(i);
		}
	}
	return ActiveRedirectDialogs.Num() > 0;
#else
	return false;
#endif
}

void UUTGameInstance::StartRedirectDownload(const FString& URL)
{
#if !UE_SERVER
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetFirstGamePlayer());
	if (LocalPlayer != NULL)
	{
		TSharedRef<SUWRedirectDialog> Dialog = SNew(SUWRedirectDialog)
			.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTGameInstance::RedirectResult))
			.DialogTitle(NSLOCTEXT("UTGameViewportClient", "Redirect", "Download"))
			.RedirectToURL(URL)
			.PlayerOwner(LocalPlayer);
		ActiveRedirectDialogs.Add(Dialog);
		LocalPlayer->OpenDialog(Dialog);
	}
#endif
}

void UUTGameInstance::HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr)
{
	switch (MessageByte)
	{
		case UNMT_Redirect:
		{
			StartRedirectDownload(MessageStr);
			break;
		}
		default:
			UE_LOG(UT, Warning, TEXT("Unexpected net control message of type %i"), int32(MessageByte));
			break;
	}
}

void UUTGameInstance::RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
#if !UE_SERVER
	if (Widget.IsValid())
	{
		ActiveRedirectDialogs.Remove(StaticCastSharedPtr<SUWRedirectDialog>(Widget));
	}
	if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		GEngine->Exec(GetWorld(), TEXT("DISCONNECT"));
		LastTriedDemo.Empty();
		bRetriedDemoAfterRedirects = false;
	}
	else if (ActiveRedirectDialogs.Num() == 0 && !bRetriedDemoAfterRedirects)
	{
		bRetriedDemoAfterRedirects = true;
		GEngine->Exec(GetWorld(), *FString::Printf(TEXT("DEMOPLAY %s"), *LastTriedDemo));
	}
#endif
}

void UUTGameInstance::StartRecordingReplay(const FString& Name, const FString& FriendlyName)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("NOREPLAYS")))
	{
		UE_LOG(UT, Warning, TEXT("UGameInstance::StartRecordingReplay: Rejected due to -noreplays option"));
		return;
	}

	UWorld* CurrentWorld = GetWorld();

	if (CurrentWorld == nullptr)
	{
		UE_LOG(UT, Warning, TEXT("UGameInstance::StartRecordingReplay: GetWorld() is null"));
		return;
	}

	FURL DemoURL;
	FString DemoName = Name;

	DemoName.ReplaceInline(TEXT("%m"), *CurrentWorld->GetMapName());

	// replace the current URL's map with a demo extension
	DemoURL.Map = DemoName;
	DemoURL.AddOption(*FString::Printf(TEXT("DemoFriendlyName=%s"), *FriendlyName));
	DemoURL.AddOption(*FString::Printf(TEXT("Remote")));

	CurrentWorld->DestroyDemoNetDriver();

	const FName NAME_DemoNetDriver(TEXT("DemoNetDriver"));

	if (!GEngine->CreateNamedNetDriver(CurrentWorld, NAME_DemoNetDriver, NAME_DemoNetDriver))
	{
		UE_LOG(UT, Warning, TEXT("RecordReplay: failed to create demo net driver!"));
		return;
	}

	CurrentWorld->DemoNetDriver = Cast< UDemoNetDriver >(GEngine->FindNamedNetDriver(CurrentWorld, NAME_DemoNetDriver));

	check(CurrentWorld->DemoNetDriver != NULL);

	CurrentWorld->DemoNetDriver->SetWorld(CurrentWorld);

	FString Error;

	if (!CurrentWorld->DemoNetDriver->InitListen(CurrentWorld, DemoURL, false, Error))
	{
		UE_LOG(UT, Warning, TEXT("Demo recording failed: %s"), *Error);
		CurrentWorld->DemoNetDriver = NULL;
	}
	else
	{
		UE_LOG(UT, VeryVerbose, TEXT("Num Network Actors: %i"), CurrentWorld->NetworkActors.Num());
	}
}
void UUTGameInstance::PlayReplay(const FString& Name)
{
	UWorld* CurrentWorld = GetWorld();

	if (CurrentWorld == nullptr)
	{
		UE_LOG(UT, Warning, TEXT("UGameInstance::PlayReplay: GetWorld() is null"));
		return;
	}

	CurrentWorld->DestroyDemoNetDriver();

	FURL DemoURL;
	UE_LOG(UT, Log, TEXT("PlayReplay: Attempting to play demo %s"), *Name);

	DemoURL.Map = Name;
	DemoURL.AddOption(*FString::Printf(TEXT("Remote")));

	const FName NAME_DemoNetDriver(TEXT("DemoNetDriver"));

	if (!GEngine->CreateNamedNetDriver(CurrentWorld, NAME_DemoNetDriver, NAME_DemoNetDriver))
	{
		UE_LOG(UT, Warning, TEXT("PlayReplay: failed to create demo net driver!"));
		return;
	}

	CurrentWorld->DemoNetDriver = Cast< UDemoNetDriver >(GEngine->FindNamedNetDriver(CurrentWorld, NAME_DemoNetDriver));

	check(CurrentWorld->DemoNetDriver != NULL);

	CurrentWorld->DemoNetDriver->SetWorld(CurrentWorld);

	FString Error;

	if (!CurrentWorld->DemoNetDriver->InitConnect(CurrentWorld, DemoURL, Error))
	{
		UE_LOG(UT, Warning, TEXT("Demo playback failed: %s"), *Error);
		CurrentWorld->DestroyDemoNetDriver();
	}
	else
	{
		FCoreUObjectDelegates::PostDemoPlay.Broadcast();
	}
}