// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_SpectatorSlideOut.h"

UUTHUDWidget_SpectatorSlideOut::UUTHUDWidget_SpectatorSlideOut(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(400.0f, 108.0f);
	ScreenPosition = FVector2D(0.0f, 0.1f);
	Origin = FVector2D(0.0f, 0.0f);

	ColumnHeaderPlayerX = 0.135;
	ColumnHeaderScoreX = 0.7;
	ColumnY = 12;

	CellHeight = 40;
	FlagX = 0.075;
	SlideIn = 0.f;
	CenterBuffer = 10.f;
	SlideSpeed = 6.f;

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FlagTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/CountryFlags.CountryFlags'"));
	FlagAtlas = FlagTex.Object;
}

bool UUTHUDWidget_SpectatorSlideOut::ShouldDraw_Implementation(bool bShowScores)
{
	if (!bShowScores && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTGameState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{
		if (UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted() || UTGameState->IsMatchAtHalftime())
		{
			return false;
		}
		return (UTHUDOwner->UTPlayerOwner->bRequestingSlideOut || (SlideIn > 0.f));
	}
	return false;
}

void UUTHUDWidget_SpectatorSlideOut::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	if (TextureAtlas && UTGameState)
	{
		SlideIn = UTHUDOwner->UTPlayerOwner->bRequestingSlideOut ? FMath::Min(Size.X, SlideIn + DeltaTime*Size.X*SlideSpeed) : FMath::Max(0.f, SlideIn - DeltaTime*Size.X*SlideSpeed);

		int32 Place = 1;
		int32 MaxRedPlaces = UTGameState->bTeamGame ? 5 : 10;
		int32 XOffset = SlideIn - Size.X;
		float DrawOffset = 0.f;
		UTGameState->FillPlayerLists();
		for (int32 i = 0; i<UTGameState->RedPlayerList.Num(); i++)
		{
			AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->RedPlayerList[i]);
			if (PlayerState)
			{
				DrawPlayer(Place, PlayerState, DeltaTime, XOffset, DrawOffset);
				DrawOffset += CellHeight;
				Place++;
				if (Place > MaxRedPlaces)
				{
					break;
				}
			}
		}
		if (UTGameState->bTeamGame)
		{
			Place = MaxRedPlaces + 1;
			for (int32 i = 0; i<UTGameState->BluePlayerList.Num(); i++)
			{
				AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->BluePlayerList[i]);
				if (PlayerState)
				{
					DrawPlayer(Place, PlayerState, DeltaTime, XOffset, DrawOffset);
					DrawOffset += CellHeight;
					Place++;
					if (Place > 10)
					{
						break;
					}
				}
			}
		}
	}
}

void UUTHUDWidget_SpectatorSlideOut::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;	// Safeguard

	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3;
	float Width = Size.X;

	FText Position = FText::Format(NSLOCTEXT("UTScoreboard", "PositionFormatText", "{0}."), FText::AsNumber(Index));
	FText PlayerName = FText::FromString(GetClampedName(PlayerState, UTHUDOwner->MediumFont, 1.f, 0.475f*Width));
	FText PlayerScore = FText::AsNumber(int32(PlayerState->Score));

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::Black;
	if (PlayerState->Team)
	{
		BarColor = (PlayerState->Team->TeamIndex == 0) ? FLinearColor::Red : FLinearColor::Blue;
	}
	float FinalBarOpacity = BarOpacity;
	/*
	if (PlayerState == SelectedPlayer)
	{
		BarColor = FLinearColor(0.0, 0.3, 0.3, 1.0);
		FinalBarOpacity = 0.75f;
	}*/

	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);	// NOTE: Once I make these interactable.. have a selection color too

	int32 FlagU = (PlayerState->CountryFlag % 8) * 32;
	int32 FlagV = (PlayerState->CountryFlag / 8) * 24;

	DrawTexture(FlagAtlas, XOffset + (Width * FlagX), YOffset + 18, 32, 24, FlagU, FlagV, 32, 24, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));	// Add a function to support additional flags

	// Draw the Text
	DrawText(Position, XOffset + (Width * FlagX - 5), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Right, ETextVertPos::Center);
	FVector2D NameSize = DrawText(PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (UTGameState && UTGameState->HasMatchStarted())
	{
		DrawText(PlayerScore, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
}
