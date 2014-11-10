// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_DMPlayerLeaderboard.h"

UUTHUDWidget_DMPlayerLeaderboard::UUTHUDWidget_DMPlayerLeaderboard(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	Position=FVector2D(5.0f, 5.0f);
	Size=FVector2D(200.0f,200.0f);
	ScreenPosition=FVector2D(0.0f, 0.0f);
	Origin=FVector2D(0.0f,0.0f);
}

void UUTHUDWidget_DMPlayerLeaderboard::CalcStanding(int32& Standing, int32& Spread)
{
	// NOTE: By here in the Hud rendering chain, the PlayerArray in the GameState
	// has been sorted.

	Standing = 0;
	Spread = 0;

	AUTGameState* GameState = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState != NULL && UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTPlayerState* MyPS = UTHUDOwner->GetViewedPlayerState();

		// Assume position 1.
		Standing = 1;
		int MyIndex = 0;
		for (int i=0;i<GameState->PlayerArray.Num();i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GameState->PlayerArray[i]);
			if (PS != NULL && !PS->bIsSpectator)
			{
				if (PS != MyPS && PS->Score > MyPS->Score)	// If they are not me, and have a better score, increase my position
				{
					Standing++;
				}
				else
				{
					MyIndex = i;
					break;	// Found my position
				}
			}
		}

		if (Standing > 1)
		{
			Spread = MyPS->Score - GameState->PlayerArray[0]->Score;
		}
		else if (MyIndex < GameState->PlayerArray.Num()-1)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GameState->PlayerArray[MyIndex+1]);					
			Spread = MyPS->Score - PS->Score;
		}
	}
}

FText UUTHUDWidget_DMPlayerLeaderboard::GetPlaceSuffix(int32 Standing)
{
	switch (Standing)
	{
		case 1:  return FText::Format(NSLOCTEXT("UTHUD","FirstPlaceSuffix","{0}st"), FText::AsNumber(Standing)); break;
		case 2:  return FText::Format(NSLOCTEXT("UTHUD","SecondPlaceSuffix","{0}nd"), FText::AsNumber(Standing)); break;
		case 3:  return FText::Format(NSLOCTEXT("UTHUD","ThirdPlaceSuffix","{0}rd"), FText::AsNumber(Standing)); break;
		default: return FText::Format(NSLOCTEXT("UTHUD","NthPlaceSuffix","{0}th"), FText::AsNumber(Standing)); break;
	}

	return FText::GetEmpty();
}

void UUTHUDWidget_DMPlayerLeaderboard::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner->UTPlayerOwner != NULL)
	{
		AUTPlayerState* PS = UTHUDOwner->GetViewedPlayerState();
		if (!PS->bOnlySpectator)
		{
			int32 Standing;
			int32 Spread;

			CalcStanding(Standing, Spread);

			DrawText(FText::Format(NSLOCTEXT("UTHUD", "LeaderboardPlace", "{0} Place"), GetPlaceSuffix(Standing)), 0.0f, 0.0f, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White);

			if (Spread != 0)
			{
				FString SpreadStr = (Spread > 0) ? FString::Printf(TEXT("+%i"), Spread) : FString::Printf(TEXT("%i"), Spread);
				DrawText(FText::FromString(SpreadStr), 0.0f, 32.0f, UTHUDOwner->MediumFont, 0.6666f, 1.0f, FLinearColor::White);
			}
		}
	}
}