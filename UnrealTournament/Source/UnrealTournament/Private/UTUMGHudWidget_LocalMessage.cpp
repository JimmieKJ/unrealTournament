// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTUMGHudWidget_LocalMessage.h"

UUTUMGHudWidget_LocalMessage::UUTUMGHudWidget_LocalMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayZOrder = 2.0f;
}

void UUTUMGHudWidget_LocalMessage::InitializeMessage_Implementation(TSubclassOf<class UUTLocalMessage> inMessageClass, int32 inSwitch, APlayerState* inRelatedPlayerState_1, APlayerState* inRelatedPlayerState_2, UObject* inOptionalObject)
{
	MessageClass = inMessageClass;
	Switch = inSwitch;
	RelatedPlayerState_1 = inRelatedPlayerState_1;
	RelatedPlayerState_2 = inRelatedPlayerState_2;
	OptionalObject = inOptionalObject;
}


