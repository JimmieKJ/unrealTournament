// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "GameFramework/PlayerState.h"
#include "UTKillerMessage.h"
#include "UTVictimMessage.h"
#include "UTKillIconMessage.h"
#include "UTDeathMessage.h"

UUTDeathMessage::UUTDeathMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("ConsoleMessage"));
	Lifetime = 3.5f;
	FontSizeIndex = -1;
}

FLinearColor UUTDeathMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor(1.f, 0.2f, 0.2f);
}

void UUTDeathMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	APlayerState* PlayerState_1 = Cast<APlayerState>(ClientData.RelatedPlayerState_1);
	APlayerState* PlayerState_2 = Cast<APlayerState>(ClientData.RelatedPlayerState_2);

	// Look to see if we should redirect this generic death message to either a killer message or a victim message.
	APlayerState* LocalPlayerState = ClientData.LocalPC->PlayerState;

	AUTHUD* UTHUD = Cast<AUTHUD>(ClientData.LocalPC->MyHUD);
	if (UTHUD != nullptr && LocalPlayerState != nullptr)
	{
		// Index 2 is kill assist, don't show in kill feed
		if (ClientData.MessageIndex != 2)
		{
			//Add msg to the UUTHUDWidgetMessage_KillIconMessages
			UTHUD->ReceiveLocalMessage(
				UUTKillIconMessage::StaticClass(),
				ClientData.RelatedPlayerState_1,
				ClientData.RelatedPlayerState_2,
				ClientData.MessageIndex,
				FText::FromString(TEXT("Hax")), //need some text to route the msg
				ClientData.OptionalObject);
		}
		//Draw the big white kill text if the player wants
		if (UTHUD->GetDrawCenteredKillMsg())
		{
			if (LocalPlayerState && (LocalPlayerState->bOnlySpectator || (Cast<AUTPlayerState>(LocalPlayerState) && ((AUTPlayerState*)LocalPlayerState)->bOutOfLives)))
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
				LocalPlayerState = PC ? PC->LastSpectatedPlayerState : nullptr;
				if (LocalPlayerState != nullptr && (LocalPlayerState == ClientData.RelatedPlayerState_1 || LocalPlayerState == ClientData.RelatedPlayerState_2))
				{
					//Spectator kill message
					if ((ClientData.RelatedPlayerState_1 != ClientData.RelatedPlayerState_2) && ((ClientData.RelatedPlayerState_1 != NULL) || (ClientData.RelatedPlayerState_2 != LocalPlayerState)))
					{
						UTHUD->ReceiveLocalMessage(
							UUTKillerMessage::StaticClass(),
							ClientData.RelatedPlayerState_1,
							ClientData.RelatedPlayerState_2,
							-99,
							GetDefault<UUTKillerMessage>()->ResolveMessage(-99, ClientData.RelatedPlayerState_1 == LocalPlayerState, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject),
							ClientData.OptionalObject);
					}
				}
			}
			else if (ClientData.RelatedPlayerState_1 == LocalPlayerState)
			{
				// Interdict and send the child message instead.
				UTHUD->ReceiveLocalMessage(
					UUTKillerMessage::StaticClass(),
					ClientData.RelatedPlayerState_1,
					ClientData.RelatedPlayerState_2,
					ClientData.MessageIndex,
					GetDefault<UUTKillerMessage>()->ResolveMessage(ClientData.MessageIndex, ClientData.RelatedPlayerState_1 == LocalPlayerState, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject),
					ClientData.OptionalObject);

				AUTCharacter* P = Cast<AUTCharacter>(ClientData.LocalPC->GetPawn());
				if (P != NULL && P->GetWeapon() != NULL)
				{
					P->GetWeapon()->NotifyKillWhileHolding(Cast<UClass>(ClientData.OptionalObject));
				}
			}
			else if (ClientData.RelatedPlayerState_2 == LocalPlayerState)
			{
				UTHUD->ReceiveLocalMessage(
					UUTVictimMessage::StaticClass(),
					ClientData.RelatedPlayerState_1,
					ClientData.RelatedPlayerState_2,
					ClientData.MessageIndex,
					GetDefault<UUTVictimMessage>()->ResolveMessage(ClientData.MessageIndex, true, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject),
					ClientData.OptionalObject);
			}
		}
		UTHUD->NotifyKill(LocalPlayerState, ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2);
	}

	// Index 2 is kill assist, don't show in kill feed
	if (ClientData.MessageIndex != 2)
	{
		// add the message to the console's output history
		ULocalPlayer* LP = Cast<ULocalPlayer>(ClientData.LocalPC->Player);
		if (LP != NULL && LP->ViewportClient != NULL && LP->ViewportClient->ViewportConsole != NULL)
		{
			LP->ViewportClient->ViewportConsole->OutputText(ResolveMessage(ClientData.MessageIndex, (ClientData.RelatedPlayerState_1 == ClientData.LocalPC->PlayerState), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.OptionalObject).ToString());
		}

		// Also receive the console message side of this if the user wants.
		if (UTHUD && UTHUD->GetDrawChatKillMsg())
		{
			Super::ClientReceive(ClientData);
		}
	}
}

FText UUTDeathMessage::GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	UClass* DamageTypeClass = Cast<UClass>(OptionalObject);
	if (DamageTypeClass != NULL && DamageTypeClass->IsChildOf(UUTDamageType::StaticClass()))
	{
		UUTDamageType* DamageType = DamageTypeClass->GetDefaultObject<UUTDamageType>();			
		if (Switch == 1)	// Suicide
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(RelatedPlayerState_2);
			return (PS != NULL && PS->IsFemale()) ? DamageType->FemaleSuicideMessage : DamageType->MaleSuicideMessage;
		}
		return DamageType->ConsoleDeathMessage;
	}

	return FText::GetEmpty();	
}
