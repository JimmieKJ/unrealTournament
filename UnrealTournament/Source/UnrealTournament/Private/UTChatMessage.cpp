// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "GameFramework/PlayerState.h"
#include "UTChatMessage.h"
#include "UTLocalPlayer.h"
#include "UTLobbyMatchInfo.h"
#include "UTLobbyGameState.h"

UUTChatMessage::UUTChatMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	bIsConsoleMessage = true;

	Lifetime = 6.0f;
	FontSizeIndex = -1;
}

FLinearColor UUTChatMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::Green;
}

int32 UUTChatMessage::GetDestinationIndex(int32 MessageIndex) const
{
	return MessageIndex;
}

void UUTChatMessage::ClientReceiveChat(const FClientReceiveData& ClientData, FName Destination) const
{
	if (Cast<AUTHUD>(ClientData.LocalPC->MyHUD) != NULL)
	{
		int32 TeamNum = 255;
		FString PlayerName(TEXT("Player"));
		if (ClientData.RelatedPlayerState_1 != nullptr)
		{
			PlayerName = ClientData.RelatedPlayerState_1->PlayerName;
			if (Cast<AUTPlayerState>(ClientData.RelatedPlayerState_1) != nullptr)
			{
				TeamNum = Cast<AUTPlayerState>(ClientData.RelatedPlayerState_1)->GetTeamNum();
			}
		}

		FString DestinationTag;
		int32 MessageIndex = 0;
		if (Destination == ChatDestinations::Global)
		{
			DestinationTag = TEXT("[Global]");
			MessageIndex = 1;
		}
		else if (Destination == ChatDestinations::System) 
		{
			DestinationTag = TEXT("[System]");
			MessageIndex = 2;
		}
		else if (Destination == ChatDestinations::Lobby) 
		{
			DestinationTag = TEXT("[Lobby]");
			MessageIndex = 3;
		}
		else if (Destination == ChatDestinations::Local)
		{
			DestinationTag = TEXT("[Chat]");
			MessageIndex = 4;
		}
		else if (Destination == ChatDestinations::Match)
		{
			DestinationTag = TEXT("[Match]");
			MessageIndex = 5;
		}
		else if (Destination == ChatDestinations::Team)
		{
			DestinationTag = TEXT("[Team]");
			MessageIndex = 6;
		}
		else if (Destination == ChatDestinations::MOTD)
		{
			DestinationTag = TEXT("[MOTD]");
			MessageIndex = 7;
		}
		else if (Destination == ChatDestinations::Whisper)
		{
			DestinationTag = TEXT("[Whisper]");
			MessageIndex = 8;
		}
		else if (Destination == ChatDestinations::Instance)
		{
			AUTLobbyMatchInfo* InstanceInfo = Cast<AUTLobbyMatchInfo>(ClientData.OptionalObject);
			AUTLobbyGameState* LobbyGameState = GetWorld()->GetGameState<AUTLobbyGameState>();
			if (InstanceInfo && LobbyGameState)
			{
				DestinationTag = TEXT("[Instance]");
				MessageIndex = 9;
				// Try to Find the index of this lobby...
				for (int32 i=0; i < LobbyGameState->AvailableMatches.Num();i++)
				{
					if (LobbyGameState->AvailableMatches[i] == InstanceInfo)
					{
						DestinationTag = FString::Printf(TEXT("[Instance %i"), i);
						break;
					}
				}
			}
			else
			{
				// Bad instance -> client chat.. just discard
				return;
			}
		}

		FText LocalMessageText = FText::FromString(FString::Printf(TEXT(": %s"), *ClientData.MessageString));
		Cast<AUTHUD>(ClientData.LocalPC->MyHUD)->ReceiveLocalMessage(GetClass(), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, MessageIndex, LocalMessageText, ClientData.OptionalObject);

		FString ChatMessage;
		if (Destination == ChatDestinations::MOTD || Destination == ChatDestinations::System || Destination == ChatDestinations::Lobby)
		{
			ChatMessage = FString::Printf(TEXT("%s %s"), *DestinationTag, *ClientData.MessageString);
		}
		else
		{
			ChatMessage = FString::Printf(TEXT("%s %s: %s"), *DestinationTag, *PlayerName, *ClientData.MessageString);
		}

		if (IsConsoleMessage(ClientData.MessageIndex) && Cast<ULocalPlayer>(ClientData.LocalPC->Player) != NULL && Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient != NULL)
		{
			Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient->ViewportConsole->OutputText(ChatMessage);
		}

		AUTBasePlayerController* PlayerController = Cast<AUTBasePlayerController>(ClientData.LocalPC);
		if (PlayerController)
		{
			UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(ClientData.LocalPC->Player);
			if (LocalPlayer)
			{
				FLinearColor ChatColor = (ClientData.MessageIndex && PlayerController->UTPlayerState && PlayerController->UTPlayerState->Team) ? PlayerController->UTPlayerState->Team->TeamColor : FLinearColor::White;
				LocalPlayer->SaveChat(Destination, PlayerName, ClientData.MessageString, ChatColor, ClientData.RelatedPlayerState_1 == PlayerController->PlayerState, TeamNum);
			}
		}
	}

}