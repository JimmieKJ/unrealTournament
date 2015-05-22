// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_GameClock.h"
#include "UTCTFGameState.h"

UUTHUDWidget_GameClock::UUTHUDWidget_GameClock(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
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
}

void UUTHUDWidget_GameClock::Draw_Implementation(float DeltaTime)
{
	GameStateText.Text = (UTGameState != NULL) ? UTGameState->GetGameStatusText() : FText::GetEmpty();
	float SkullX = (UTHUDOwner->CurrentPlayerScore < 10) ? 110.f : 125.f;
	UTHUDOwner->CalcStanding();
	if (UTHUDOwner->CurrentPlayerScore > 99)
	{
		SkullX = 140.f;
	}
	bool bEmptyText = GameStateText.Text.IsEmpty();
	GameStateBackground.bHidden = true; // bEmptyText; FIXMESTEVE remove widget entirely
	GameStateText.bHidden = bEmptyText;
	Skull.Position = FVector2D(SkullX, 10.f); // position 140, 10
	Super::Draw_Implementation(DeltaTime);
}

FText UUTHUDWidget_GameClock::GetPlayerScoreText_Implementation()
{
	return FText::AsNumber(UTHUDOwner->CurrentPlayerScore);
}

FText UUTHUDWidget_GameClock::GetClockText_Implementation()
{
	float RemainingTime = UTGameState ? UTGameState->GetClockTime() : 0.f;
	FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(),FText::GetEmpty(), RemainingTime,false);
	ClockText.TextScale = (RemainingTime >= 3600) ? AltClockScale : GetClass()->GetDefaultObject<UUTHUDWidget_GameClock>()->ClockText.TextScale;
	return ClockString;
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

