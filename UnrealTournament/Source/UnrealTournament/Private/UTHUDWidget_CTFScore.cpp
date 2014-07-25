// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFScore.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFScore::UUTHUDWidget_CTFScore(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	ScreenPosition = FVector2D(0.5f, 0.0f);
	static ConstructorHelpers::FObjectFinder<UTexture> HudTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseA.UI_HUD_BaseA'"));
	IconTexture = HudTexture.Object;
}

void UUTHUDWidget_CTFScore::Draw_Implementation(float DeltaTime)
{
	FLinearColor BGColor = ApplyHUDColor(FLinearColor::White);

	float RedScale  = UTHUDOwner->UTPlayerOwner->GetTeamNum() == 0 ? 1.25 : 1.0;
	float BlueScale = UTHUDOwner->UTPlayerOwner->GetTeamNum() == 1 ? 1.25 : 1.0;

	DrawTexture(IconTexture, -162 * RedScale - 15, 0, 162 * RedScale, 80 * RedScale, 1,0,108,53,1.0,BGColor);
	DrawTexture(IconTexture, -15,  0, 30, 80,  109,0,20,53,1.0,BGColor);
	DrawTexture(IconTexture, 15, 0, 162 * BlueScale, 80 * BlueScale, 129,0,108,53,1.0,BGColor);

	// Draw the Red Score...

	AUTCTFGameState* CGS = UTHUDOwner->GetWorld()->GetGameState<AUTCTFGameState>();
	if (CGS != NULL)
	{
		//HACK: temp fix. Seems like the hud is trying to draw when the teams havn't fully replicated
		if ((CGS->Teams.Num() != 2) || (CGS->Teams[0] == NULL)  || (CGS->Teams[1] == NULL))
		{
			return;
		}
		// Draw the Red Score...
		UTHUDOwner->TempDrawNumber(CGS->Teams[0]->Score, RenderPosition.X - (157 * RenderScale * RedScale) , 10 * RenderScale * RedScale, FLinearColor::Yellow, 1.0,RenderScale * RedScale * 0.75);
	
		// Draw the Blue Score...
		UTHUDOwner->TempDrawNumber(CGS->Teams[1]->Score, RenderPosition.X + (75 * RenderScale * BlueScale) , 10 * RenderScale * BlueScale, FLinearColor::Yellow, 1.0,RenderScale * BlueScale * 0.75);

		// Draw the Red Flag State...

		FName FlagState = CGS->GetFlagState(0);

		if (FlagState == CarriedObjectState::Dropped)
		{
			DrawFlagIcon(-46 * RedScale,53 * RedScale, 25,34, 894, 1, 25,34, FLinearColor::Red, RedScale);
		}
		else if (FlagState == CarriedObjectState::Held)
		{
			DrawFlagIcon(-46 * RedScale,53 * RedScale, 43,35, 843, 50, 43,35, FLinearColor::Red, RedScale);
			DrawText(FText::FromString(CGS->GetFlagHolder(0)->PlayerName), -75* RedScale, 56 * RedScale, UTHUDOwner->GetFontFromSizeIndex(0), RedScale,1.0, FLinearColor::White, ETextHorzPos::Right);
		}
		else
		{
			DrawFlagIcon(-46 * RedScale,53 * RedScale, 43,41, 843, 87, 43,41, FLinearColor::Red, RedScale);
		}



		FlagState = CGS->GetFlagState(1);
		if (FlagState == CarriedObjectState::Dropped)
		{
			DrawFlagIcon(46 * BlueScale,53 * BlueScale, 25,34, 894, 1, 25,34, FLinearColor::Blue, BlueScale);
		}
		else if (FlagState == CarriedObjectState::Held)
		{
			DrawFlagIcon(46 * BlueScale,53 * BlueScale, 43,35, 843, 50, 43,35, FLinearColor::Blue, RedScale);
			DrawText(FText::FromString(CGS->GetFlagHolder(1)->PlayerName), 75* BlueScale, 56 * BlueScale, UTHUDOwner->GetFontFromSizeIndex(0), BlueScale,1.0, FLinearColor::White);
		}
		else
		{
			DrawFlagIcon(46 * BlueScale,53 * BlueScale, 43,41, 843, 87, 43,41, FLinearColor::Blue, BlueScale);
		}

		if (CGS->bHalftime)
		{
			FText TimeStr = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), CGS->TimeLimit - CGS->RemainingTime);
			DrawText(TimeStr, 0, 95, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White,ETextHorzPos::Center);
			DrawText(NSLOCTEXT("CTFScore","HalfTime","!! Halftime !!"), 0, 115, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White,ETextHorzPos::Center);
		}
		else
		{
			FText TimeStr = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), (CGS->TimeLimit - CGS->RemainingTime) + (CGS->bSecondHalf ? CGS->TimeLimit : 0));
			DrawText(TimeStr, 0, 95, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White,ETextHorzPos::Center);
			FText HalfText = !CGS->bSecondHalf ? NSLOCTEXT("CTFScore","FirstHalf","First Half") : NSLOCTEXT("CTFScore","SecondHalf","Second Half");
			DrawText(HalfText, 0, 115, UTHUDOwner->GetFontFromSizeIndex(2), 0.5, 1.0, FLinearColor::White,ETextHorzPos::Center);
		}
	}
}

void UUTHUDWidget_CTFScore::DrawFlagIcon(float CenterX, float CenterY,  float Width, float Height, float U, float V, float UL, float VL, FLinearColor DrawColor, float Scale)
{
	// NOTE: the 1.5x are because I scaled up the source art so it didn't look bad.
	DrawTexture(IconTexture, CenterX - (Width * Scale * 0.5), CenterY - (Height * Scale * 0.5), Width * Scale, Height * Scale, U,V,UL,VL,1.0, DrawColor);
}

