// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_ConsoleMessages.h"

UUTHUDWidgetMessage_ConsoleMessages::UUTHUDWidgetMessage_ConsoleMessages(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	ManagedMessageArea = FName(TEXT("ConsoleMessage"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.0f, 0.65f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.0f, 0.0f);				

	MessageColor = FLinearColor::White;
}

