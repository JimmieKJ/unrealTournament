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
		ServerReceiveRank(LP->GetRankDuel(), LP->GetRankCTF(), LP->GetRankTDM(), LP->GetRankDM(), LP->GetRankShowdown(), LP->GetTotalChallengeStars(), FMath::Min(255, LP->DuelEloMatches()), FMath::Min(255, LP->CTFEloMatches()), FMath::Min(255, LP->TDMEloMatches()), FMath::Min(255, LP->DMEloMatches()), FMath::Min(255, LP->ShowdownEloMatches()));
	}
}

// FIXMESTEVE shouldn't receive this from client
bool AUTBasePlayerController::ServerReceiveRank_Validate(int32 NewDuelRank, int32 NewCTFRank, int32 NewTDMRank, int32 NewDMRank, int32 NewShowdownRank, int32 TotalStars, uint8 DuelMatchesPlayed, uint8 CTFMatchesPlayed, uint8 TDMMatchesPlayed, uint8 DMMatchesPlayed, uint8 ShowdownMatchesPlayed) { return true; }
void AUTBasePlayerController::ServerReceiveRank_Implementation(int32 NewDuelRank, int32 NewCTFRank, int32 NewTDMRank, int32 NewDMRank, int32 NewShowdownRank, int32 TotalStars, uint8 DuelMatchesPlayed, uint8 CTFMatchesPlayed, uint8 TDMMatchesPlayed, uint8 DMMatchesPlayed, uint8 ShowdownMatchesPlayed)
{
	if (UTPlayerState)
	{
		UTPlayerState->DuelRank = NewDuelRank;
		UTPlayerState->CTFRank = NewCTFRank;
		UTPlayerState->TDMRank = NewTDMRank;
		UTPlayerState->DMRank = NewDMRank;
		UTPlayerState->ShowdownRank = NewShowdownRank;
		UTPlayerState->TotalChallengeStars = TotalStars;
		UTPlayerState->DuelMatchesPlayed = DuelMatchesPlayed;
		UTPlayerState->CTFMatchesPlayed = CTFMatchesPlayed;
		UTPlayerState->TDMMatchesPlayed = TDMMatchesPlayed;
		UTPlayerState->DMMatchesPlayed = DMMatchesPlayed;
		UTPlayerState->ShowdownMatchesPlayed = ShowdownMatchesPlayed;

		AUTBaseGameMode* BaseGameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
		if (BaseGameMode)
		{
			BaseGameMode->ReceivedRankForPlayer(UTPlayerState);
		}
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

void AUTBasePlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	
	UUTLocalPlayer* UTLocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (UTLocalPlayer)
	{
		ServerSetAvatar(UTLocalPlayer->GetAvatar());
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