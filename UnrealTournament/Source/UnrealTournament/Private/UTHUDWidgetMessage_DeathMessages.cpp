// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidgetMessage.h"
#include "UTHUDWidgetMessage_DeathMessages.h"

UUTHUDWidgetMessage_DeathMessages::UUTHUDWidgetMessage_DeathMessages(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	ManagedMessageArea = FName(TEXT("DeathMessage"));
	Position = FVector2D(0.0f, 0.0f);			
	ScreenPosition = FVector2D(0.5f, 0.65f);
	Size = FVector2D(0.0f, 0.0f);			
	Origin = FVector2D(0.5f, 0.0f);				

	MessageColor = FLinearColor::Red;

	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("Font'/Game/RestrictedAssets/Proto/UI/Fonts/fntRobotoBlack36.fntRobotoBlack36'"));
	MessageFont = Font.Object;
}

