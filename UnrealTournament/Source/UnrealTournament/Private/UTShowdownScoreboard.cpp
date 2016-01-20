// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTShowdownScoreboard.h"
#include "UTShowdownGameState.h"

UUTShowdownScoreboard::UUTShowdownScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CellHeight = 120.f;
	ColumnHeaderScoreX = 0.65f;
	ColumnHeaderKillsX = 0.75f;
	ColumnHeaderDeathsX = 0.85f;
}

void UUTShowdownScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	Super::DrawPlayer(Index, PlayerState, RenderDelta, XOffset, YOffset);

	// strike out players that are dead
	AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
	float Width = (Size.X * 0.5f) - CenterBuffer;
	if (GS != NULL && GS->bMatchHasStarted)
	{
		if (PlayerState->bOutOfLives)
		{
			float Height = 8.0f;
			float XL, YL;
			Canvas->TextSize(UTHUDOwner->MediumFont, PlayerState->PlayerName, XL, YL, 1.f, 1.f);
			float StrikeWidth = FMath::Min(0.475f*Width, XL);
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY + 0.5f*Height, StrikeWidth, Height, 185.f, 400.f, 4.f, 4.f, 1.0f, FLinearColor::Red);

			// draw skull here
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (Width * ColumnHeaderPlayerX), YOffset + 0.35f*CellHeight, 0.5f*CellHeight, 0.5f*CellHeight, 725, 0, 28, 36, 1.0, FLinearColor::White);
		}
		else if ((GS->GetMatchState() == MatchState::MatchIntermission) || GS->HasMatchEnded() || GS->OnSameTeam(PlayerState, UTHUDOwner->PlayerOwner))
		{
			AUTCharacter* UTC = PlayerState->GetUTCharacter();
			if (UTC)
			{
				// draw armor and health bars
				float HealthPct = FMath::Max<float>(0.0f, float(UTC->Health) / float(UTC->SuperHealthMax));
				float ArmorPct = float(UTC->ArmorAmount) / float(UTC->MaxStackedArmor);
				float Height = 8.f;
				float YPos = YOffset + 0.5f*CellHeight;
				XOffset += 0.05f*Width;
				Width *= 0.75f;
				FLinearColor BarColor = FLinearColor(0.5f, 0.5f, 0.5f, 0.5f);
				DrawTexture(UTHUDOwner->HUDAtlas, XOffset + 1.0f, YPos, Width, Height, 185.f, 400.f, 4.f, 4.f, 1.f, BarColor);
				// armor BG
				DrawTexture(UTHUDOwner->HUDAtlas, XOffset + 1.0f, YPos + 1.25f*Height, Width, Height, 185.f, 400.f, 4.f, 4.f, 1.f, BarColor);
				// health bar
				BarColor = FLinearColor::Green;
				BarColor.A = 0.5f;
				DrawTexture(UTHUDOwner->HUDAtlas, XOffset + 2.0f, YPos + 1.0f, (Width - 2.0f) * HealthPct, Height - 2.0f, 185.f, 400.f, 4.f, 4.f, 1.f, BarColor);
				// armor bar
				BarColor = FLinearColor::Yellow;
				BarColor.A = 0.5f;
				DrawTexture(UTHUDOwner->HUDAtlas, XOffset + 2.0f, YPos + 1.0f + 1.25f*Height, (Width - 2.0f) * ArmorPct, Height - 2.0f, 185.f, 400.f, 4.f, 4.f, 1.f, BarColor);
			}
		}
	}
}

void UUTShowdownScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float XOffset = 0.f;
	float Width = (Size.X * 0.5f) - CenterBuffer;
	float Height = 23;

	FText CH_PlayerName = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerName", "Player");
	FText CH_Score = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerScore", "Score");
	FText CH_Kills = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerKills", "KILLS");
	FText CH_Deaths = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerDeaths", "DEATHS");
	FText CH_Ping = (GetWorld()->GetNetMode() == NM_Standalone) ? NSLOCTEXT("UTScoreboard", "ColumnHeader_BotSkill", "Skill") : NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerPing", "Ping");
	FText CH_Ready = NSLOCTEXT("UTScoreboard", "ColumnHeader_Ready", "");

	for (int32 i = 0; i < 2; i++)
	{
		// Draw the background Border
		DrawTexture(TextureAtlas, XOffset, YOffset, Width, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));

		DrawText(CH_PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);
		if (UTGameState && UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Kills, XOffset + (Width * ColumnHeaderKillsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Deaths, XOffset + (Width * ColumnHeaderDeathsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawText(CH_Ready, XOffset + (Width * ColumnHeaderKillsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		DrawText(CH_Ping, XOffset + (Width * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		XOffset = Size.X - Width;
	}

	YOffset += Height + 4.f;
}


void UUTShowdownScoreboard::DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor)
{
	DrawText(FText::AsNumber(int32(PlayerState->Score)), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Kills), XOffset + (Width * ColumnHeaderKillsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Deaths), XOffset + (Width * ColumnHeaderDeathsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}
void UUTShowdownScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->bMatchHasStarted && ((GS->GetMatchState() == MatchState::MatchIntermission) || GS->HasMatchEnded()))
	{
		// show current round damage and total damage by local player
		float Width = 0.5f*Size.X;
		float Height = 0.2f*Size.Y;
		float XOffset = 0.5*(Size.X - CenterBuffer) - Width;
		float YOffset = 0.62f*Size.Y;
		float TextOffset = 0.1f*Size.X;
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset, YOffset, Width, Height, 185.f, 400.f, 4.f, 4.f, 1.f, FLinearColor(0.5f, 0.5f, 0.5f, 0.2f));
		DrawText(NSLOCTEXT("UTScoreboard", "DamageDone", "DAMAGE DONE BY YOU"), XOffset + TextOffset, YOffset + 0.02f*Height, UTHUDOwner->MediumFont, 1.f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
		DrawText(NSLOCTEXT("UTScoreboard", "ThisRound", "THIS ROUND"), XOffset + TextOffset, YOffset + 0.35f*Height, UTHUDOwner->MediumFont, 1.f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
		DrawText(NSLOCTEXT("UTScoreboard", "ThisMatch", "THIS MATCH"), XOffset + TextOffset, YOffset + 0.65f*Height, UTHUDOwner->MediumFont, 1.f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);

		AUTPlayerState* PS = (UTHUDOwner && UTHUDOwner->PlayerOwner) ? Cast <AUTPlayerState>(UTHUDOwner->PlayerOwner->PlayerState) : NULL;
		if (PS)
		{
			FFormatNamedArguments Args;
			Args.Add("Val", FText::AsNumber(int32(PS->RoundDamageDone)));
			DrawText(FText::Format(NSLOCTEXT("UTScoreboard", "Value", "{Val}"), Args), XOffset + 0.6f*Width, YOffset + 0.4f*Height, UTHUDOwner->MediumFont, 1.f, 1.0f, FLinearColor::Yellow, ETextHorzPos::Left, ETextVertPos::Top);
			FFormatNamedArguments ArgsB;
			ArgsB.Add("Val", FText::AsNumber(int32(PS->DamageDone)));
			DrawText(FText::Format(NSLOCTEXT("UTScoreboard", "Value", "{Val}"), ArgsB), XOffset + 0.6f*Width, YOffset + 0.7f*Height, UTHUDOwner->MediumFont, 1.f, 1.0f, FLinearColor::Yellow, ETextHorzPos::Left, ETextVertPos::Top);
		}

		// show kills feed for this round
		XOffset = 0.5f* (Size.X + CenterBuffer);
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset, YOffset, Width, Height, 185.f, 400.f, 4.f, 4.f, 1.f, FLinearColor(0.5f, 0.5f, 0.5f, 0.2f));
	}
}

