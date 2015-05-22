// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_TeamGameClock.h"
#include "UTCTFGameState.h"

UUTHUDWidget_TeamGameClock::UUTHUDWidget_TeamGameClock(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(430.0f,83.0f);
	ScreenPosition=FVector2D(0.5f, 0.0f);
	Origin=FVector2D(0.5f,0.0f);
}


void UUTHUDWidget_TeamGameClock::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	RedScoreText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_TeamGameClock::GetRedScoreText_Implementation);
	BlueScoreText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_TeamGameClock::GetBlueScoreText_Implementation);
	ClockText.GetTextDelegate.BindUObject(this, &UUTHUDWidget_TeamGameClock::GetClockText_Implementation);
}

void UUTHUDWidget_TeamGameClock::Draw_Implementation(float DeltaTime)
{
	FText StatusText = FText::GetEmpty();
	if (UTGameState != NULL)
	{
		StatusText = UTGameState->GetGameStatusText();
	}

	if (!StatusText.IsEmpty())
	{
		GameStateBackground.bHidden = true; // @TODO FIXMESTEVE remove entirely, also clock background
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

FText UUTHUDWidget_TeamGameClock::GetRedScoreText_Implementation()
{
	if (UTGameState && UTGameState->bTeamGame && UTGameState->Teams.Num() > 0 && UTGameState->Teams[0]) return FText::AsNumber(UTGameState->Teams[0]->Score);
	return FText::AsNumber(0);
}

FText UUTHUDWidget_TeamGameClock::GetBlueScoreText_Implementation()
{
	if (UTGameState && UTGameState->bTeamGame && UTGameState->Teams.Num() > 1 && UTGameState->Teams[1]) return FText::AsNumber(UTGameState->Teams[1]->Score);
	return FText::AsNumber(0);
}


FText UUTHUDWidget_TeamGameClock::GetClockText_Implementation()
{
	float RemainingTime = UTGameState ? UTGameState->GetClockTime() : 0.f;
	FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(),FText::GetEmpty(),RemainingTime,false);
	ClockText.TextScale = (RemainingTime >= 3600) ? AltClockScale : GetClass()->GetDefaultObject<UUTHUDWidget_TeamGameClock>()->ClockText.TextScale;
	return ClockString;
}
