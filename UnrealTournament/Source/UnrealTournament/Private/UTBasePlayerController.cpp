// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChatMessage.h"
#include "Engine/Console.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTWeap_RocketLauncher.h"
#include "UTGameEngine.h"
#include "UnrealNetwork.h"
#include "UTGameViewportClient.h"
#include "UTRconAdminInfo.h"
#include "UTLocalPlayer.h"
#include "UTProfileSettings.h"
#include "UTGameInstance.h"
#include "UTParty.h"

AUTBasePlayerController::AUTBasePlayerController(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ChatOverflowTime = 0.0f;
	bOverflowed = false;
	SpamText = NSLOCTEXT("AUTBasePlayerController", "SpamMessage", "You must wait a few seconds before sending another message.");
}

void AUTBasePlayerController::Destroyed()
{
	ClientCloseAllUI(true); // need to leave dialogs open for connection failures that might already be up

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

	// read profile on every level change so we can detect updates
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
#if WITH_PROFILE
	if (LP != NULL && LP->IsLoggedIn())
	{
		FClientUrlContext QueryContext = FClientUrlContext::Default; // IMPORTANT to make a copy!
		LP->GetMcpProfileManager()->GetMcpProfileAs<UUtMcpProfile>(EUtMcpProfile::Profile)->ForceQueryProfile(QueryContext);
	}
#endif

	// Let the viewport client know we have connected to a server.
	if (GetWorld()->GetNetMode() == ENetMode::NM_Client)
	{
		if (LP && LP->ViewportClient)
		{
			UUTGameViewportClient* VC = Cast<UUTGameViewportClient>(LP->ViewportClient);
			if (VC)
			{
				VC->ClientConnectedToServer();
			}
		}
	}
}

void AUTBasePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction("ShowMenu", IE_Released, this, &AUTBasePlayerController::execShowMenu);
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

void AUTBasePlayerController::execShowMenu()
{
	ShowMenu(TEXT(""));
}

void AUTBasePlayerController::ShowMenu(const FString& Parameters)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->ShowMenu(Parameters);
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
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != nullptr && LP->ViewportClient->ViewportConsole != nullptr)
	{
		LP->ShowQuickChat(ChatDestinations::Local);
	}
}

void AUTBasePlayerController::TeamTalk()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != nullptr && LP->ViewportClient->ViewportConsole != nullptr)
	{
		LP->ShowQuickChat(ChatDestinations::Team);
	}
}

bool AUTBasePlayerController::AllowTextMessage(const FString& Msg)
{
	const float TIME_PER_MSG = 2.0f;
	const float MAX_OVERFLOW = 4.0f;

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
	if (AllowTextMessage(Message) && PlayerState != nullptr)
	{
		// Look to see if this message is a direct message to a given player.

		if (Message.Left(1) == TEXT("@"))
		{
			// Remove the @
			FString TrimmedMessage = Message.Right(Message.Len()-1);

			// Talking to someone directly.
	
			int32 Pos = -1;
			if (TrimmedMessage.FindChar(TEXT(' '), Pos) && Pos > 0)
			{
				FString User = TrimmedMessage.Left(Pos);
				FString FinalMessage = TrimmedMessage.Right(Message.Len() - Pos - 1);		
				DirectSay(User, FinalMessage);
			}
			return;
		}

		bool bSpectatorMsg = PlayerState->bOnlySpectator;

		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTBasePlayerController* UTPC = Cast<AUTBasePlayerController>(*Iterator);
			if (UTPC != nullptr)
			{
				if (!bTeamMessage || UTPC->GetTeamNum() == GetTeamNum())
				{
					// Dont send spectator chat to players
					if (UTPC->PlayerState != nullptr && (!bSpectatorMsg || UTPC->PlayerState->bOnlySpectator))
					{
						UTPC->ClientSay(UTPlayerState, Message, (bTeamMessage ? ChatDestinations::Team : ChatDestinations::Local));
					}
				}
			}
		}
	}
}

