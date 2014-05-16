// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "GameFramework/PlayerState.h"
#include "UTKillerMessage.h"
#include "UTVictimMessage.h"
#include "UTDeathMessage.h"

UUTDeathMessage::UUTDeathMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	MessageArea = 1;
	DrawColor = FColor(255, 0, 0, 255);
	bIsSpecial = false;

	GenericKillMessage = NSLOCTEXT("UTDeathMessage","GenericKillMessage","{Player1Name} killed {Player2Name} with {WeaponName}");
}

void UUTDeathMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	APlayerState* PlayerState_1 = Cast<APlayerState>(ClientData.RelatedPlayerState_1);
	APlayerState* PlayerState_2 = Cast<APlayerState>(ClientData.RelatedPlayerState_2);

	// Look to see if we should redirect this generic death message to either a killer message or a victim message.

	if (ClientData.RelatedPlayerState_1 == ClientData.LocalPC->PlayerState) // We need to add code to allow for spectators to see the death messages of those they are a spectator
	{
		// Interdict and send the child message instead.
		AUTHUD* UTHUD = Cast<AUTHUD>(ClientData.LocalPC->MyHUD);
		if ( UTHUD != NULL )
		{
			UTHUD->ReceiveLocalMessage(
			UUTKillerMessage::StaticClass(),
			ClientData.RelatedPlayerState_1,
			ClientData.RelatedPlayerState_2,
			0,
			GetDefault<UUTKillerMessage>()->GetText(ClientData.MessageIndex, ClientData.RelatedPlayerState_1== ClientData.LocalPC->PlayerState, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject),
			ClientData.OptionalObject );
		}
	}
	else if (ClientData.RelatedPlayerState_2 == ClientData.LocalPC->PlayerState)
	{
		AUTHUD* UTHUD = Cast<AUTHUD>(ClientData.LocalPC->MyHUD);
		if ( UTHUD != NULL )
		{
			UTHUD->ReceiveLocalMessage(
			UUTVictimMessage::StaticClass(),
			ClientData.RelatedPlayerState_1,
			ClientData.RelatedPlayerState_2,
			0,
			GetDefault<UUTVictimMessage>()->GetText(ClientData.MessageIndex, true, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject),
			ClientData.OptionalObject );
		}
	}
	else
	{
		Super::ClientReceive(ClientData);
	}
}

void UUTDeathMessage::GetArgs(FFormatNamedArguments& Args, int32 Switch, bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	Super::GetArgs(Args, Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);

	// Add code here to look at the damage type passed in via OptionalObject and set the (WeaponName) args.  In the future,
	// we'll need to add support for vehicle damage types as well.
}

FText UUTDeathMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	return GenericKillMessage;
}
