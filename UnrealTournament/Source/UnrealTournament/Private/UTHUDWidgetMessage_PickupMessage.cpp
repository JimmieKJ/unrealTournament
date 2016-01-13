// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_PickupMessage.h"

UUTHUDWidgetMessage_PickupMessage::UUTHUDWidgetMessage_PickupMessage(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ManagedMessageArea = FName(TEXT("PickupMessage"));
	Position = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.5f, 0.8f);
	Size = FVector2D(0.0f, 0.0f);
	Origin = FVector2D(0.5f, 0.0f);
	FadeTime = 1.0;
}

