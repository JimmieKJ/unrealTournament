// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTChatMessage.h"
#include "Engine/Console.h"

AUTBasePlayerController::AUTBasePlayerController(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
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
		LP->ShowMenu();
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
	ServerDebugTest(TestCommand);
}

void AUTBasePlayerController::ServerDebugTest_Implementation(const FString& TestCommand)
{

}

bool AUTBasePlayerController::ServerDebugTest_Validate(const FString& TestCommand) {return true;}

void AUTBasePlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
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
	ServerSay(Message, false);
}

void AUTBasePlayerController::TeamSay(FString Message)
{
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
