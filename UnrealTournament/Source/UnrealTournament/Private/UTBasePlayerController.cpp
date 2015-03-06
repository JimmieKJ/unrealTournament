// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChatMessage.h"
#include "Engine/Console.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTWeap_RocketLauncher.h"
#include "UTGameEngine.h"
AUTBasePlayerController::AUTBasePlayerController(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
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

void AUTBasePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction("ShowMenu", IE_Released, this, &AUTBasePlayerController::ShowMenu);
}

void AUTBasePlayerController::ShowMenu()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		FInputModeUIOnly InputMode;
		SetInputMode(InputMode);
		LP->ShowMenu();
	}

}

void AUTBasePlayerController::HideMenu()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
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
	UTPlayerState->ChatDestination = ChatDestinations::Local;

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


void AUTBasePlayerController::Say(FString Message)
{
	// clamp message length; aside from troll prevention this is needed for networking reasons
	Message = Message.Left(128);
	ServerSay(Message, false);
}

void AUTBasePlayerController::TeamSay(FString Message)
{
	// clamp message length; aside from troll prevention this is needed for networking reasons
	Message = Message.Left(128);
	ServerSay(Message, true);
}

bool AUTBasePlayerController::ServerSay_Validate(const FString& Message, bool bTeamMessage) { return true; }

void AUTBasePlayerController::ServerSay_Implementation(const FString& Message, bool bTeamMessage)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTBasePlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
		if (UTPC != nullptr)
		{
			if (!bTeamMessage || UTPC->GetTeamNum() == GetTeamNum())
			{
				UTPC->ClientSay(UTPlayerState, Message, (bTeamMessage ? ChatDestinations::Team : ChatDestinations::Local) );
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
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer != NULL && LocalPlayer->LastLobbyServerGUID != TEXT(""))
	{
		ConnectToServerViaGUID(LocalPlayer->LastLobbyServerGUID, false);
	}
	else
	{
		ConsoleCommand("Disconnect");
	}
}

void AUTBasePlayerController::ConnectToServerViaGUID(FString ServerGUID, bool bSpectate)
{
	GUIDJoinWantsToSpectate = bSpectate;
	GUIDJoin_CurrentGUID = ServerGUID;
	GUIDJoinAttemptCount = 0;
	GUIDSessionSearchSettings.Reset();

	AttemptGUIDJoin();
}

void AUTBasePlayerController::AttemptGUIDJoin()
{

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
		FString GameVer = FString::Printf(TEXT("%i"),GetDefault<UUTGameEngine>()->GameNetworkVersion);
		GUIDSessionSearchSettings->QuerySettings.Set(SETTING_SERVERVERSION, GameVer, EOnlineComparisonOp::Equals);		// Must equal the game version
		GUIDSessionSearchSettings->QuerySettings.Set(SETTING_SERVERINSTANCEGUID, GUIDJoin_CurrentGUID, EOnlineComparisonOp::Equals);	// The GUID to find

		TSharedRef<FUTOnlineGameSearchBase> SearchSettingsRef = GUIDSessionSearchSettings.ToSharedRef();

		// Cancel any existing find session calls.
		OnlineSessionInterface->CancelFindSessions();

		if (!OnFindGUIDSessionCompleteDelegate.IsBound())
		{
			OnFindGUIDSessionCompleteDelegate.BindUObject(this, &AUTBasePlayerController::OnFindSessionsComplete);
		}

		if (OnFindGUIDSessionCompleteDelegateHandle.IsValid())
		{
			OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindGUIDSessionCompleteDelegateHandle);
			OnFindGUIDSessionCompleteDelegateHandle.Reset();
		}

		if (!OnFindGUIDSessionCompleteDelegateHandle.IsValid())
		{		
			OnFindGUIDSessionCompleteDelegateHandle = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindGUIDSessionCompleteDelegate);
		}

		OnlineSessionInterface->FindSessions(0, SearchSettingsRef);
	}
}

void AUTBasePlayerController::OnFindSessionsComplete(bool bWasSuccessful)
{
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
					if (LP->JoinSession(Result, GUIDJoinWantsToSpectate))
					{
						LP->HideMenu();
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
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTBasePlayerController::AttemptGUIDJoin, 5.0f, false);
}

void AUTBasePlayerController::ClientReturnedToMenus()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		LP->LeaveSession();	
		LP->UpdatePresence(TEXT("In Menus"), false, false, false, false, false);
	}
}

void AUTBasePlayerController::ClientSetPresence_Implementation(const FString& NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly, bool bIsInstance)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		LP->UpdatePresence(NewPresenceString, bAllowInvites, bAllowJoinInProgress, bAllowJoinViaPresence, bAllowJoinViaPresenceFriendsOnly, bIsInstance);
	}
}


void AUTBasePlayerController::ClientGenericInitialization_Implementation()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		ServerRecieveAverageRank(LP->GetBaseELORank());
	}
}

bool AUTBasePlayerController::ServerRecieveAverageRank_Validate(int32 NewAverageRank) { return true; }
void AUTBasePlayerController::ServerRecieveAverageRank_Implementation(int32 NewAverageRank)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS) PS->AverageRank = NewAverageRank;
}

