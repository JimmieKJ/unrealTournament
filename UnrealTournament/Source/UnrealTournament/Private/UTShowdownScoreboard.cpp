// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTShowdownScoreboard.h"
#include "UTShowdownGameState.h"

UUTShowdownScoreboard::UUTShowdownScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CellHeight = 120.f;
}

void UUTShowdownScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	Super::DrawPlayer(Index, PlayerState, RenderDelta, XOffset, YOffset);

	// strike out players that are dead
	AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
	float Width = (Size.X * 0.5f) - CenterBuffer;
	if (GS != NULL && GS->bMatchHasStarted)
	{
		AUTCharacter* UTC = PlayerState->GetUTCharacter();
		if (UTC == NULL || UTC->IsDead())
		{
			float Height = 8.0f;
			float XL, YL;
			Canvas->TextSize(UTHUDOwner->MediumFont, PlayerState->PlayerName, XL, YL, 1.f, 1.f);
			float StrikeWidth = FMath::Min(0.475f*Width, XL);
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY + 0.5f*Height, StrikeWidth, Height, 185.f, 400.f, 4.f, 4.f, 1.0f, FLinearColor::Red);

			// draw skull here
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (Width * ColumnHeaderPlayerX), YOffset + 0.35f*CellHeight, 0.5f*CellHeight, 0.5f*CellHeight, 725, 0, 28, 36, 1.0, FLinearColor::White);
		}
		else if ((GS->GetMatchState() == MatchState::MatchIntermission) || GS->OnSameTeam(UTC, UTHUDOwner->PlayerOwner))
		{
			// draw armor and health bars
			float HealthPct = float(UTC->Health) / float(UTC->SuperHealthMax);
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

void UUTShowdownScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	AUTShowdownGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->bMatchHasStarted && (GS->GetMatchState() == MatchState::MatchIntermission))
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

