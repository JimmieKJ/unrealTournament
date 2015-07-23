// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChatMessage.h"
#include "Engine/Console.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTWeap_RocketLauncher.h"
#include "UTGameEngine.h"
#include "UnrealNetwork.h"
#include "UTGameViewportClient.h"

AUTBasePlayerController::AUTBasePlayerController(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ChatOverflowTime = 0.0f;
	bOverflowed = false;
	SpamText = NSLOCTEXT("AUTBasePlayerController", "SpamMessage", "You must wait a few seconds before sending another message.");
}

void AUTBasePlayerController::Destroyed()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (MyHUD)
	{
		GetWorldTimerManager().ClearAllTimersForObject(MyHUD);
	}
	if (PlayerCameraManager != NULL)
	{
		GetWorldTimerManager().ClearAllTimersForObject(PlayerCameraManager);
	}
	if (PlayerInput != NULL)
	{
		GetWorldTimerManager().ClearAllTimersForObject(PlayerInput);
	}
	Super::Destroyed();
}

void AUTBasePlayerController::InitInputSystem()
{
	Super::InitInputSystem();

	// read profile items on every level change so we can detect updates
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL && LP->IsLoggedIn())
	{
		LP->ReadProfileItems();
	}
}

void AUTBasePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction("ShowMenu", IE_Released, this, &AUTBasePlayerController::ShowMenu);
}

void AUTBasePlayerController::SetName(const FString& S)
{
	if (!S.IsEmpty())
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
		if (LP != NULL)
		{
			LP->SetNickname(S);
			LP->SaveProfileSettings();
		}
	}
}

void AUTBasePlayerController::ShowMenu()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->ShowMenu();
	}
}

void AUTBasePlayerController::HideMenu()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->HideMenu();
	}
}

#if !UE_SERVER
void AUTBasePlayerController::ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->ShowMessage(MessageTitle, MessageText, Buttons, Callback);
	}	
}
#endif

// LEAVE ME for quick debug commands when we need them.
void AUTBasePlayerController::DebugTest(FString TestCommand)
{
}

void AUTBasePlayerController::ServerDebugTest_Implementation(const FString& TestCommand)
{
}

bool AUTBasePlayerController::ServerDebugTest_Validate(const FString& TestCommand) {return true;}

void AUTBasePlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
	if (UTPlayerState)
	{
		UTPlayerState->ChatDestination = ChatDestinations::Local;
	}
}


void AUTBasePlayerController::Talk()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
	if (LP != nullptr && LP->ViewportClient->ViewportConsole != nullptr)
	{
		LP->ViewportClient->ViewportConsole->StartTyping("Say ");
	}
}

void AUTBasePlayerController::TeamTalk()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
	if (LP != nullptr && LP->ViewportClient->ViewportConsole != nullptr)
	{
		LP->ViewportClient->ViewportConsole->StartTyping("TeamSay ");
	}
}

bool AUTBasePlayerController::AllowTextMessage(const FString& Msg)
{
	static const float TIME_PER_MSG = 2.0f;
	static const float MAX_OVERFLOW = 4.0f;

	if (GetNetMode() == NM_Standalone || (GetNetMode() == NM_ListenServer && Role == ROLE_Authority))
	{
		return true;
	}

	ChatOverflowTime = FMath::Max(ChatOverflowTime, GetWorld()->RealTimeSeconds);

	//When overflowed, wait till the time is back to 0
	if (bOverflowed && ChatOverflowTime > GetWorld()->RealTimeSeconds)
	{
		return false;
	}
	bOverflowed = false;

	//Accumulate time for each message, double for a duplicate message
	ChatOverflowTime += (LastChatMessage == Msg) ? TIME_PER_MSG * 2 : TIME_PER_MSG;
	LastChatMessage = Msg;

	if (ChatOverflowTime - GetWorld()->RealTimeSeconds <= MAX_OVERFLOW)
	{
		return true;
	}

	bOverflowed = true;
	return false;
}

