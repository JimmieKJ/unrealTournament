// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_GameClock.h"
#include "UTCTFGameState.h"

UUTHUDWidget_GameClock::UUTHUDWidget_GameClock(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	static ConstructorHelpers::FObjectFinder<UTexture> Tex(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	HudTexture = Tex.Object;

	Position=FVector2D(5.0f, 5.0f);
	Size=FVector2D(0.0f,0.0f);
	ScreenPosition=FVector2D(0.0f, 0.0f);
	Origin=FVector2D(0.0f,0.0f);

}

void UUTHUDWidget_GameClock::Draw_Implementation(float DeltaTime)
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{

		float OldScale = RenderScale;

		if (GS->IsMatchInProgress() && GS->RemainingTime <= 10.0)
		{
			BounceValue += DeltaTime * 6;
			RenderScale *= 1.0 + (0.1 * sin(BounceValue));
		}
		else
		{
			BounceValue = 0.0f;
		}

		// Draw the background
		DrawTexture(HudTexture, 0.0f,0.0f, 181.0f,43.0f, 491.0f,396.0f,181.0f,43.0f,1.0f,ApplyHUDColor(FLinearColor::White));
		FText TimeStr = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), GS->RemainingTime);
		DrawText(TimeStr, 37, 8, UTHUDOwner->NumberFont, 0.5, 1.0, BounceValue > 0.0 ? FLinearColor::Yellow : FLinearColor::White, ETextHorzPos::Left);

		AUTCTFGameState* CGS = Cast<AUTCTFGameState>(GS);
		if (CGS != NULL)
		{
			if (!GS->IsMatchInProgress())
			{
				if (GS->HasMatchEnded())
				{
					DrawText(NSLOCTEXT("GameClock", "PostGame", "!! Game Over !!"), 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White, ETextHorzPos::Center);
				}
				else
				{
					DrawText(NSLOCTEXT("GameClock", "PreGame", "!! Pre-Game !!"), 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White, ETextHorzPos::Center);
				}
			}
			else if (CGS->IsMatchInSuddenDeath())
			{
				DrawText(NSLOCTEXT("GameClock", "SuddenDeath", "!! Sudden Death !!"), 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White, ETextHorzPos::Center);
			}
			else if (GS->IsMatchInOvertime())
			{
				DrawText(NSLOCTEXT("GameClock","OverTime","!! Overtime !!"), 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White,ETextHorzPos::Center);
			}
			else if (CGS->bHalftime)
			{
				if (CGS->bSecondHalf)
				{
					DrawText(NSLOCTEXT("GameClock", "PreOvertime", "!! Get Ready !!"), 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White, ETextHorzPos::Center);

				}
				else
				{
					DrawText(NSLOCTEXT("GameClock", "HalfTime", "!! Halftime !!"), 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White, ETextHorzPos::Center);

				}
			}
			else if (CGS->bPlayingAdvantage)
			{
				FText AdvantageText = CGS->AdvantageTeamIndex == 0 ? NSLOCTEXT("GameClock", "RedAdvantage", "!! Red Advantage !!") : NSLOCTEXT("GameClock", "BlueAdvantage", "!! Blue Advantage !!");
				FLinearColor AdvantageColor = CGS->AdvantageTeamIndex == 1 ? FLinearColor::Red : FLinearColor::Blue;
				DrawText(AdvantageText, 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, AdvantageColor, ETextHorzPos::Center);
			}
			else
			{
				FText HalfText = !CGS->bSecondHalf ? NSLOCTEXT("CTFScore","FirstHalf","First Half") : NSLOCTEXT("CTFScore","SecondHalf","Second Half");
				DrawText(HalfText, 90, 50, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White,ETextHorzPos::Center);
			}
		}

		RenderScale = OldScale;
	}
}
