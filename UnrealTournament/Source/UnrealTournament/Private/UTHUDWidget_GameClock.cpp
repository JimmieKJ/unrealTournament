// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_GameClock.h"
#include "UTCTFGameState.h"

UUTHUDWidget_GameClock::UUTHUDWidget_GameClock(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture> Tex(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));

	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(430.0f,83.0f);
	ScreenPosition=FVector2D(0.5f, 0.0f);
	Origin=FVector2D(0.5f,0.0f);
}


void UUTHUDWidget_GameClock::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	PlayerScoreText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_GameClock::GetPlayerScoreText_Implementation);
	ClockText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_GameClock::GetClockText_Implementation);
	PlayerRankText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_GameClock::GetPlayerRankText_Implementation);
	PlayerRankThText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_GameClock::GetPlayerRankThText_Implementation);
	NumPlayersText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_GameClock::GetNumPlayersText_Implementation);
	GameStateText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_GameClock::GetGameStateText_Implementation);
}

void UUTHUDWidget_GameClock::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);
}

FText UUTHUDWidget_GameClock::GetPlayerScoreText_Implementation()
{
	return FText::AsNumber(UTHUDOwner->CurrentPlayerScore);
}

FText UUTHUDWidget_GameClock::GetClockText_Implementation()
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	return UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), (GS ? GS->RemainingTime : 0));
}

FText UUTHUDWidget_GameClock::GetPlayerRankText_Implementation()
{
	return FText::AsNumber(UTHUDOwner->CurrentPlayerStanding);
}

FText UUTHUDWidget_GameClock::GetPlayerRankThText_Implementation()
{
	return UTHUDOwner->GetPlaceSuffix(UTHUDOwner->CurrentPlayerStanding);
}

FText UUTHUDWidget_GameClock::GetNumPlayersText_Implementation()
{
	return FText::AsNumber(UTHUDOwner->NumActualPlayers);
}

FText UUTHUDWidget_GameClock::GetGameStateText_Implementation()
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		if (!GS->IsMatchInProgress())
		{
			if (GS->HasMatchEnded())
			{
				return NSLOCTEXT("GameClock", "PostGame", "!! Game Over !!");
			}
			else
			{
				return NSLOCTEXT("GameClock", "PreGame", "!! Pre-Game !!");
			}
		}

	}

	return FText::GetEmpty();
}

