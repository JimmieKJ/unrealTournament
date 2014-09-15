// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameViewportClient.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWDialog.h"
#include "Slate/SUWInputBox.h"


UUTGameViewportClient::UUTGameViewportClient(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UUTGameViewportClient::PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	Super::PeekNetworkFailureMessages(World, NetDriver, FailureType, ErrorString);
#if !UE_SERVER
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.

	FText NetworkErrorMessage;

	if (FailureType == ENetworkFailure::PendingConnectionFailure && ErrorString == "NEEDPASS")
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
