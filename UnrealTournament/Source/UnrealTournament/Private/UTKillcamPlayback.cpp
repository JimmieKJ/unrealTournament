// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTKillcamPlayback.h"
#include "UTGameInstance.h"
#include "Components/ModelComponent.h"
#include "NetworkReplayStreaming.h"
#include "UnrealNetwork.h"
#include "UTLocalPlayer.h"
#include "UTGameViewportClient.h"
#include "UTDemoRecSpectator.h"
#include "UTDemoNetDriver.h"
#include "UTProj_Redeemer.h"
#include "UTRemoteRedeemer.h"
#include "UTCharacter.h"
#include "UTRallyPoint.h"

TAutoConsoleVariable<int32> CVarUTEnableInstantReplay(
	TEXT("UT.EnableInstantReplay"),
	0,
	TEXT("Enables creation of a separate world for instant replay playback.\n"));

TAutoConsoleVariable<float> CVarUTKillcamRewindTime(
	TEXT("UT.KillcamRewindTime"),
	3.5f,
	TEXT("Number of seconds to rewind the killcam for playback."));

TAutoConsoleVariable<float> CVarUTCoolMomentRewindTime(
	TEXT("UT.CoolMomentRewindTime"),
	9.5f,
	TEXT("Number of seconds to rewind the cool moments for playback."));

int32 UUTKillcamPlayback::NumWorldsCreated = 0;

UUTKillcamPlayback::UUTKillcamPlayback()
	: KillcamWorld(nullptr)
	, KillcamWorldPackage(nullptr)
	, bIsEnabled(false)
	, OnWorldCleanupHandle(FWorldDelegates::OnWorldCleanup.AddUObject(this, &UUTKillcamPlayback::CleanUpKillcam))
{
}

DEFINE_LOG_CATEGORY_STATIC(LogUTKillcam, Log, All);

void UUTKillcamPlayback::BeginDestroy()
{
	FWorldDelegates::OnWorldCleanup.Remove(OnWorldCleanupHandle);

	if (GEngine != nullptr)
	{
		FWorldContext* KillcamContext = GEngine->GetWorldContextFromWorld(KillcamWorld);
		if (KillcamContext != nullptr)
		{
			KillcamContext->RemoveRef(KillcamWorld);
		}
	}

	Super::BeginDestroy();
}

void UUTKillcamPlayback::CreateKillcamWorld(const FURL& InURL, const FWorldContext& SourceWorldContext)
{
	if (CVarUTEnableInstantReplay.GetValueOnGameThread() == 0)
	{
		return;
	}
	
	// Prevent recursively creating killcam worlds
	if (SourceWorldContext.World() == nullptr || (SourceWorldContext.World() == KillcamWorld))
	{
		return;
	}
	
	// Spawn killcam world if client
	if (SourceWorldContext.World()->GetNetMode() != NM_Client)
	{
		return;
	}

	if (SourceWorldContext.WorldType == EWorldType::PIE)
	{
		UE_LOG(LogUTKillcam, Log, TEXT("UUTKillcamPlayback::CreateKillcamWorld: Killcam recording is not supported for PIE."));
		return;
	}

	// Create WorldContext and World for killcam.
	if (SourceWorldContext.OwningGameInstance != nullptr)
	{
		UUTGameInstance* UTGameInstance = Cast<UUTGameInstance>(SourceWorldContext.OwningGameInstance);
		if (UTGameInstance)
		{
			UTGameInstance->bIgnoreLevelLoad = true;
		}
		SourceWorld = SourceWorldContext.World();

		// Creates the world context. This should be the only WorldContext that ever gets created for this GameInstance.
		FWorldContext& NewContext = SourceWorldContext.OwningGameInstance->GetEngine()->CreateNewWorldContext(EWorldType::Game);
		NewContext.OwningGameInstance = SourceWorldContext.OwningGameInstance;
		NewContext.GameViewport = SourceWorldContext.OwningGameInstance->GetGameViewportClient();
		NewContext.AddRef(KillcamWorld);

		++NumWorldsCreated;
		NewContext.PIEInstance = NumWorldsCreated;

		// Create a dummy world from the beginning to avoid issues of not having a world until LoadMap gets us our real world
		UWorld* TempWorld = UWorld::CreateWorld(EWorldType::Game, false);
		TempWorld->SetGameInstance(SourceWorldContext.OwningGameInstance);
		NewContext.SetCurrentWorld(TempWorld);
	}
}

