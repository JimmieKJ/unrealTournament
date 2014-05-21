// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
}

void UUTHUDWidget_Paperdoll::Draw(float DeltaTime)
{

	if (UTHUDOwner->UTCharacterOwner)
	{
		DrawText( FText::Format(NSLOCTEXT("UTHUD","HEALTH","Health: {0}"), FText::AsNumber(UTHUDOwner->UTCharacterOwner->Health)), 0.0, 0.0f, GEngine->GetLargeFont(), FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);	
	}
}

