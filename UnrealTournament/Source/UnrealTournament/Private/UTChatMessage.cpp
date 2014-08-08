// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "GameFramework/PlayerState.h"
#include "UTChatMessage.h"

UUTChatMessage::UUTChatMessage(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	bIsSpecial = false;
	bIsConsoleMessage = true;

	Lifetime = 6.0f;
}

void UUTChatMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	if (Cast<AUTHUD>(ClientData.LocalPC->MyHUD) != NULL)
	{
		FString PlayerName(TEXT("Player"));
		if (ClientData.RelatedPlayerState_1 != nullptr)
		{
			PlayerName = ClientData.RelatedPlayerState_1->PlayerName;
		}

		FString ChatMessage;
		if (ClientData.MessageIndex)
		{
			ChatMessage = FString::Printf(TEXT("[TEAM] %s: %s"), *PlayerName, *ClientData.MessageString);
		}
		else
		{
			ChatMessage = FString::Printf(TEXT("[CHAT] %s: %s"), *PlayerName, *ClientData.MessageString);
		}

		FText LocalMessageText = FText::FromString(ChatMessage);
		Cast<AUTHUD>(ClientData.LocalPC->MyHUD)->ReceiveLocalMessage(GetClass(), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.MessageIndex, LocalMessageText, ClientData.OptionalObject);

		if (IsConsoleMessage(ClientData.MessageIndex) && Cast<ULocalPlayer>(ClientData.LocalPC->Player) != NULL && Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient != NULL)
		{
			Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient->ViewportConsole->OutputText(LocalMessageText.ToString());
		}
	}
}