void UUTKillcamPlayback::PlayKillcamReplay(const FString& ReplayUniqueName)
{
	if (CVarUTEnableInstantReplay.GetValueOnGameThread() == 0)
	{
		return;
	}

	if (KillcamWorld == nullptr)
	{
		UE_LOG(LogUTKillcam, Log, TEXT("UUTKillcamPlayback::PlayKillcamReplay: KillcamWorld is null."));
		return;
	}
	
	if (SourceWorld.IsValid() && SourceWorld->GetNetMode() != NM_Client)
	{
		return;
	}

	UGameInstance* GameInstance = KillcamWorld->GetGameInstance();
	check(GameInstance != nullptr);

	if (GameInstance->GetWorldContext() != nullptr && GameInstance->GetWorldContext()->WorldType == EWorldType::PIE)
	{
		UE_LOG(LogUTKillcam, Log, TEXT("UUTKillcamPlayback::PlayKillcamReplay: Killcam playback is not supported for PIE."));
		return;
	}

	TArray<FString> AdditionalOptions;
	AdditionalOptions.Add(TEXT("ReplayStreamerOverride=") + GetKillcamReplayStreamerName());
	// Currently, loading the killcam world seamlessly is not supported, so force a synchronous load for now
	AdditionalOptions.Add(TEXT("AsyncLoadWorldOverride=0"));

	GameInstance->PlayReplay(ReplayUniqueName, KillcamWorld, AdditionalOptions);

	UWorld* CachedSourceWorld = SourceWorld.Get();
	if (CachedSourceWorld)
	{
		UUTDemoNetDriver* UTDemoNetDriver = Cast<UUTDemoNetDriver>(CachedSourceWorld->DemoNetDriver);
		if (UTDemoNetDriver)
		{
			UTDemoNetDriver->bIsLocalReplay = true;
		}
	}
}

void UUTKillcamPlayback::KillcamStart(const float RewindDemoSeconds, const FNetworkGUID FocusActorGUID)
{
	UE_LOG(LogUTKillcam, Log, TEXT("UUTKillcamPlayback::KillcamStart: %f"), RewindDemoSeconds);

	UWorld* CachedSourceWorld = SourceWorld.Get();
	if (CachedSourceWorld)
	{
		const float AbsoluteTimeSeconds = CachedSourceWorld->DemoNetDriver->DemoCurrentTime - RewindDemoSeconds;

		KillcamGoToTime(
			AbsoluteTimeSeconds,
			FocusActorGUID,
			FOnGotoTimeDelegate::CreateUObject(this, &UUTKillcamPlayback::OnKillcamReady, FocusActorGUID));
	}
}

void UUTKillcamPlayback::CoolMomentCamStart(const float RewindDemoSeconds, const FUniqueNetIdRepl FocusActorNetId)
{
	UE_LOG(LogUTKillcam, Log, TEXT("UUTKillcamPlayback::CoolMomentCamStart: %f"), RewindDemoSeconds);

	UWorld* CachedSourceWorld = SourceWorld.Get();
	if (CachedSourceWorld)
	{
		const float AbsoluteTimeSeconds = CachedSourceWorld->DemoNetDriver->DemoCurrentTime - RewindDemoSeconds;

		KillcamGoToTime(
			AbsoluteTimeSeconds,
			FNetworkGUID(),
			FOnGotoTimeDelegate::CreateUObject(this, &UUTKillcamPlayback::OnCoolMomentCamReady, FocusActorNetId));
	}
}

