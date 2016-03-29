// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameInstance.h"
#include "UnrealNetwork.h"
#include "Dialogs/SUTRedirectDialog.h"
#include "UTDemoNetDriver.h"
#include "UTGameEngine.h"
#include "UTGameViewportClient.h"
#include "UTMatchmaking.h"
#include "UTParty.h"
#include "UTPartyGameState.h"
#include "UTPlaylistManager.h"

/* Delays for various timers during matchmaking */
#define DELETESESSION_DELAY 1.0f

UUTGameInstance::UUTGameInstance(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Matchmaking(nullptr)
	, Party(nullptr)
{
}

void UUTGameInstance::Init()
{
	Super::Init();
	InitPerfCounters();

	IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();
	if (PerfCounters)
	{
		// Attach a handler for exec commands passed in via the perf counter query port
		PerfCounters->OnPerfCounterExecCommand() = FPerfCounterExecCommandCallback::CreateUObject(this, &ThisClass::PerfExecCmd);
	}

	if (!IsDedicatedServerInstance())
	{
		// handles all matchmaking in game
		Matchmaking = NewObject<UUTMatchmaking>(this);
		check(Matchmaking);

		Party = NewObject<UUTParty>(this);
		check(Party);

		// Initialize both after construction (each needs the pointer of the other)
		Matchmaking->Init();

		if (Party)
		{
			Party->Init();
		}
		PlaylistManager = NewObject<UUTPlaylistManager>(this);
	}
	else
	{
		PlaylistManager = NewObject<UUTPlaylistManager>(this);
	}
}

bool UUTGameInstance::PerfExecCmd(const FString& ExecCmd, FOutputDevice& Ar)
{

	FWorldContext* WorldContext = GetWorldContext();
	if (WorldContext)
	{
		UWorld* World = WorldContext->World();
		if (World)
		{
			if (GEngine->Exec(World, *ExecCmd, Ar))
			{
				return true;
			}
			Ar.Log(FString::Printf(TEXT("ExecCmd %s not found"), *ExecCmd));
			return false;
		}
	}

	Ar.Log(FString::Printf(TEXT("WorldContext for ExecCmd %s not found"), *ExecCmd));
	return false;
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

	// Invalidate the LastSession member in UTLocalPlayer since we connected via IP.  NOTE: clients will attempt to rejoin the session
	// of servers connected to the internet and at that point LastSession will become valid again.
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetFirstGamePlayer());
	if (LocalPlayer != nullptr)
	{
		LocalPlayer->InvalidateLastSession();
		LocalPlayer->LastConnectToIP = LocalPlayer->StripOptionsFromAddress(Cmd);
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

bool UUTGameInstance::StartRedirectDownload(const FString& PakName, const FString& URL, const FString& Checksum)
{
#if !UE_SERVER
	UUTGameViewportClient* Viewport = Cast<UUTGameViewportClient>(GetGameViewportClient());
	if (Viewport != NULL && !Viewport->CheckIfRedirectExists(FPackageRedirectReference(PakName, TEXT(""), TEXT(""), Checksum)))
	{
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetFirstGamePlayer());
		if (LocalPlayer != NULL)
		{
			TSharedRef<SUTRedirectDialog> Dialog = SNew(SUTRedirectDialog)
				.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTGameInstance::RedirectResult))
				.DialogTitle(NSLOCTEXT("UTGameViewportClient", "Redirect", "Download"))
				.RedirectToURL(URL)
				.PlayerOwner(LocalPlayer);
			ActiveRedirectDialogs.Add(Dialog);
			LocalPlayer->OpenDialog(Dialog);

			return true;
		}
		else
		{
			return false;
		}
	}
	else
#endif
	{
		return false;
	}
}

void UUTGameInstance::HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr)
{
	switch (MessageByte)
	{
		case UNMT_Redirect:
		{
			UE_LOG(UT, Verbose, TEXT("Received redirect request: %s"), *MessageStr);
			TArray<FString> Pieces;
			MessageStr.ParseIntoArray(Pieces, TEXT("\n"), false);
			if (Pieces.Num() == 3)
			{
				// 0: pak name, 1: URL, 2: checksum
				StartRedirectDownload(Pieces[0], Pieces[1], Pieces[2]);
			}
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
		ActiveRedirectDialogs.Remove(StaticCastSharedPtr<SUTRedirectDialog>(Widget));
	}
	if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		GEngine->Exec(GetWorld(), TEXT("DISCONNECT"));
		LastTriedDemo.Empty();
		bRetriedDemoAfterRedirects = false;
	}
	else if (ActiveRedirectDialogs.Num() == 0 && !bRetriedDemoAfterRedirects && !LastTriedDemo.IsEmpty())
	{
		bRetriedDemoAfterRedirects = true;
		GEngine->Exec(GetWorld(), *FString::Printf(TEXT("DEMOPLAY %s"), *LastTriedDemo));
	}
#endif
}

void UUTGameInstance::StartRecordingReplay(const FString& Name, const FString& FriendlyName, const TArray<FString>& AdditionalOptions)
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
		//UE_LOG(UT, VeryVerbose, TEXT("Num Network Actors: %i"), CurrentWorld->NetworkActors.Num());
	}
}
void UUTGameInstance::PlayReplay(const FString& Name, UWorld* WorldOverride, const TArray<FString>& AdditionalOptions)
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


void UUTGameInstance::Shutdown()
{
	if (Party)
	{
		Party->OnShutdown();
	}
	
	Super::Shutdown();
}