void AUTBasePlayerController::DirectSay(const FString& User, const FString& Message)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTBasePlayerController* UTPC = Cast<AUTBasePlayerController>(*Iterator);
		if (UTPC != nullptr && UTPC->PlayerState->PlayerName.Equals(User, ESearchCase::IgnoreCase))
		{
			UTPC->ClientSay(UTPlayerState, Message, ChatDestinations::Whisper);
		}
	}

	// Make sure I see that I sent it..

	ClientSay(UTPlayerState, Message, ChatDestinations::Whisper);

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
	return (UTPlayerState != NULL && UTPlayerState->Team != NULL) ? UTPlayerState->Team->TeamIndex : 255;
}

void AUTBasePlayerController::ClientReturnToLobby_Implementation()
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->HubGuid.IsValid())
	{
		ConnectToServerViaGUID(GameState->HubGuid.ToString(), false);
	}
	else
	{
		ConsoleCommand("Disconnect");
	}
}

void AUTBasePlayerController::CancelConnectViaGUID()
{
		GUIDJoinWantsToSpectate = false;
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

void AUTBasePlayerController::ConnectToServerViaGUID(FString ServerGUID, int32 DesiredTeam, bool bSpectate)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem && !GUIDSessionSearchSettings.IsValid()) 
	{

		UE_LOG(UT,Verbose,TEXT("Attempting to Connect to Server Via GUID: %s"), *ServerGUID);

		IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

		GUIDJoinWantsToSpectate = bSpectate;
		GUIDJoin_CurrentGUID = ServerGUID;
		GUIDJoinAttemptCount = 0;
		GUIDSessionSearchSettings.Reset();
		GUIDJoinDesiredTeam = DesiredTeam;
		
		// Check to make sure we are not downloading content.  If we are.. stall until it's completed.

		UUTGameViewportClient* ViewportClient = Cast<UUTGameViewportClient>(Cast<ULocalPlayer>(Player)->ViewportClient);
		if (ViewportClient && ViewportClient->IsDownloadInProgress())
		{
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);			
			LP->ShowDownloadDialog(true);
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
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindGUIDSessionCompleteDelegateHandle);
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
	// Clear the delegate
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		IOnlineSessionPtr OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
		if (OnlineSessionInterface.IsValid())
		{
			OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindGUIDSessionCompleteDelegateHandle);
		}
	}

	UE_LOG(UT,Log,TEXT("OnFindSesssionComplete %i"), bWasSuccessful);
	if (bWasSuccessful)
	{
		if (GUIDSessionSearchSettings.IsValid()) 
		{
			if (GUIDSessionSearchSettings->SearchResults.Num() > 0)
			{
				FOnlineSessionSearchResult Result = GUIDSessionSearchSettings->SearchResults[0];
				UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
				if (LP)
				{
					// Clear the Quickmatch wait timer.
					LP->QuickMatchLimitTime = 0.0;
					if (LP->JoinSession(Result, GUIDJoinWantsToSpectate, GUIDJoinDesiredTeam))
					{
						//LP->HideMenu(); // should happen on level change now
					}
				}

				GUIDJoinWantsToSpectate = false;
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

		if (LP->IsPartyLeader())
		{
			UUTGameInstance* GameInstance = Cast<UUTGameInstance>(LP->GetGameInstance());
			if (GameInstance)
			{
				UUTParty* Party = GameInstance->GetParties();
				if (Party)
				{
					Party->RestorePersistentPartyState();
				}
			}
		}
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
		ServerReceiveStars(LP->GetTotalChallengeStars());
	}
}

// FIXMESTEVE shouldn't receive this from client
bool AUTBasePlayerController::ServerReceiveStars_Validate(int32 TotalStars) { return true; }
void AUTBasePlayerController::ServerReceiveStars_Implementation(int32 TotalStars)
{
	if (UTPlayerState)
	{
		UTPlayerState->TotalChallengeStars = TotalStars;
	}
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
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GameMode)
	{
		GameMode->RconAuth(this, Password);
	}
}

