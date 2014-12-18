// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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
	FText StatusText = FText::GetEmpty();
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		StatusText = GS->GetGameStatusText();
	}

	if (!StatusText.IsEmpty())
	{
		GameStateBackground.bHidden = false;
		GameStateText.bHidden = false;
		GameStateText.Text = StatusText;
	}
	else
	{
		GameStateBackground.bHidden = true;
		GameStateText.bHidden = true;
		GameStateText.Text = StatusText;
	}

	Super::Draw_Implementation(DeltaTime);
}

FText UUTHUDWidget_GameClock::GetPlayerScoreText_Implementation()
{
	return FText::AsNumber(UTHUDOwner->CurrentPlayerScore);
}

FText UUTHUDWidget_GameClock::GetClockText_Implementation()
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	float RemainingTime = 0.0;
	if (GS)
	{
		RemainingTime = (GS->TimeLimit) ? GS->RemainingTime : GS->ElapsedTime;
	}
	
	FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(),FText::GetEmpty(),RemainingTime,false);

	if (GS && GS->RemainingTime >= 3600)	// More than an hour
	{
		ClockText.TextScale = AltClockScale;
	}
	else
	{
		ClockText.TextScale = GetClass()->GetDefaultObject<UUTHUDWidget_GameClock>()->ClockText.TextScale;
	}

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