void AUTBasePlayerController::Say(FString Message)
{
	// clamp message length; aside from troll prevention this is needed for networking reasons
	Message = Message.Left(128);
	if (AllowTextMessage(Message))
	{
		ServerSay(Message, false);
	}
	else
	{
		//Display spam message to the player
		ClientSay_Implementation(nullptr, SpamText.ToString(), ChatDestinations::System);
	}
}

void AUTBasePlayerController::TeamSay(FString Message)
{
	// clamp message length; aside from troll prevention this is needed for networking reasons
	Message = Message.Left(128);
	if (AllowTextMessage(Message))
	{
		ServerSay(Message, true);
	}
	else
	{
		//Display spam message to the player
		ClientSay_Implementation(nullptr, SpamText.ToString(), ChatDestinations::System);
	}
}

bool AUTBasePlayerController::ServerSay_Validate(const FString& Message, bool bTeamMessage) { return true; }

void AUTBasePlayerController::ServerSay_Implementation(const FString& Message, bool bTeamMessage)
{
	if (AllowTextMessage(Message))
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTBasePlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
			if (UTPC != nullptr)
			{
				if (!bTeamMessage || UTPC->GetTeamNum() == GetTeamNum())
				{
					UTPC->ClientSay(UTPlayerState, Message, (bTeamMessage ? ChatDestinations::Team : ChatDestinations::Local));
				}
			}
		}
	}
}

void AUTBasePlayerController::ClientSay_Implementation(AUTPlayerState* Speaker, const FString& Message, FName Destination)
{
	FClientReceiveData ClientData;
	ClientData.LocalPC = this;
	ClientData.MessageIndex = Destination == ChatDestinations::Team;
	ClientData.RelatedPlayerState_1 = Speaker;
	ClientData.MessageString = Message;

	UUTChatMessage::StaticClass()->GetDefaultObject<UUTChatMessage>()->ClientReceiveChat(ClientData, Destination);
}

uint8 AUTBasePlayerController::GetTeamNum() const
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
}

void AUTBasePlayerController::ClientReturnToLobby_Implementation()
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->HubGuid.IsValid())
	{
		ConnectToServerViaGUID(GameState->HubGuid.ToString(), false, true);
	}
	else
	{
		ConsoleCommand("Disconnect");
	}
}

void AUTBasePlayerController::CancelConnectViaGUID()
{
		GUIDJoinWantsToSpectate = false;
		GUIDJoinWantsToFindMatch = false;
		GUIDJoin_CurrentGUID = TEXT("");
		GUIDJoinAttemptCount = 0;
		GUIDSessionSearchSettings.Reset();
		
		if (OnDownloadCompleteDelegateHandle.IsValid())
		{
			UUTGameViewportClient* ViewportClient = Cast<UUTGameViewportClient>(Cast<ULocalPlayer>(Player)->ViewportClient);
			if (ViewportClient && ViewportClient->IsDownloadInProgress())
			{
				ViewportClient->RemoveContentDownloadCompleteDelegate(OnDownloadCompleteDelegateHandle);
			}
		}
}

void AUTBasePlayerController::ConnectToServerViaGUID(FString ServerGUID, int32 DesiredTeam, bool bSpectate, bool bFindLastMatch)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem && !GUIDSessionSearchSettings.IsValid()) 
	{

		UE_LOG(UT,Verbose,TEXT("Attempting to Connect to Server Via GUID: %s"), *ServerGUID);

		IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

		GUIDJoinWantsToSpectate = bSpectate;
		GUIDJoinWantsToFindMatch = bFindLastMatch;
		GUIDJoin_CurrentGUID = ServerGUID;
		GUIDJoinAttemptCount = 0;
		GUIDSessionSearchSettings.Reset();
		GUIDJoinDesiredTeam = DesiredTeam;
		
		// Check to make sure we are not downloading content.  If we are.. stall until it's completed.

		UUTGameViewportClient* ViewportClient = Cast<UUTGameViewportClient>(Cast<ULocalPlayer>(Player)->ViewportClient);
		if (ViewportClient && ViewportClient->IsDownloadInProgress())
		{
			OnDownloadCompleteDelegateHandle = ViewportClient->RegisterContentDownloadCompleteDelegate(FContentDownloadComplete::FDelegate::CreateUObject(this, &AUTBasePlayerController::OnDownloadComplete));
		}
		else
		{
			StartGUIDJoin();
		}
	}
}

