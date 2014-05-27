// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_DeathMessages.h"

UUTHUDWidgetMessage_DeathMessages::UUTHUDWidgetMessage_DeathMessages(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	ManagedMessageArea = FName(TEXT("DeathMessage"));
	Position = FVector2D(960.0f, 810.0f);	// Middle of the screen, 3/4 of the way down @ 1290x1080
	Size = FVector2D(1920.0f, 60.0f);		// Full width, about 60px tall
	Origin = FVector2D(0.5f, 0.0f);	// Centered on the position
}

