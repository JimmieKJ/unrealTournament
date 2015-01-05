// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTScoreboard.h"

UUTScoreboard::UUTScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(1269.0f, 974.0f);
	ScreenPosition = FVector2D(0.5f, 0.5f);
	Origin = FVector2D(0.5f, 0.5f);

	ColumnHeaderPlayerX = 15;
	ColumnHeaderScoreX = 403;
	ColumnHeaderDeathsX = 480;
	ColumnHeaderPingX = 570;
	ColumnHeaderY = 8;
	ColumnY = 12;

	// Preload the fonts

	static ConstructorHelpers::FObjectFinder<UFont> LFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Large.fntScoreboard_Large'"));
	LargeFont = LFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> MFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Medium.fntScoreboard_Medium'"));
	MediumFont = MFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> SFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Small.fntScoreboard_Small'"));
	SmallFont = SFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> TFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Tiny.fntScoreboard_Tiny'"));
	TinyFont = TFont.Object;

	static ConstructorHelpers::FObjectFinder<UFont> CFont(TEXT("Font'/Game/RestrictedAssets/UI/Fonts/fntScoreboard_Clock.fntScoreboard_Clock'"));
	ClockFont = CFont.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;

}

void UUTScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	float YOffset = 0;
	DrawGamePanel(RenderDelta, YOffset);
	DrawTeamPanel(RenderDelta, YOffset);
	DrawScorePanel(RenderDelta, YOffset);

	YOffset = 936;
	DrawServerPanel(RenderDelta, YOffset);
}

void UUTScoreboard::DrawGamePanel(float RenderDelta, float& YOffset)
{
	// Draw the Background
	DrawTexture(TextureAtlas,0,0, 1269, 128, 4,2,124, 128, 1.0);

	// Draw the Logo
	DrawTexture(TextureAtlas, 165, 60, 301, 98, 162,14,301, 98.0, 1.0f, FLinearColor::White, FVector2D(0.5, 0.5));
	
	// Draw the Spacer Bar
	DrawTexture(TextureAtlas, 325, 60, 4, 99, 488,13,4,99, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));

	// Draw Game Text
	DrawText(NSLOCTEXT("UTScoreBoard","Game","GAME"), 455, 38, MediumFont, 1.0, 1.0, FLinearColor(0.64,0.64,0.64,1.0), ETextHorzPos::Right, ETextVertPos::Center );
	DrawText(NSLOCTEXT("UTScoreBoard","Map", "MAP"),  455, 90, MediumFont, 1.0, 1.0, FLinearColor(0.64, 0.64, 0.64, 1.0), ETextHorzPos::Right, ETextVertPos::Center);

	FText MapName = UTHUDOwner ? FText::FromString(UTHUDOwner->GetWorld()->GetMapName().ToUpper()) : FText::GetEmpty();

	FText GameName = FText::GetEmpty();
	if (UTGameState && UTGameState->GameModeClass)
	{
		AUTGameMode* DefaultGame = UTGameState->GameModeClass->GetDefaultObject<AUTGameMode>();
		if (DefaultGame) 
		{
			GameName = FText::FromString(DefaultGame->DisplayName.ToString().ToUpper());
		}
	}

	if (!GameName.IsEmpty())
	{
		DrawText(GameName, 470, 28, LargeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	}
	if (!MapName.IsEmpty())
	{
		DrawText(MapName, 470, 80, LargeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	}

	DrawGameOptions(RenderDelta, YOffset);
	YOffset += 128;	// The size of this zone.

}
void UUTScoreboard::DrawGameOptions(float RenderDelta, float& YOffset)
{
	if (UTGameState)
	{
		if (UTGameState->GoalScore > 0)
		{
			// Draw Game Text
			FText Score = FText::Format(NSLOCTEXT("UTScoreboard","GoalScoreFormat","First to {0} Frags"), FText::AsNumber(UTGameState->GoalScore));
			DrawText(Score, 1255, 28, MediumFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
		}

		FText StatusText = UTGameState->GetGameStatusText();
		if (!StatusText.IsEmpty())
		{
			DrawText(StatusText, 1255, 90, MediumFont, 1.0, 1.0, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		}
		else
		{
			FText Timer = (UTGameState->TimeLimit > 0) ? UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), UTGameState->RemainingTime, true, true, true) :
															UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), UTGameState->ElapsedTime, true, true, true);

			DrawText(Timer, 1255, 88, ClockFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
		}
	}
}


void UUTScoreboard::DrawTeamPanel(float RenderDelta, float& YOffset)
{
	YOffset += 39.0; // A small gap
}

void UUTScoreboard::DrawScorePanel(float RenderDelta, float& YOffset)
{
	float DrawY = YOffset;

	DrawScoreHeaders(RenderDelta, DrawY);
	DrawPlayerScores(RenderDelta, DrawY);

}

void UUTScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float XOffset = 0.0;
	float Width = 625;  // 20 pixels between them
	float Height = 23;

	FText CH_PlayerName = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerName", "PLAYER");
	FText CH_Score = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerScore", "SCORE");
	FText CH_Deaths = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerDeaths", "DEATHS");
	FText CH_Ping = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerPing", "PING");
	FText CH_Ready = NSLOCTEXT("UTScoreboard","ColumnHeader_Ready","READY TO PLAY");

	for (int32 i = 0; i < 2; i++)
	{
		// Draw the background Border
		DrawTexture(TextureAtlas, XOffset, YOffset, Width, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));

		DrawText(CH_PlayerName, XOffset + ColumnHeaderPlayerX, YOffset + ColumnHeaderY, TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);

		if (UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + ColumnHeaderScoreX, YOffset + ColumnHeaderY, TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Deaths, XOffset + ColumnHeaderDeathsX, YOffset + ColumnHeaderY, TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawText(CH_Ready, XOffset + ColumnHeaderScoreX, YOffset + ColumnHeaderY, TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		DrawText(CH_Ping, XOffset + ColumnHeaderPingX, YOffset + ColumnHeaderY, TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);

		XOffset += Width + 20;
	}

	YOffset += Height + 4;
}

void UUTScoreboard::DrawPlayerScores(float RenderDelta, float& YOffset)
{
	int32 Place = 1;
	int32 NumSpectators = 0;
	int32 XOffset = 0;
	for (int32 i=0; i<UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PlayerState)
		{
			if (!PlayerState->bOnlySpectator)
			{
				DrawPlayer(Place, PlayerState, RenderDelta, XOffset, YOffset);
				XOffset = XOffset == 0 ? 648 : 0;
				if (XOffset == 0) YOffset += 40;

				Place++;
			}
			else
			{
				NumSpectators++;
			}
		}
	}
	
	if (UTGameState->PlayerArray.Num() <= 28 && NumSpectators > 0)
	{
		FText SpectatorCount = FText::Format(NSLOCTEXT("UTScoreboard","SpectatorFormat","{0} spectators are watching this match"), FText::AsNumber(NumSpectators));
		DrawText(SpectatorCount, 635, 765, SmallFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Bottom);
	}
}

void UUTScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{

	if (PlayerState == NULL) return;	// Safeguard

	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3;

	FText Position = FText::Format(NSLOCTEXT("UTScoreboard", "PositionFormatText", "{0}."), FText::AsNumber(Index));
	FText PlayerName = FText::FromString(PlayerState->PlayerName);
	FText PlayerScore = FText::AsNumber(int32(PlayerState->Score));
	FText PlayerDeaths = FText::AsNumber(PlayerState->Deaths);

	int32 Ping = PlayerState->Ping * 4;
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == PlayerState)
	{
		Ping = PlayerState->ExactPing;
		DrawColor = FLinearColor(0.0f,0.92f,1.0f,1.0f);
		BarOpacity = 0.5;
	}

	FText PlayerPing = FText::Format(NSLOCTEXT("UTScoreboard","PingFormatText","{0}ms"), FText::AsNumber(Ping));

	// Draw the position
	
	// Draw the background border.
	DrawTexture(TextureAtlas, XOffset, YOffset, 625, 36, 149, 138, 32, 32, BarOpacity, FLinearColor::Black);	// NOTE: Once I make these interactable.. have a selection color too
	DrawTexture(TextureAtlas, XOffset + 72, YOffset + 18, 32,24, 193,138,32,24,1.0, FLinearColor::White, FVector2D(0.5f,0.5f));	// Add a function to support additional flags

	// Draw the Text
	DrawText(Position, XOffset + 50, YOffset + ColumnY, MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Right, ETextVertPos::Center);
	DrawText(PlayerName, XOffset + 94, YOffset + ColumnY, MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center); 		

	if (UTGameState->HasMatchStarted())
	{
		DrawText(PlayerScore, XOffset + ColumnHeaderScoreX, YOffset + ColumnY, MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(PlayerDeaths, XOffset + ColumnHeaderDeathsX, YOffset + ColumnY, SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	else
	{
		FText PlayerReady = PlayerState->bReadyToPlay ? NSLOCTEXT("UTScoreboard", "YES", "YES") : NSLOCTEXT("UTScoreboard", "NO", "NO");
		DrawText(PlayerReady, XOffset + ColumnHeaderScoreX, YOffset + ColumnY, MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	DrawText(PlayerPing, XOffset + ColumnHeaderPingX, YOffset + ColumnY, SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}


void UUTScoreboard::DrawServerPanel(float RenderDelta, float& YOffset)
{
	if (UTGameState)
	{
		// Draw the Background
		DrawTexture(TextureAtlas, 0, YOffset, 1269, 38, 4, 132, 30, 38, 1.0);
		DrawText(FText::FromString(UTGameState->ServerName), 10, YOffset + 13, SmallFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawText(FText::FromString(UTGameState->ServerDescription), 1259, YOffset + 13, SmallFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
	}
}