void AUTBasePlayerController::OnDownloadComplete(class UUTGameViewportClient* ViewportClient, ERedirectStatus::Type RedirectStatus, const FString& PackageName)
{
	if (ViewportClient && !ViewportClient->IsDownloadInProgress())
	{
		StartGUIDJoin();
	}
}

void AUTBasePlayerController::StartGUIDJoin()
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem && !GUIDSessionSearchSettings.IsValid()) 
	{
		IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

		OnCancelGUIDFindSessionCompleteDelegate.BindUObject(this, &AUTBasePlayerController::OnCancelGUIDFindSessionComplete);
		OnCancelGUIDFindSessionCompleteDelegateHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(OnCancelGUIDFindSessionCompleteDelegate);
		OnlineSessionInterface->CancelFindSessions();
	}
}

void AUTBasePlayerController::OnCancelGUIDFindSessionComplete(bool bWasSuccessful)
{

	UE_LOG(UT,Log,TEXT("OnCancelGUIDFindSessionComplete %i"), bWasSuccessful);

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem && !GUIDSessionSearchSettings.IsValid()) 
	{
		IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
		OnlineSessionInterface->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelGUIDFindSessionCompleteDelegateHandle);
		AttemptGUIDJoin();
	}
}


void AUTBasePlayerController::AttemptGUIDJoin()
{

	UE_LOG(UT,Log,TEXT("Attempting a join #%i"), GUIDJoinAttemptCount);

	if (GUIDJoinAttemptCount > 5)
	{
		UE_LOG(UT,Log,TEXT("AttemptedGUIDJoin Timeout at 5 attempts"));
		return;
	}

	GUIDJoinAttemptCount++;

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem && !GUIDSessionSearchSettings.IsValid()) 
	{
		IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
		
		GUIDSessionSearchSettings = MakeShareable(new FUTOnlineGameSearchBase(false));
		GUIDSessionSearchSettings->MaxSearchResults = 1;
		FString GameVer = FString::Printf(TEXT("%i"), FNetworkVersion::GetLocalNetworkVersion());
		GUIDSessionSearchSettings->QuerySettings.Set(SETTING_SERVERVERSION, GameVer, EOnlineComparisonOp::Equals);		// Must equal the game version
		GUIDSessionSearchSettings->QuerySettings.Set(SETTING_SERVERINSTANCEGUID, GUIDJoin_CurrentGUID, EOnlineComparisonOp::Equals);	// The GUID to find

		TSharedRef<FUTOnlineGameSearchBase> SearchSettingsRef = GUIDSessionSearchSettings.ToSharedRef();

		if (OnlineSessionInterface.IsValid())
		{
			OnlineSessionInterface->CancelFindSessions();				
		}

		OnFindGUIDSessionCompleteDelegate.BindUObject(this, &AUTBasePlayerController::OnFindSessionsComplete);
		OnFindGUIDSessionCompleteDelegateHandle = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindGUIDSessionCompleteDelegate);

		OnlineSessionInterface->FindSessions(0, SearchSettingsRef);
	}
}

void AUTBasePlayerController::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(UT,Log,TEXT("OnFindSesssionComplete %i"), bWasSuccessful);
	if (bWasSuccessful)
	{
		// Clear the delegate
		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		if (OnlineSubsystem && GUIDSessionSearchSettings.IsValid()) 
		{
			IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
			OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindGUIDSessionCompleteDelegateHandle);

			if (GUIDSessionSearchSettings->SearchResults.Num() > 0)
			{
				FOnlineSessionSearchResult Result = GUIDSessionSearchSettings->SearchResults[0];
				UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
				if (LP)
				{
					if (LP->JoinSession(Result, GUIDJoinWantsToSpectate, NAME_None, GUIDJoinWantsToFindMatch,GUIDJoinDesiredTeam))
					{
						LP->HideMenu();
					}
				}

				GUIDJoinWantsToSpectate = false;
				GUIDJoinWantsToFindMatch = false;
				GUIDSessionSearchSettings.Reset();
				return;
			}
		}
	}
	
	GUIDSessionSearchSettings.Reset();
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTBasePlayerController::AttemptGUIDJoin, 10.0f, false);
}