void UUTKillcamPlayback::KillcamGoToTime(const float TimeInSeconds, const FNetworkGUID FocusActor, const FOnGotoTimeDelegate& InOnGotoTimeDelegate)
{
	UE_LOG(LogUTKillcam, Log, TEXT("UUTKillcamPlayback::KillcamGoToTime: %f"), TimeInSeconds);
	if (KillcamWorld != nullptr && KillcamWorld->DemoNetDriver != nullptr && SourceWorld != nullptr && SourceWorld->DemoNetDriver != nullptr)
	{
		CachedGoToTimeSeconds = TimeInSeconds;
		CachedFocusActorGUID = FocusActor;

		if (KillcamWorld->GetWorldSettings() != nullptr)
		{
			KillcamWorld->GetWorldSettings()->Pauser = nullptr;
		}
		
		// Add the replay spectator's GUID and its pawn's GUID to the non-queued list, so that it's
		// available to the game when the GotoTime delegate executes. We can't rely on the DemoNetDriver
		// to this in this case because the first time through the SpectatorController in
		// the killcam world can be null.
		if(SourceWorld.IsValid() && SourceWorld->DemoNetDriver != nullptr)
		{
			const APlayerController* Spectator = SourceWorld->DemoNetDriver->SpectatorController;
			const FNetworkGUID SpectatorGUID = SourceWorld->DemoNetDriver->GetGUIDForActor(Spectator);
			KillcamWorld->DemoNetDriver->AddNonQueuedGUIDForScrubbing(SpectatorGUID);
			
			if (Spectator != nullptr)
			{
				const FNetworkGUID SpectatorPawnGUID = SourceWorld->DemoNetDriver->GetGUIDForActor(Spectator->GetPawn());
				KillcamWorld->DemoNetDriver->AddNonQueuedGUIDForScrubbing(SpectatorPawnGUID);
			}

			// Don't queue bunches for always relevant actors
			for ( FActorIterator It( SourceWorld.Get() ); It; ++It )
			{
				if ( It->bAlwaysRelevant )
				{
					const FNetworkGUID ActorGUID = SourceWorld->DemoNetDriver->GetGUIDForActor(*It);
					KillcamWorld->DemoNetDriver->AddNonQueuedGUIDForScrubbing(ActorGUID);
				}
			}
		}

		KillcamWorld->DemoNetDriver->AddNonQueuedGUIDForScrubbing(FocusActor);
		KillcamWorld->DemoNetDriver->GotoTimeInSeconds(TimeInSeconds,
			FOnGotoTimeDelegate::CreateUObject(this, &UUTKillcamPlayback::OnKillcamGoToTimeComplete, InOnGotoTimeDelegate));
	}
}

void UUTKillcamPlayback::OnKillcamGoToTimeComplete(bool bWasSuccessful, FOnGotoTimeDelegate UserDelegate)
{
	if (bWasSuccessful)
	{
		ShowKillcamToUser();
	}
	else
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("UUTKillcamPlayback::OnKillcamGoToTimeComplete failed"));
	}

	UserDelegate.ExecuteIfBound(bWasSuccessful);
}

FString UUTKillcamPlayback::GetKillcamReplayStreamerName()
{
	return TEXT("InMemoryNetworkReplayStreaming");
}

bool UUTKillcamPlayback::IsKillcamWorld(const UGameInstance* const GameInstance, const UWorld* const World)
{
	if (GameInstance != nullptr && World != nullptr)
	{
		for (auto PlayerIt = GameInstance->GetLocalPlayerIterator(); PlayerIt; ++PlayerIt)
		{
			const UUTLocalPlayer* const Player = Cast<UUTLocalPlayer>(*PlayerIt);
			if (Player && Player->GetKillcamPlaybackManager()->GetKillcamWorld() == World)
			{
				return true;
			}
		}
	}

	return false;
}

bool UUTKillcamPlayback::IsEnabled() const
{
	return bIsEnabled;
}

void UUTKillcamPlayback::KillcamStop()
{
	UE_LOG(LogUTKillcam, Verbose, TEXT("UUTKillcamPlayback::KillcamStop"));

	if (KillcamWorld != nullptr && KillcamWorld->DemoNetDriver != nullptr)
	{
		if (KillcamWorld->GetWorldSettings() != nullptr && KillcamWorld->DemoNetDriver->SpectatorController != nullptr)
		{
			KillcamWorld->GetWorldSettings()->Pauser = KillcamWorld->DemoNetDriver->SpectatorController->PlayerState;
		}

		HideKillcamFromUser();
	}
	
	bIsEnabled = false;
}

