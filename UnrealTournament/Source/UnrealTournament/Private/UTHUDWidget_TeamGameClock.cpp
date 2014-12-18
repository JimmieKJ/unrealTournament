// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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

FText UUTHUDWidget_TeamGameClock::GetRedScoreText_Implementation()
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS && GS->bTeamGame && GS->Teams.Num() > 0 && GS->Teams[0]) return FText::AsNumber(GS->Teams[0]->Score);
	return FText::AsNumber(0);
}

FText UUTHUDWidget_TeamGameClock::GetBlueScoreText_Implementation()
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS && GS->bTeamGame && GS->Teams.Num() > 1 && GS->Teams[1]) return FText::AsNumber(GS->Teams[1]->Score);
	return FText::AsNumber(0);
}


FText UUTHUDWidget_TeamGameClock::GetClockText_Implementation()
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
		ClockText.TextScale = GetClass()->GetDefaultObject<UUTHUDWidget_TeamGameClock>()->ClockText.TextScale;
	}

	return ClockString;
}
