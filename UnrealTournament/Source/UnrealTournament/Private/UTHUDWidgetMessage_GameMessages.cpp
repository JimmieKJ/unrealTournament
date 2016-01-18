// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_GameMessages.h"
#include "UTGameMessage.h"

UUTHUDWidgetMessage_GameMessages::UUTHUDWidgetMessage_GameMessages(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("GameMessages"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.5f, 0.7f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.0f, 0.0f);				
	FadeTime = 0.5f;
}