void UUTKillcamPlayback::KillcamRestart()
{
	KillcamGoToTime(
		CachedGoToTimeSeconds,
		CachedFocusActorGUID,
		FOnGotoTimeDelegate::CreateUObject(this, &UUTKillcamPlayback::OnKillcamReady, CachedFocusActorGUID));
}

void UUTKillcamPlayback::CleanUpKillcam(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if ( !SourceWorld.IsValid() || SourceWorld.Get() != World )
	{
		return;
	}

	FWorldContext* SourceWorldContext = GEngine->GetWorldContextFromWorld(World);
	if (SourceWorldContext != nullptr && KillcamWorld != nullptr)
	{
		check(SourceWorldContext->OwningGameInstance != nullptr);
		check(SourceWorldContext->OwningGameInstance->GetEngine() != nullptr);

		for (FActorIterator ActorIt(KillcamWorld); ActorIt; ++ActorIt)
		{
			// Is LevelTransition the best reason to use here?
			ActorIt->RouteEndPlay(EEndPlayReason::LevelTransition);
		}

		KillcamWorld->CleanupWorld();

		if( GEngine )
		{
			GEngine->WorldDestroyed(KillcamWorld);
		}
		KillcamWorld->RemoveFromRoot();

		// mark everything else contained in the world to be deleted
		for (auto LevelIt(KillcamWorld->GetLevelIterator()); LevelIt; ++LevelIt)
		{
			const ULevel* Level = *LevelIt;
			if (Level)
			{
				CastChecked<UWorld>(Level->GetOuter())->MarkObjectsPendingKill();
			}
		}

		SourceWorldContext->OwningGameInstance->GetEngine()->DestroyWorldContext(KillcamWorld);
	}
	KillcamWorld = nullptr;
	KillcamWorldPackage = nullptr;
}

APlayerController* UUTKillcamPlayback::GetKillcamSpectatorController()
{
	if (KillcamWorld != nullptr && KillcamWorld->DemoNetDriver != nullptr)
	{
		return KillcamWorld->DemoNetDriver->SpectatorController;
	}

	return nullptr;
}

void UUTKillcamPlayback::OnKillcamReady(bool bWasSuccessful, FNetworkGUID InKillcamViewTargetGuid)
{
	if (!bWasSuccessful)
	{
		return;
	}

	// If the killcam viewtarget guid is valid, try to follow it
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetOuter());
	if (!InKillcamViewTargetGuid.IsValid() || LocalPlayer == nullptr)
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("Not given killcam target"));
		return;
	}

	if (KillcamWorld == nullptr || KillcamWorld->DemoNetDriver == nullptr)
	{
		// Cancel killcam
		KillcamStop();
		return;
	}

	AUTDemoRecSpectator* SpecController = Cast<AUTDemoRecSpectator>(GetKillcamSpectatorController());
	if (SpecController == nullptr)
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("Couldn't find spectator controller"));
		KillcamStop();
		return;
	}

	SpecController->QueuedPlayerStateToView = nullptr;
	SpecController->QueuedViewTargetGuid.Reset();
	SpecController->QueuedViewTargetNetId = FUniqueNetIdRepl();
	SpecController->bAutoCam = false;

	AActor* KillcamActor = KillcamWorld->DemoNetDriver->GetActorForGUID(InKillcamViewTargetGuid);
	if (KillcamActor == nullptr)
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("Couldn't find killcam actor for NetGUID %d"), InKillcamViewTargetGuid.Value);
		SpecController->SetQueuedViewTargetGuid(InKillcamViewTargetGuid.Value);
	}
	else
	{

		// Killcam started and we found the actor we want to follow!

	//	SpecController->SetSpectatorCameraType(ESpectatorCameraType::Chase);
		APawn* KillcamPawn = Cast<APawn>(KillcamActor);
		if (KillcamPawn && KillcamPawn->PlayerState)
		{
			UE_LOG(LogUTKillcam, Log, TEXT("Killcam viewing %s"), *KillcamPawn->PlayerState->PlayerName);
		}
		SpecController->ViewPawn(KillcamPawn);
		// Weapon isn't replicated so first person view doesn't have a class to spawn for first person visuals
		SpecController->bSpectateBehindView = true;
		SpecController->BehindView(SpecController->bSpectateBehindView);
		// Just in case we ever get moved off this view target guid
		SpecController->SetQueuedViewTargetGuid(InKillcamViewTargetGuid.Value);
		/*
		UUTSpectatorCamComp_Chase* const SpectatorCameraComponent = KillcamActor->FindComponentByClass<UUTSpectatorCamComp_Chase>();
		if (SpectatorCameraComponent != nullptr)
		{
			SpectatorCameraComponent->SetAutoFollow(true);
		}*/
	}
}

