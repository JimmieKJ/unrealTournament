// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Paperdoll.h"

UUTHUDWidget_Paperdoll::UUTHUDWidget_Paperdoll(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	PaperDollTexture = HudTexture.Object;

	Position=FVector2D(5.0f, -5.0f);
	Size=FVector2D(400.0f,111.0f);
	ScreenPosition=FVector2D(0.0f, 1.0f);
	Origin=FVector2D(0.0f,1.0f);

}

void UUTHUDWidget_Paperdoll::Draw(float DeltaTime)
{
	if (UTHUDOwner->UTCharacterOwner)
	{
		DrawTexture(PaperDollTexture, 0,0, 65.0f, 111.0f, 0.066f, 0.436f, 0.127f, 0.651f);
		DrawText( FText::Format(NSLOCTEXT("UTHUD","HEALTH","Health: {0}"), FText::AsNumber(UTHUDOwner->UTCharacterOwner->Health)), 67.0f, 20.0f, GEngine->GetLargeFont(), FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);	
	}
}

