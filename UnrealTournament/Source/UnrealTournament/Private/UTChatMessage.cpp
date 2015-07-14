// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "GameFramework/PlayerState.h"
#include "UTChatMessage.h"
#include "UTLocalPlayer.h"

UUTChatMessage::UUTChatMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	bIsSpecial = false;
	bIsConsoleMessage = true;

	Lifetime = 6.0f;
}

bool UUTChatMessage::UseLargeFont(int32 MessageIndex) const
{
	return false;
}

FLinearColor UUTChatMessage::GetMessageColor(int32 MessageIndex) const
{
	return FLinearColor::Green;
}

void UUTChatMessage::ClientReceiveChat(const FClientReceiveData& ClientData, FName Destination) const
{
	if (Cast<AUTHUD>(ClientData.LocalPC->MyHUD) != NULL)
	{
		FString PlayerName(TEXT("Player"));
		if (ClientData.RelatedPlayerState_1 != nullptr)
		{
			PlayerName = ClientData.RelatedPlayerState_1->PlayerName;
		}

		FString DestinationTag;
		if      (Destination == ChatDestinations::Global) DestinationTag = TEXT("[Global]");
		else if (Destination == ChatDestinations::System) DestinationTag = TEXT("[System]");
		else if (Destination == ChatDestinations::Lobby) DestinationTag = TEXT("[Lobby]");
		else if (Destination == ChatDestinations::Local) DestinationTag = TEXT("[Chat]");
		else if (Destination == ChatDestinations::Match) DestinationTag = TEXT("[Match]");
		else if (Destination == ChatDestinations::Team) DestinationTag = TEXT("[Team]");
		else if (Destination == ChatDestinations::Team) DestinationTag = TEXT("[Team]");
		else if (Destination == ChatDestinations::MOTD) DestinationTag = TEXT("[MOTD]");

		FString ChatMessage;
		if (Destination == ChatDestinations::MOTD || Destination == ChatDestinations::System)
		{
			ChatMessage = FString::Printf(TEXT("[%s] %s"), *DestinationTag, *ClientData.MessageString);
		}
		else
		{
			ChatMessage = FString::Printf(TEXT("[%s] %s: %s"), *DestinationTag, *PlayerName, *ClientData.MessageString);
		}

		FText LocalMessageText = FText::FromString(ChatMessage);
		Cast<AUTHUD>(ClientData.LocalPC->MyHUD)->ReceiveLocalMessage(GetClass(), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.MessageIndex, LocalMessageText, ClientData.OptionalObject);

		if (IsConsoleMessage(ClientData.MessageIndex) && Cast<ULocalPlayer>(ClientData.LocalPC->Player) != NULL && Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient != NULL)
		{
			Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient->ViewportConsole->OutputText(LocalMessageText.ToString());
		}

		AUTBasePlayerController* PlayerController = Cast<AUTBasePlayerController>(ClientData.LocalPC);
		if (PlayerController)
		{
			UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(ClientData.LocalPC->Player);
			if (LocalPlayer)
			{
				FString StoredMessage;
				FLinearColor ChatColor = (ClientData.MessageIndex && PlayerController->UTPlayerState && PlayerController->UTPlayerState->Team) ? PlayerController->UTPlayerState->Team->TeamColor : FLinearColor::White;

				if (Destination == ChatDestinations::MOTD || Destination == ChatDestinations::System)
				{
					StoredMessage = FString::Printf(TEXT("[System] %s"), *ClientData.MessageString);
				}
				else
				{
					StoredMessage = FString::Printf(TEXT("%s: %s"), *PlayerName, *ClientData.MessageString);
				}
				LocalPlayer->SaveChat(Destination, StoredMessage, ChatColor);
			}
		}
	}

}