void AUTBasePlayerController::ClientReturnedToMenus()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		LP->LeaveSession();	
		LP->UpdatePresence(TEXT("In Menus"), false, false, false, false);
	}
}

void AUTBasePlayerController::ClientSetPresence_Implementation(const FString& NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		LP->UpdatePresence(NewPresenceString, bAllowInvites, bAllowJoinInProgress, bAllowJoinViaPresence, bAllowJoinViaPresenceFriendsOnly);
	}
}


void AUTBasePlayerController::ClientGenericInitialization_Implementation()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		ServerReceiveAverageRank(LP->GetBaseELORank());
	}
}

bool AUTBasePlayerController::ServerReceiveAverageRank_Validate(int32 NewAverageRank) { return true; }
void AUTBasePlayerController::ServerReceiveAverageRank_Implementation(int32 NewAverageRank)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS) PS->AverageRank = NewAverageRank;
}


void AUTBasePlayerController::ClientRequireContentItem_Implementation(const FString& PakFile, const FString& MD5)
{
	bool bContentMatched = false;

	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine)
	{
		if (UTEngine->LocalContentChecksums.Contains(PakFile) && UTEngine->LocalContentChecksums[PakFile] == MD5)
		{
			UE_LOG(UT, Log, TEXT("ClientRequireContentItem %s is my content"), *PakFile);
			bContentMatched = true;
		}

		if (UTEngine->MountedDownloadedContentChecksums.Contains(PakFile))
		{
			if (UTEngine->MountedDownloadedContentChecksums[PakFile] == MD5)
			{
				UE_LOG(UT, Log, TEXT("ClientRequireContentItem %s was already downloaded"), *PakFile);
				bContentMatched = true;
			}
			else
			{
				UE_LOG(UT, Log, TEXT("ClientRequireContentItem %s was already downloaded, but an old version"), *PakFile);
			}
		}

		if (UTEngine->DownloadedContentChecksums.Contains(PakFile))
		{
			UE_LOG(UT, Log, TEXT("ClientRequireContentItem %s was already downloaded, but it is not mounted yet"), *PakFile);
		}

		if (!bContentMatched)
		{
			UTEngine->FilesToDownload.Add(PakFile, MD5);
		}
	}
}

void AUTBasePlayerController::ClientRequireContentItemListBegin_Implementation(const FString& CloudId)
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine)
	{
		UTEngine->ContentDownloadCloudId = CloudId;
		UTEngine->FilesToDownload.Empty();
	}
}

void AUTBasePlayerController::ClientRequireContentItemListComplete_Implementation()
{

}


void AUTBasePlayerController::RconAuth(FString Password)
{
	ServerRconAuth(Password);
}

bool AUTBasePlayerController::ServerRconAuth_Validate(const FString& Password)
{
	return true;
}

void AUTBasePlayerController::ServerRconAuth_Implementation(const FString& Password)
{
	if (UTPlayerState != nullptr && !UTPlayerState->bIsRconAdmin && !GetDefault<UUTGameEngine>()->RconPassword.IsEmpty())
	{
		if (GetDefault<UUTGameEngine>()->RconPassword == Password)
		{
			ClientSay(UTPlayerState, TEXT("Rcon authenticated!"), ChatDestinations::System);
			UTPlayerState->bIsRconAdmin = true;
		}
		else
		{
			ClientSay(UTPlayerState, TEXT("Rcon password incorrect"), ChatDestinations::System);
		}
	}
	else
	{
		ClientSay(UTPlayerState, TEXT("Rcon password unset"), ChatDestinations::System);
	}
}


void AUTBasePlayerController::RconExec(FString Command)
{
	ServerRconExec(Command);
}

bool AUTBasePlayerController::ServerRconExec_Validate(const FString& Command)
{
	return true;
}

