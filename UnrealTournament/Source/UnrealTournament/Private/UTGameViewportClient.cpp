// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameViewportClient.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWDialog.h"
#include "Slate/SUWInputBox.h"
#include "Slate/SUWRedirectDialog.h"
#include "Engine/GameInstance.h"

UUTGameViewportClient::UUTGameViewportClient(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTGameViewportClient::PeekTravelFailureMessages(UWorld* World, enum ETravelFailure::Type FailureType, const FString& ErrorString)
{
	Super::PeekTravelFailureMessages(World, FailureType, ErrorString);
#if !UE_SERVER
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.

	if (FailureType == ETravelFailure::PackageMissing && !ErrorString.IsEmpty())
	{
		if (FPaths::GetExtension(ErrorString) == FString(TEXT("pak")))
		{
			LastAttemptedURL = GEngine->PendingNetGameFromWorld(World)->URL;

			FirstPlayer->OpenDialog(SNew(SUWRedirectDialog)
				.OnDialogResult( FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::RedirectResult))
				.DialogTitle(NSLOCTEXT("UTGameViewportClient", "Redirect", "Download"))
				.RedirectToURL(ErrorString)
				.PlayerOwner(FirstPlayer)
				);

			return;
		}
	}

	FText NetworkErrorMessage;

	switch (FailureType)
	{
		case ETravelFailure::PackageMissing: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient", "TravelErrors_PackageMissing", "Package Missing"); break;

		default: NetworkErrorMessage = FText::FromString(ErrorString);
	}

	if (!ReconnectDialog.IsValid())
	{
		ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient", "NetworkErrorDialogTitle", "Network Error"), NetworkErrorMessage, UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
	}

#endif
}

void UUTGameViewportClient::PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	Super::PeekNetworkFailureMessages(World, NetDriver, FailureType, ErrorString);
#if !UE_SERVER

	// Don't care about net drivers that aren't the game net driver, they are probably just beacon net drivers
	if (NetDriver->NetDriverName != FName(TEXT("PendingNetDriver")) && NetDriver->NetDriverName != NAME_GameNetDriver)
	{
		return;
	}

	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.

	FText NetworkErrorMessage;

	if (FailureType == ENetworkFailure::PendingConnectionFailure)
	{

		if (ErrorString == TEXT("NEEDPASS"))
		{

			UE_LOG(UT,Log,TEXT("%s %s"), *NetDriver->LowLevelGetNetworkNumber(), *GetNameSafe(GetWorld()));
			if (NetDriver != NULL && NetDriver->ServerConnection != NULL)
			{
				LastAttemptedURL = NetDriver->ServerConnection->URL;

				FirstPlayer->OpenDialog(SNew(SUWInputBox)
										.OnDialogResult( FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::ConnectPasswordResult))
										.PlayerOwner(FirstPlayer)
										.DialogTitle(NSLOCTEXT("UTGameViewportClient", "PasswordRequireTitle", "Password is Required"))
										.MessageText(NSLOCTEXT("UTGameViewportClient", "PasswordRequiredText", "This server requires a password:"))
										);
			}
		}
		else if (ErrorString == TEXT("NOTLOGGEDIN"))
		{
			// NOTE: It's possible that the player logged in during the connect sequence but after Prelogin was called on the client.  If this is the case, just reconnect.
			if (FirstPlayer->IsLoggedIn())
			{
				FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
				return;
			}
			
			// If we already have a reconnect message, then don't handle this
			if (!ReconnectDialog.IsValid())
			{
				ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","LoginRequiredTitle","Login Required"), 
														   NSLOCTEXT("UTGameViewportClient","LoginRequiredMessage","You need to login to your Epic account before you can play on this server."), UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::LoginFailureDialogResult));
			}
		}
		return;
	}


	switch (FailureType)
	{
		case ENetworkFailure::ConnectionLost		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ConnectionLost","Connection to server Lost!"); break;
		case ENetworkFailure::ConnectionTimeout		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ConnectionTimeout","Connection to server timed out!"); break;
		case ENetworkFailure::OutdatedClient		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ClientOutdated","Your client is outdated.  Please update your version of UT.  Go to Forums.UnrealTournament.com for more information."); break;
		case ENetworkFailure::OutdatedServer		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ServerOutdated","The server you are connecting to is running a different version of UT.  Make sure you have the latest version of UT.  Go to Forums.UnrealTournament.com for more information."); break;
		default:									  NetworkErrorMessage = FText::FromString(ErrorString);
	}

	if (!ReconnectDialog.IsValid())
	{
		ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","NetworkErrorDialogTitle","Network Error"), NetworkErrorMessage, UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
	}
#endif
}

void UUTGameViewportClient::PostRender(UCanvas* Canvas)
{
#if WITH_EDITORONLY_DATA
	if (!GIsEditor)
#endif
	{
		// work around bug where we can end up with no PlayerController during initial connection (or when losing connection) by rendering an overlay to explain to the user what's going on
		TArray<ULocalPlayer*> GamePlayers;
		if (GameInstance != NULL)
		{
			GamePlayers = GameInstance->GetLocalPlayers();
			// remove players that aren't currently set up for proper rendering (no PlayerController or currently using temp proxy while waiting for server)
			for (int32 i = GamePlayers.Num() - 1; i >= 0; i--)
			{
				if (GamePlayers[i]->PlayerController == NULL || GamePlayers[i]->PlayerController->Player == NULL)
				{
					GamePlayers.RemoveAt(i);
				}
			}
		}
		if (GamePlayers.Num() > 0)
		{
			Super::PostRender(Canvas);
		}
		else
		{
			UFont* Font = GetDefault<AUTHUD>()->MediumFont;
			FText Message = NSLOCTEXT("UTHUD", "WaitingForServer", "Waiting for server to respond...");
			float XL, YL;
			Canvas->SetLinearDrawColor(FLinearColor::White);
			Canvas->TextSize(Font, Message.ToString(), XL, YL);
			Canvas->DrawText(Font, Message, (Canvas->ClipX - XL) * 0.5f, (Canvas->ClipY - YL) * 0.5f);
		}
	}
}

void UUTGameViewportClient::LoginFailureDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		if (FirstPlayer->IsLoggedIn())
		{
			// We are already Logged in.. Reconnect;
			FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
		}
		else
		{
			FirstPlayer->LoginOnline(TEXT(""),TEXT(""),false, false);
		}
	}
	else
	{
		FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
	}
}

void UUTGameViewportClient::NetworkFailureDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_RECONNECT)
	{
		UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));
		FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
	}
	ReconnectDialog.Reset();
}

void UUTGameViewportClient::ConnectPasswordResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
#if !UE_SERVER
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUWInputBox> Box = StaticCastSharedPtr<SUWInputBox>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();

			UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
			if (!InputText.IsEmpty() && FirstPlayer != NULL)
			{
				FString ReconnectCommand = FString::Printf(TEXT("open %s:%i?password=%s"), *LastAttemptedURL.Host, LastAttemptedURL.Port, *InputText);
				FirstPlayer->PlayerController->ConsoleCommand(ReconnectCommand);
			}
		}
	}
#endif
}

void UUTGameViewportClient::RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
#if !UE_SERVER
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
		if (FirstPlayer != nullptr)
		{
			FString ReconnectCommand = FString::Printf(TEXT("open %s:%i"), *LastAttemptedURL.Host, LastAttemptedURL.Port);
			FirstPlayer->PlayerController->ConsoleCommand(ReconnectCommand);
		}
	}
#endif
}