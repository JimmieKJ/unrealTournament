// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTEngineMessage.h"
#include "GameFramework/LocalMessage.h"

UUTEngineMessage::UUTEngineMessage(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	bIsConsoleMessage = true;
	Lifetime = 6.0f;

	FailedPlaceMessage = NSLOCTEXT("UTEngineMessage", "FailedPlaceMessage", "No valid playerstart found.");
	MaxedOutMessage = NSLOCTEXT("UTEngineMessage", "MaxedOutMessage", "Server is full.");
	EnteredMessage = NSLOCTEXT("UTEngineMessage", "EnteredMessage", " has joined the server.");
	LeftMessage = NSLOCTEXT("UTEngineMessage", "LeftMessage", " has left the game.");
	GlobalNameChange = NSLOCTEXT("UTEngineMessage", "GlobalNameChange", " {Player1OldName} changed name to {Player1Name}.");
	SpecEnteredMessage = NSLOCTEXT("UTEngineMessage", "SpecEnteredMessage", " joined as a spectator.");
	NewPlayerMessage = NSLOCTEXT("UTEngineMessage", "NewPlayerMessage", "A new player has joined the server.");
	NewSpecMessage = NSLOCTEXT("UTEngineMessage", "NewSpecMessage", "A new spectator joined the server.");
	ServerNotResponding = NSLOCTEXT("UTGameMessage", "ServerNotResponding", "Server not responding.");
	FontSizeIndex = -1;
}

FText UUTEngineMessage::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 1:
		return (RelatedPlayerState_1 == NULL) ? NewPlayerMessage : EnteredMessage;
	case 2:
		return (RelatedPlayerState_1 == NULL) ? FText::GetEmpty() : GlobalNameChange;
	case 4:
		return (RelatedPlayerState_1 == NULL) ? FText::GetEmpty() : LeftMessage;
	case 7:
		return MaxedOutMessage;
	case 16:
		return (RelatedPlayerState_1 == NULL) ? NewSpecMessage : SpecEnteredMessage;
	case 17:
		return ServerNotResponding;
	}
	return FText::GetEmpty();
}

FString UUTEngineMessage::GetConsoleString(const FClientReceiveData& ClientData, FText LocalMessageText) const
{
	bool bMatchesMessageIndex = (ClientData.MessageIndex == 1) || (ClientData.MessageIndex == 4) || (ClientData.MessageIndex == 16);
	if (ClientData.RelatedPlayerState_1 != nullptr && bMatchesMessageIndex)
	{
		return ClientData.RelatedPlayerState_1->PlayerName + LocalMessageText.ToString();
	}
	return LocalMessageText.ToString();
}


