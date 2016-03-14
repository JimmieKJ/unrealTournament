// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_TeamLives.h"
#include "UTCTFGameState.h"

UUTHUDWidget_TeamLives::UUTHUDWidget_TeamLives(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Position=FVector2D(0.0f, 0.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.5f, 0.15f);
	Origin=FVector2D(0.5f,0.0f);
}

void UUTHUDWidget_TeamLives::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);

	RedTeamCount.GetTextDelegate.BindUObject(this, &UUTHUDWidget_TeamLives::GetRedTeamLives_Implementation);
	BlueTeamCount.GetTextDelegate.BindUObject(this, &UUTHUDWidget_TeamLives::GetBlueTeamLives_Implementation);
}

bool UUTHUDWidget_TeamLives::ShouldDraw_Implementation(bool bShowScores)
{
	return !bShowScores;
}


FText UUTHUDWidget_TeamLives::GetRedTeamLives_Implementation()
{
	if ( Cast<AUTCTFGameState>(UTGameState) )
	{
		return FText::AsNumber(((AUTCTFGameState*)(UTGameState))->RedLivesRemaining);
	}

	return FText::AsNumber(0);
}

FText UUTHUDWidget_TeamLives::GetBlueTeamLives_Implementation()
{
	if ( Cast<AUTCTFGameState>(UTGameState) )
	{
		return FText::AsNumber(((AUTCTFGameState*)(UTGameState))->BlueLivesRemaining);
	}

	return FText::AsNumber(0);
}