void AUTBasePlayerController::RconNormal()
{
	ServerRconNormal();
}

bool AUTBasePlayerController::ServerRconNormal_Validate() {return true;}
void AUTBasePlayerController::ServerRconNormal_Implementation()
{
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GameMode)
	{
		GameMode->RconNormal(this);
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

	if (Command.ToLower() == TEXT("adminmenu"))
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		AUTRconAdminInfo* AdminInfo = GetWorld()->SpawnActor<AUTRconAdminInfo>(AUTRconAdminInfo::StaticClass(), Params);
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

	AUTBaseGameMode* GM = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GM)
	{
		GM->RconKick(NameOrUIDStr, bBan, Reason);
	}
}

void AUTBasePlayerController::RconMessage(const FString& DestinationId, const FString &Message)
{
	ServerRconMessage(DestinationId, Message);
}

bool AUTBasePlayerController::ServerRconMessage_Validate(const FString& DestinationId, const FString &Message) { return true; }
void AUTBasePlayerController::ServerRconMessage_Implementation(const FString& DestinationId, const FString &Message)
{
	if (UTPlayerState && UTPlayerState->bIsRconAdmin)
	{
		AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
		if (GameMode)
		{
			GameMode->SendRconMessage(DestinationId, Message);
		}
	}
}


void AUTBasePlayerController::HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString)
{
}

void AUTBasePlayerController::ClientCloseAllUI_Implementation(bool bExceptDialogs)
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->CloseAllUI(bExceptDialogs);
	}
}

#if !UE_SERVER
void AUTBasePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateInputMode();

	if (bRequestShowMenu)
	{
		bRequestShowMenu = false;
		ShowMenu(TEXT(""));
	}
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
				Super::SetInputMode(FInputModeGameOnly());
				break;
			case EInputMode::EIM_GameAndUI:
				Super::SetInputMode(FInputModeGameAndUI().SetLockMouseToViewport(true).SetWidgetToFocus(LocalPlayer->ViewportClient->GetGameViewportWidget()));
				break;
			case EInputMode::EIM_UIOnly:
				Super::SetInputMode(FInputModeUIOnly().SetLockMouseToViewport(true).SetWidgetToFocus(LocalPlayer->ViewportClient->GetGameViewportWidget()));
				break;
			}
		}

		bShowMouseCursor = (InputMode == EInputMode::EIM_GameAndUI || InputMode == EInputMode::EIM_UIOnly);
	
	}
}
#endif

bool AUTBasePlayerController::ServerSetAvatar_Validate(FName NewAvatar) { return true; }
void AUTBasePlayerController::ServerSetAvatar_Implementation(FName NewAvatar)
{
	if (UTPlayerState)
	{
		UTPlayerState->Avatar = NewAvatar;
	}

}

void AUTBasePlayerController::SendStatsIDToServer()
{
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
		if (OnlineSub != nullptr)
		{
			IOnlineIdentityPtr OnlineIdentityInterface = OnlineSub->GetIdentityInterface();
			if (OnlineIdentityInterface.IsValid())
			{
				if (OnlineIdentityInterface->GetLoginStatus(UTLocalPlayer->GetControllerId()))
				{
					TSharedPtr<const FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(UTLocalPlayer->GetControllerId());
					if (UserId.IsValid())
					{
						ServerReceiveStatsID(UserId->ToString());
					}
					/*
					#if WITH_PROFILE
					if (GetNetMode() != NM_DedicatedServer)
					{
					InitializeMcpProfile();
					}
					#endif
					*/
				}
				else
				{
					//OnLoginStatusChangedDelegate = OnlineIdentityInterface->AddOnLoginStatusChangedDelegate_Handle(LP->GetControllerId(), FOnLoginStatusChangedDelegate::CreateUObject(this, &AUTPlayerController::OnLoginStatusChanged));
				}
			}
		}
	}
}
void AUTBasePlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		ServerSetAvatar(UTLocalPlayer->GetAvatar());
	}

	SendStatsIDToServer();
}

