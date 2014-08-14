// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameViewportClient.h"


UUTGameViewportClient::UUTGameViewportClient(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UUTGameViewportClient::PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	Super::PeekNetworkFailureMessages(World, NetDriver, FailureType, ErrorString);

	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.

	FText NetworkErrorMessage;

	switch (FailureType)
	{
		case ENetworkFailure::ConnectionLost		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ConnectionLost","Connection to server Lost!"); break;
		case ENetworkFailure::ConnectionTimeout		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ConnectionTimeout","Connection to server timed out!"); break;
		case ENetworkFailure::OutdatedClient		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ClientOutdated","Your client is outdated.  Please update your version of UT."); break;
		case ENetworkFailure::OutdatedServer		: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ServerOutdated","The server you are connecting to is running an older version of UT."); break;
		default:									  NetworkErrorMessage = FText::FromString(ErrorString);
	}

	if (!ReconnectDialog.IsValid())
	{
		ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","NetworkErrorDialogTitle","Network Error"), NetworkErrorMessage, UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
	}
	
}

void UUTGameViewportClient::NetworkFailureDialogResult(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_RECONNECT)
	{
		UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));
		FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
	}
	ReconnectDialog.Reset();
}
