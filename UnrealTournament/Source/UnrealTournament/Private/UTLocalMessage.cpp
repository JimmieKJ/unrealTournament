// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/Console.h"
#include "UTLocalMessage.h"
#include "UTHud.h"

UUTLocalMessage::UUTLocalMessage(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	MessageArea = 1;
}

void UUTLocalMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	FText LocalMessageText = GetDefault<UUTLocalMessage>(GetClass())->GetText(ClientData.MessageIndex, (ClientData.RelatedPlayerState_1 == ClientData.LocalPC->PlayerState), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_1, ClientData.OptionalObject);
	if ( !LocalMessageText.IsEmpty() )
	{
		if ( Cast<AUTHUD>(ClientData.LocalPC->MyHUD) != NULL )
		{
			Cast<AUTHUD>(ClientData.LocalPC->MyHUD)->ReceiveLocalMessage(GetClass(), ClientData.RelatedPlayerState_1, ClientData.RelatedPlayerState_2, ClientData.MessageIndex, LocalMessageText, ClientData.OptionalObject );
		}

		if(IsConsoleMessage(ClientData.MessageIndex) && Cast<ULocalPlayer>(ClientData.LocalPC->Player) != NULL && Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient != NULL)
		{
			Cast<ULocalPlayer>(ClientData.LocalPC->Player)->ViewportClient->ViewportConsole->OutputText( LocalMessageText.ToString() );
		}
	}
}

FText UUTLocalMessage::ResolveMessage(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	FFormatNamedArguments Args;
	GetArgs(Args);
	return FText::Format(GetText(Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject), Args);
}

void UUTLocalMessage::GetArgs(FFormatNamedArguments& Args, int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject ) const
{
	Args.Add(TEXT("Player1Name"), RelatedPlayerState_1 != NULL ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty());
	Args.Add(TEXT("Player1Score"), RelatedPlayerState_1 != NULL ? FText::AsNumber(int32(RelatedPlayerState_1->Score)) : FText::GetEmpty());
	Args.Add(TEXT("Player2Name"), RelatedPlayerState_2 != NULL ? FText::FromString(RelatedPlayerState_2->PlayerName) : FText::GetEmpty());
	Args.Add(TEXT("Player2Score"), RelatedPlayerState_2 != NULL ? FText::AsNumber(int32(RelatedPlayerState_2->Score)) : FText::GetEmpty());
	return;
}


FText UUTLocalMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	return FText::GetEmpty();
}

FColor UUTLocalMessage::GetConsoleColor( APlayerState* RelatedPlayerState_1 ) const
{
    return GetDefault<UUTLocalMessage>(GetClass())->DrawColor;
}

FColor UUTLocalMessage::GetColor(
	int32 Switch,
	APlayerState* RelatedPlayerState_1,
	APlayerState* RelatedPlayerState_2,
	UObject* OptionalObject
	) const
{
	return GetDefault<UUTLocalMessage>(GetClass())->DrawColor;
}

int32 UUTLocalMessage::GetFontSize( int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, APlayerState* LocalPlayer ) const
{
    return GetDefault<UUTLocalMessage>(GetClass())->FontSize;
}

float UUTLocalMessage::GetLifeTime(int32 Switch) const
{
    return GetDefault<UUTLocalMessage>(GetClass())->Lifetime;
}

bool UUTLocalMessage::IsConsoleMessage(int32 Switch) const
{
    return GetDefault<UUTLocalMessage>(GetClass())->bIsConsoleMessage;
}

/**
  * @RETURN true if messages are similar enough to trigger "partially unique" check for HUD display
  */
bool UUTLocalMessage::PartiallyDuplicates(int32 Switch1, int32 Switch2, UObject* OptionalObject1, UObject* OptionalObject2 )
{
	return (Switch1 == Switch2);
}