bool AUTBasePlayerController::ServerReceiveStatsID_Validate(const FString& NewStatsID)
{
	return true;
}
/** Store an id for stats tracking.  Right now we are using the machine ID for this PC until we have have a proper ID available.  */
void AUTBasePlayerController::ServerReceiveStatsID_Implementation(const FString& NewStatsID)
{
	if (UTPlayerState != NULL && !GetWorld()->IsPlayInEditor()) // && GetWorld()->GetNetMode() != NM_Standalone)
	{
		UTPlayerState->StatsID = NewStatsID;
		UTPlayerState->ReadStatsFromCloud();
		UTPlayerState->ReadMMRFromBackend();
	}
}

void AUTBasePlayerController::ShowAdminDialog(AUTRconAdminInfo* AdminInfo)
{
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		UTLocalPlayer->ShowAdminDialog(AdminInfo);
	}
}

void AUTBasePlayerController::ShowAdminMessage(const FString& Message)
{
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		UTLocalPlayer->ShowAdminMessage(Message);
	}
}

void AUTBasePlayerController::UTLogOut()
{
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		UTLocalPlayer->Logout();
	}
}

void AUTBasePlayerController::FriendSay(FString Message)
{
	TArray<FUniqueNetIdRepl> DesiredIds;
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (GameState && LocalPlayer)
	{
		for (int32 i = 0; i <  GameState->PlayerArray.Num(); i++)
		{
			if (LocalPlayer->IsAFriend(GameState->PlayerArray[i]->UniqueId))
			{
				DesiredIds.Add(GameState->PlayerArray[i]->UniqueId);
			}
		}
	}

	if (DesiredIds.Num() > 0)
	{
		ServerFriendSay(Message, DesiredIds);	
	}
}

bool AUTBasePlayerController::ServerFriendSay_Validate(const FString& Message, const TArray<FUniqueNetIdRepl>& FriendIds) { return true; }
void AUTBasePlayerController::ServerFriendSay_Implementation(const FString& Message, const TArray<FUniqueNetIdRepl>& FriendIds)
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTBasePlayerController* UTPC = Cast<AUTBasePlayerController>(*Iterator);
			if (UTPC != nullptr && UTPC->PlayerState != nullptr)
			{
				if (FriendIds.Find(UTPC->PlayerState->UniqueId) != INDEX_NONE)
				{
					UTPC->ClientSay(UTPlayerState, Message, ChatDestinations::Friends);
				}
			}
		}
	}

	// Tell yourself
	ClientSay(UTPlayerState, Message, ChatDestinations::Friends);
}

void AUTBasePlayerController::LobbySay(FString Message)
{
	// clamp message length; aside from troll prevention this is needed for networking reasons
	Message = Message.Left(128);
	if (AllowTextMessage(Message))
	{
		ServerLobbySay(Message);
	}
	else
	{
		//Display spam message to the player
		ClientSay_Implementation(nullptr, SpamText.ToString(), ChatDestinations::System);
	}
}

bool AUTBasePlayerController::ServerLobbySay_Validate(const FString& Message) { return true; }
void AUTBasePlayerController::ServerLobbySay_Implementation(const FString& Message)
{
	AUTGameMode * GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode && GameMode->IsGameInstanceServer()  && AllowTextMessage(Message) && PlayerState != nullptr)
	{
		GameMode->SendLobbyMessage(Message, Cast<AUTPlayerState>(PlayerState));
	}
}

UUTProfileSettings* AUTBasePlayerController::GetProfileSettings()
{
	if (Cast<UUTLocalPlayer>(Player) != nullptr) return Cast<UUTLocalPlayer>(Player)->GetProfileSettings();
	return nullptr;
}
