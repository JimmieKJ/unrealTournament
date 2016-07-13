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
	CenterBuffer = 440.f;
}

void UUTShowdownScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	Super::DrawPlayer(Index, PlayerState, RenderDelta, XOffset, YOffset);

	// strike out players that are dead
	AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
	float Width = ScaledCellWidth;
	if (GS != NULL && GS->bMatchHasStarted && !PlayerState->bOutOfLives
		&& ((GS->GetMatchState() == MatchState::MatchIntermission) || GS->HasMatchEnded() || GS->OnSameTeam(PlayerState, UTHUDOwner->PlayerOwner)))
	{
		AUTCharacter* UTC = PlayerState->GetUTCharacter();
		if (UTC)
		{
			// draw armor and health bars
			float HealthPct = FMath::Clamp<float>(float(UTC->Health) / float(UTC->SuperHealthMax), 0.f, 1.f);
			float ArmorPct = float(UTC->GetArmorAmount()) / float(UTC->MaxStackedArmor);
			float Height = 8.f;
			float YPos = YOffset + 0.5f*CellHeight*RenderScale;
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

void UUTShowdownScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float XOffset = ScaledEdgeSize;
	float Height = 23.f*RenderScale;

	for (int32 i = 0; i < 2; i++)
	{
		// Draw the background Border
		DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YOffset, ScaledCellWidth, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));

		DrawText(CH_PlayerName, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);
		if (UTGameState && UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + (ScaledCellWidth * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Kills, XOffset + (ScaledCellWidth * ColumnHeaderKillsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Deaths, XOffset + (ScaledCellWidth * ColumnHeaderDeathsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		DrawText((GetWorld()->GetNetMode() == NM_Standalone) ? CH_Skill : CH_Ping, XOffset + (ScaledCellWidth * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		XOffset = Canvas->ClipX - ScaledCellWidth - ScaledEdgeSize;
	}
	YOffset += Height + 4.f;
}

void UUTShowdownScoreboard::DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor)
{
	DrawText(FText::AsNumber(int32(PlayerState->Score)), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Kills), XOffset + (Width * ColumnHeaderKillsX), YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Deaths), XOffset + (Width * ColumnHeaderDeathsX), YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}

void UUTShowdownScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->bMatchHasStarted && ((GS->GetMatchState() == MatchState::MatchIntermission) || GS->HasMatchEnded()))
	{
		// show current round damage and total damage by local player
		float Width = 0.4f*Canvas->ClipX;
		float Height = 0.2f*Canvas->ClipY;
		float XOffset = 0.05*Canvas->ClipX;
		float YOffset = 0.62f*Canvas->ClipY;
		float TextOffset = 0.1f*Canvas->ClipX;
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset, YOffset, Width, Height, 185.f, 400.f, 4.f, 4.f, 1.f, FLinearColor(0.5f, 0.5f, 0.5f, 0.2f));
		DrawText(NSLOCTEXT("UTScoreboard", "DamageDone", "DAMAGE DONE BY YOU"), XOffset + TextOffset, YOffset + 0.02f*Height, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
		DrawText(NSLOCTEXT("UTScoreboard", "ThisRound", "THIS ROUND"), XOffset + TextOffset, YOffset + 0.35f*Height, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
		DrawText(NSLOCTEXT("UTScoreboard", "ThisMatch", "THIS MATCH"), XOffset + TextOffset, YOffset + 0.65f*Height, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);

		AUTPlayerState* PS = (UTHUDOwner && UTHUDOwner->PlayerOwner) ? Cast <AUTPlayerState>(UTHUDOwner->PlayerOwner->PlayerState) : NULL;
		if (PS)
		{
			FFormatNamedArguments Args;
			Args.Add("Val", FText::AsNumber(int32(PS->RoundDamageDone)));
			DrawText(FText::Format(NSLOCTEXT("UTScoreboard", "Value", "{Val}"), Args), XOffset + 0.8f*Width, YOffset + 0.4f*Height, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::Yellow, ETextHorzPos::Left, ETextVertPos::Top);
			FFormatNamedArguments ArgsB;
			ArgsB.Add("Val", FText::AsNumber(int32(PS->DamageDone)));
			DrawText(FText::Format(NSLOCTEXT("UTScoreboard", "Value", "{Val}"), ArgsB), XOffset + 0.8f*Width, YOffset + 0.7f*Height, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::Yellow, ETextHorzPos::Left, ETextVertPos::Top);
		}

		// show kills feed for this round
		XOffset = 0.5f*Canvas->ClipX;
		Width = 0.3f*Canvas->ClipX;
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset, YOffset, Width, Height, 185.f, 400.f, 4.f, 4.f, 1.f, FLinearColor(0.5f, 0.5f, 0.5f, 0.2f));
	}
}