void AUTBasePlayerController::ServerRconExec_Implementation(const FString& Command)
{
	if (UTPlayerState == nullptr || !UTPlayerState->bIsRconAdmin)
	{
		ClientSay(UTPlayerState, TEXT("Rcon not authenticated"), ChatDestinations::System);
		return;
	}

	ConsoleCommand(Command);
}

void AUTBasePlayerController::RconKick(FString NameOrUIDStr, bool bBan, FString Reason)
{
	ServerRconKick(NameOrUIDStr, bBan, Reason);
}

bool AUTBasePlayerController::ServerRconKick_Validate(const FString& NameOrUIDStr, bool bBan, const FString& Reason) { return true; }
void AUTBasePlayerController::ServerRconKick_Implementation(const FString& NameOrUIDStr, bool bBan, const FString& Reason)
{
	// Quick out if we haven't been authenticated.
	if (UTPlayerState == nullptr || !UTPlayerState->bIsRconAdmin)
	{
		ClientSay(UTPlayerState, TEXT("Rcon not authenticated"), ChatDestinations::System);
		return;
	}

	AGameState* GS = GetWorld()->GetGameState<AGameState>();
	AGameMode* GM = GetWorld()->GetAuthGameMode<AGameMode>();
	AGameSession* GSession = GM->GameSession;
	if (GS && GM && GSession)
	{
		for (int32 i=0; i < GS->PlayerArray.Num(); i++)
		{
			if ( (GS->PlayerArray[i]->PlayerName.ToLower() == NameOrUIDStr.ToLower()) ||
				 (GS->PlayerArray[i]->UniqueId.ToString() == NameOrUIDStr))
			{
				APlayerController* PC = Cast<APlayerController>(GS->PlayerArray[i]->GetOwner());
				if (PC)
				{
					if (bBan)
					{
						GSession->BanPlayer(PC,FText::FromString(Reason));
					}
					else
					{
						GSession->KickPlayer(PC, FText::FromString(Reason));
					}
				}
			}
		}
	}
}

void AUTBasePlayerController::HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString)
{
}

void AUTBasePlayerController::ClientCloseAllUI_Implementation()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->CloseAllUI();
	}
}

void AUTBasePlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	ClientCloseAllUI();
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

#if !UE_SERVER
void AUTBasePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateInputMode();
}

void AUTBasePlayerController::UpdateInputMode()
{
	EInputMode::Type NewInputMode = EInputMode::EIM_None;

	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		//Menus default to UI
		if (LocalPlayer->AreMenusOpen())
		{
			NewInputMode = EInputMode::EIM_UIOnly;
		}
		else if (GetWorld()->DemoNetDriver != nullptr
			|| LocalPlayer->ViewportClient->ViewportConsole->ConsoleState != NAME_None) //Console has some focus issues with UI Only
		{
			NewInputMode = EInputMode::EIM_GameAndUI;
		}
		else
		{
			//Give blueprints a chance to set the input
			AUTHUD* UTHUD = Cast<AUTHUD>(MyHUD);
			if (UTHUD != nullptr)
			{
				NewInputMode = UTHUD->GetInputMode();
			}
			
			//Default to game only if no other input mode is wanted
			if (NewInputMode == EInputMode::EIM_None)
			{
				NewInputMode = EInputMode::EIM_GameOnly;
			}
		}

		//Apply the new input if it needs to be changed
		if (NewInputMode != InputMode && NewInputMode != EInputMode::EIM_None)
		{
			InputMode = NewInputMode;
			switch (NewInputMode)
			{
			case EInputMode::EIM_GameOnly:
				bShowMouseCursor = false;
				Super::SetInputMode(FInputModeGameOnly());
				break;
			case EInputMode::EIM_GameAndUI:
				bShowMouseCursor = true;
				Super::SetInputMode(FInputModeGameAndUI().SetWidgetToFocus(LocalPlayer->ViewportClient->GetGameViewportWidget()));
				break;
			case EInputMode::EIM_UIOnly:
				bShowMouseCursor = true;
				Super::SetInputMode(FInputModeUIOnly());
				break;
			}
		}
	}
}
#endif