void UUTKillcamPlayback::OnCoolMomentCamReady(bool bWasSuccessful, FUniqueNetIdRepl InCoolMomentViewTargetNetId)
{
	if (!bWasSuccessful)
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("OnCoolMomentCamReady failed"));
		return;
	}

	if (KillcamWorld == nullptr || KillcamWorld->DemoNetDriver == nullptr || KillcamWorld->GameState == nullptr)
	{
		// Cancel killcam
		KillcamStop();
		return;
	}

	AUTDemoRecSpectator* SpecController = Cast<AUTDemoRecSpectator>(GetKillcamSpectatorController());
	if (SpecController == nullptr)
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("Couldn't find spectator controller"));
		KillcamStop();
		return;
	}
	
	APlayerState* PS = nullptr;
	for (int32 i = 0; i < KillcamWorld->GameState->PlayerArray.Num(); i++)
	{
		if (KillcamWorld->GameState->PlayerArray[i]->UniqueId == InCoolMomentViewTargetNetId)
		{
			PS = KillcamWorld->GameState->PlayerArray[i];
		}
	}

	SpecController->bAutoCam = false;

	if (PS == nullptr)
	{
		UE_LOG(LogUTKillcam, Warning, TEXT("Couldn't find player to view"));
		SpecController->SetQueuedViewTargetNetId(InCoolMomentViewTargetNetId);
	}
	else
	{
		SpecController->ViewPlayerState(PS);
		// Weapon isn't replicated so first person view doesn't have a class to spawn for first person visuals
		SpecController->bSpectateBehindView = true;
		SpecController->BehindView(SpecController->bSpectateBehindView);
	}
}

void UUTKillcamPlayback::ShowKillcamToUser()
{
	if (KillcamWorld == nullptr)
	{
		return;
	}

	UGameInstance* GameInstance = KillcamWorld->GetGameInstance();
	if (GameInstance != nullptr)
	{
		UUTGameViewportClient* ViewportClient = Cast<UUTGameViewportClient>(GameInstance->GetGameViewportClient());
		if (ViewportClient != nullptr)
		{
			ViewportClient->SetActiveWorldOverride(KillcamWorld);
		}
	}
}

void UUTKillcamPlayback::HideKillcamFromUser()
{
	if (KillcamWorld == nullptr)
	{
		return;
	}

	// Clean up actors that have looping sounds
	for (TActorIterator<AUTProjectile> It(KillcamWorld); It; ++It)
	{
		It->Destroy();
	}

	// Clean up actors that have looping sounds
	for (TActorIterator<AUTRemoteRedeemer> It(KillcamWorld); It; ++It)
	{
		It->Destroy();
	}

	for (TActorIterator<AUTCharacter> It(KillcamWorld); It; ++It)
	{
		It->SetAmbientSound(NULL);
		It->SetLocalAmbientSound(NULL);
	}

	for (FActorIterator It(KillcamWorld); It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(UUTResetInterface::StaticClass()))
		{
			IUTResetInterface::Execute_Reset(*It);
		}
	}

	UGameInstance* GameInstance = KillcamWorld->GetGameInstance();
	if (GameInstance != nullptr)
	{
		UUTGameViewportClient* ViewportClient = Cast<UUTGameViewportClient>(GameInstance->GetGameViewportClient());
		if (ViewportClient != nullptr)
		{
			ViewportClient->ClearActiveWorldOverride();
		}
	}
}