/*static*/ UUTGameInstance* UUTGameInstance::Get(UObject* ContextObject)
{
	UUTGameInstance* GameInstance = NULL;
	UWorld* OwningWorld = GEngine->GetWorldFromContextObject(ContextObject, false);

	if (OwningWorld)
	{
		GameInstance = Cast<UUTGameInstance>(OwningWorld->GetGameInstance());
	}
	return GameInstance;
}

UUTMatchmaking* UUTGameInstance::GetMatchmaking() const
{
	return Matchmaking;
}

UUTParty* UUTGameInstance::GetParties() const
{
	return Party;
}

UUTPlaylistManager* UUTGameInstance::GetPlaylistManager() const
{
	return PlaylistManager;
}

bool UUTGameInstance::IsInSession(const FUniqueNetId& SessionId) const
{
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(/*World*/);
	if (SessionInt.IsValid())
	{
		FNamedOnlineSession* Session = SessionInt->GetNamedSession(GameSessionName);
		if (Session && Session->SessionInfo.IsValid() && Session->SessionInfo->GetSessionId() == SessionId)
		{
			return true;
		}
	}
	return false;
}

void UUTGameInstance::SafeSessionDelete(FName SessionName, FOnDestroySessionCompleteDelegate DestroySessionComplete)
{
	UWorld* World = GetWorld();
	check(World);

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(/*World*/);
	if (SessionInt.IsValid())
	{
		EOnlineSessionState::Type SessionState = SessionInt->GetSessionState(SessionName);
		if (SessionState != EOnlineSessionState::NoSession)
		{
			if (SessionState != EOnlineSessionState::Destroying)
			{
				if (SessionState != EOnlineSessionState::Creating &&
					SessionState != EOnlineSessionState::Ending)
				{
					SafeSessionDeleteTimerHandle.Invalidate();

					FOnDestroySessionCompleteDelegate CompletionDelegate;
					CompletionDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UUTGameInstance::OnDeleteSessionComplete, DestroySessionComplete);

					DeleteSessionDelegateHandle = SessionInt->AddOnDestroySessionCompleteDelegate_Handle(CompletionDelegate);
					SessionInt->DestroySession(SessionName);
				}
				else
				{
					if (!SafeSessionDeleteTimerHandle.IsValid())
					{
						// Retry shortly
						FTimerDelegate RetryDelegate;
						RetryDelegate.BindUObject(this, &UUTGameInstance::SafeSessionDelete, SessionName, DestroySessionComplete);
						World->GetTimerManager().SetTimer(SafeSessionDeleteTimerHandle, RetryDelegate, DELETESESSION_DELAY, false);
					}
					else
					{
						// Timer already in flight
						TArray<FOnDestroySessionCompleteDelegate>& DestroyDelegates = PendingDeletionDelegates.FindOrAdd(SessionName);
						DestroyDelegates.Add(DestroySessionComplete);
					}
				}
			}
			else
			{
				// Destroy already in flight
				TArray<FOnDestroySessionCompleteDelegate>& DestroyDelegates = PendingDeletionDelegates.FindOrAdd(SessionName);
				DestroyDelegates.Add(DestroySessionComplete);
			}
		}
		else
		{
			UE_LOG(LogOnlineGame, Verbose, TEXT("SafeSessionDelete called on session %s in state %s, skipping."), *SessionName.ToString(), EOnlineSessionState::ToString(SessionState));
			DestroySessionComplete.ExecuteIfBound(SessionName, false);
		}

		return;
	}

	// Non shipping builds can return success, otherwise fail
	bool bWasSuccessful = !UE_BUILD_SHIPPING;
	DestroySessionComplete.ExecuteIfBound(SessionName, bWasSuccessful);
}

void UUTGameInstance::OnDeleteSessionComplete(FName SessionName, bool bWasSuccessful, FOnDestroySessionCompleteDelegate DestroySessionComplete)
{
	UE_LOG(LogOnlineGame, Verbose, TEXT("OnDeleteSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	SafeSessionDeleteTimerHandle.Invalidate();

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(/*GetWorld()*/);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnDestroySessionCompleteDelegate_Handle(DeleteSessionDelegateHandle);
	}

	DestroySessionComplete.ExecuteIfBound(SessionName, bWasSuccessful);

	TArray<FOnDestroySessionCompleteDelegate> DelegatesCopy;
	if (PendingDeletionDelegates.RemoveAndCopyValue(SessionName, DelegatesCopy))
	{
		// All other delegates captured while destroy was in flight
		for (const FOnDestroySessionCompleteDelegate& ExtraDelegate : DelegatesCopy)
		{
			ExtraDelegate.ExecuteIfBound(SessionName, bWasSuccessful);
		}
	}
}

bool UUTGameInstance::ClientTravelToSession(int32 ControllerId, FName InSessionName)
{
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(/*World*/);
	if (SessionInt.IsValid())
	{
		FString URL;
		if (SessionInt->GetResolvedConnectString(InSessionName, URL))
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), ControllerId);
			if (PC)
			{
				UUTParty* Parties = GetParties();
				if (Parties)
				{
					UUTPartyGameState* PartyGameState = Parties->GetUTPersistentParty();
					if (PartyGameState)
					{
						URL += TEXT("?PartySize=") + FString::FromInt(PartyGameState->GetPartySize());
					}
					//Parties->NotifyPreClientTravel();
				}

				PC->ClientTravel(URL, TRAVEL_Absolute);
				return true;
			}
		}
		else
		{
			UE_LOG(LogOnlineGame, Warning, TEXT("Failed to resolve session connect string for %s"), *InSessionName.ToString());
		}
	}
	else
	{
		UE_LOG(LogOnlineGame, Warning, TEXT("ClientTravelToSession: No online subsystem"));
	}

	return false;
}