static void HandleKillcamPlayCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	FString Temp;
	const TCHAR* ErrorString = nullptr;

	if ( Args.Num() < 1 )
	{
		ErrorString = TEXT( "You must specify a filename" );
	}
	else if ( InWorld == nullptr )
	{
		ErrorString = TEXT( "InWorld is null" );
	}
	else if ( InWorld->GetGameInstance() == nullptr )
	{
		ErrorString = TEXT( "InWorld->GetGameInstance() is null" );
	}

	if ( ErrorString != nullptr )
	{
		UE_LOG(LogUTKillcam, Log, TEXT("%s"), *ErrorString );

		if ( InWorld->GetGameInstance() != nullptr )
		{
			InWorld->GetGameInstance()->HandleDemoPlaybackFailure( EDemoPlayFailure::Generic, FString( ErrorString ) );
		}
	}
	else
	{
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(InWorld->GetFirstLocalPlayerFromController());
		if (LocalPlayer != nullptr && LocalPlayer->GetKillcamPlaybackManager() != nullptr)
		{
			LocalPlayer->GetKillcamPlaybackManager()->PlayKillcamReplay( Temp );
		}
	}
}

void UUTKillcamPlayback::HandleKillcamToggleCommand(UWorld* InWorld)
{
	const TCHAR* ErrorString = nullptr;

	// Toggles the killcam flag in the game viewport client
	if ( InWorld == nullptr )
	{
		ErrorString = TEXT( "InWorld is null" );
		return;
	}

	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(InWorld->GetFirstLocalPlayerFromController());
	if (LocalPlayer != nullptr)
	{
		UUTKillcamPlayback* LocalKillcamPlayback = LocalPlayer->GetKillcamPlaybackManager();
		UUTGameViewportClient* UTVC = Cast<UUTGameViewportClient>(InWorld->GetGameViewport());
		if (LocalKillcamPlayback != nullptr && UTVC != nullptr)
		{
			if (UTVC->GetActiveWorldOverride() == nullptr)
			{
				LocalKillcamPlayback->ShowKillcamToUser();
			}
			else
			{
				LocalKillcamPlayback->HideKillcamFromUser();
			}
		}
	}
}

static void HandleInstantReplayCommand(const TArray<FString>& Args, UWorld* InWorld)
{
	if (Args.Num() != 1)
	{
		return;
	}

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(InWorld->GetFirstPlayerController());
	if (UTPC)
	{
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(InWorld->GetFirstLocalPlayerFromController());
		if (LocalPlayer != nullptr)
		{
			UUTKillcamPlayback* LocalKillcamPlayback = LocalPlayer->GetKillcamPlaybackManager();
			UUTGameViewportClient* UTVC = Cast<UUTGameViewportClient>(InWorld->GetGameViewport());
			if (LocalKillcamPlayback != nullptr && UTVC != nullptr)
			{
				FNetworkGUID FocusPawnGuid = InWorld->DemoNetDriver->GetGUIDForActor(UTPC->GetPawn());
				FTimerHandle Timer1;
				FTimerHandle Timer2;
				InWorld->GetTimerManager().SetTimer(
					Timer1,
					FTimerDelegate::CreateUObject(UTPC, &AUTPlayerController::OnKillcamStart, FocusPawnGuid, FCString::Atof(*Args[0])),
					0.5f,
					false);
				InWorld->GetTimerManager().SetTimer(
					Timer2,
					FTimerDelegate::CreateUObject(UTPC, &AUTPlayerController::ClientStopKillcam),
					FCString::Atof(*Args[0]) + 0.5f + 0.5f,
					false);
			}
		}
	}
}

FAutoConsoleCommandWithWorldAndArgs KillcamPlayCommand(
	TEXT("UT.KillcamPlay"),
	TEXT("Plays the replay with the given name in the killcam playback world."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(HandleKillcamPlayCommand));

FAutoConsoleCommandWithWorld KillcamToggleCommand(
	TEXT("UT.KillcamToggle"),
	TEXT("Switches the active world for the viewport to the killcam world"),
	FConsoleCommandWithWorldDelegate::CreateStatic(UUTKillcamPlayback::HandleKillcamToggleCommand));

FAutoConsoleCommandWithWorldAndArgs InstantReplayCommand(
	TEXT("UT.ShowInstantReplay"),
	TEXT("Shows instant replay from last X seconds of current player"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(HandleInstantReplayCommand));