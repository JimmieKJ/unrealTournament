// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Intermission.h"
#include "UTCTFGameState.h"

UUTHUDWidget_Intermission::UUTHUDWidget_Intermission(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;

	AssistedByText = NSLOCTEXT("CTF", "AssistedBy", "Assisted by");
	UnassistedText = NSLOCTEXT("CTF", "Unassisted", "Unassisted");
}

bool UUTHUDWidget_Intermission::ShouldDraw_Implementation(bool bShowScores)
{
	if (!bShowScores && GetWorld()->GetGameState() && (GetWorld()->GetGameState()->GetMatchState() == MatchState::MatchIntermission))
	{
		AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
		bool bIsHalfTime = (CTFState->CTFRound == 0);
		bool bHasScore = (CTFState->GetScoringPlays().Num() > 0) && (!bIsHalfTime || (CTFState->GetScoringPlays().Last().RemainingTime <= 0.f));
		return bHasScore;
	}
	return false;
}

void UUTHUDWidget_Intermission::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	Canvas->SetLinearDrawColor(FLinearColor::White);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;

	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	bool bIsHalfTime = (CTFState->CTFRound == 0);
	bool bHasScore = (CTFState->GetScoringPlays().Num() > 0) && (!bIsHalfTime || (CTFState->GetScoringPlays().Last().RemainingTime <= 0.f));

	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, "TEST", XL, MedYL, RenderScale, RenderScale);

	float ScoreHeight = MedYL + SmallYL;
	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->MediumFont, "99 - 99", ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);

	// draw background
	float ScoreWidth = 0.5f * Canvas->ClipX;
	FLinearColor DrawColor = FLinearColor::White;
	float IconHeight = 0.8f*ScoreHeight;
	float IconOffset = 0.1f*ScoreWidth;
	float BackAlpha = 0.5f;
	float XOffset = 0.5f*(Canvas->ClipX - ScoreWidth);
	float YPos = 0.7f*Canvas->ClipY;
	DrawTexture(TextureAtlas, XOffset, YPos, ScoreWidth, ScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);

	if (bHasScore)
	{
		const FCTFScoringPlay& Play = CTFState->GetScoringPlays().Last();

		// draw scoring team icon
		int32 IconIndex = Play.Team->TeamIndex;
		IconIndex = FMath::Min(IconIndex, 1);
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset + IconOffset, YPos + 0.5f*(ScoreHeight - IconHeight), IconHeight, IconHeight, UTHUDOwner->TeamIconUV[IconIndex].X, UTHUDOwner->TeamIconUV[IconIndex].Y, 72, 72, 1.f, Play.Team->TeamColor);

		FString ScoredByLine = Play.ScoredBy.GetPlayerName();
		if (Play.ScoredByCaps > 1)
		{
			ScoredByLine += FString::Printf(TEXT(" (%i)"), Play.ScoredByCaps);
		}

		// time of game
		FString TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), Play.RemainingTime, false, true, false).ToString();
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(UTHUDOwner->SmallFont, TimeStampLine, XOffset + 0.04f*ScoreWidth, YPos + 0.5f*ScoreHeight - 0.5f*SmallYL, RenderScale, RenderScale, TextRenderInfo);

		// scored by
		Canvas->SetLinearDrawColor(Play.Team->TeamColor);
		float ScorerNameYOffset = YPos;
		Canvas->DrawText(UTHUDOwner->MediumFont, ScoredByLine, XOffset + 0.22f*ScoreWidth, ScorerNameYOffset, RenderScale, RenderScale, TextRenderInfo);

		// assists
		FString AssistLine;
		if (Play.Assists.Num() > 0)
		{
			AssistLine = AssistedByText.ToString() + TEXT(" ");
			for (const FCTFAssist& Assist : Play.Assists)
			{
				AssistLine += Assist.AssistName.GetPlayerName() + TEXT(", ");
			}
			AssistLine = AssistLine.LeftChop(2);
		}
		else
		{
			AssistLine = UnassistedText.ToString();
		}
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(UTHUDOwner->SmallFont, AssistLine, XOffset + 0.22f*ScoreWidth, YPos + MedYL, RenderScale, RenderScale, TextRenderInfo);

		if (CTFState->Teams[0] && CTFState->Teams[1])
		{
			float SingleXL, SingleYL;
			YPos = YPos + 0.5f*ScoreHeight - 0.6f*ScoringOffsetY;
			float ScoreX = XOffset + 0.95f*ScoreWidth - ScoringOffsetX;
			Canvas->SetLinearDrawColor(CTFState->Teams[0]->TeamColor);
			FString SingleScorePart = FString::Printf(TEXT(" %i"), Play.TeamScores[0]);
			Canvas->TextSize(UTHUDOwner->MediumFont, SingleScorePart, SingleXL, SingleYL, RenderScale, RenderScale);
			Canvas->DrawText(UTHUDOwner->MediumFont, SingleScorePart, ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			Canvas->SetLinearDrawColor(FLinearColor::White);
			ScoreX += SingleXL;
			Canvas->TextSize(UTHUDOwner->MediumFont, "-", SingleXL, SingleYL, RenderScale, RenderScale);
			ScoreX += SingleXL;
			Canvas->DrawText(UTHUDOwner->MediumFont, "-", ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			Canvas->SetLinearDrawColor(CTFState->Teams[1]->TeamColor);
			ScoreX += SingleXL;
			Canvas->DrawText(UTHUDOwner->MediumFont, FString::Printf(TEXT(" %i"), Play.TeamScores[1]), ScoreX, YPos, RenderScale, RenderScale, TextRenderInfo);
			YPos = YPos + ScoreHeight + 8.f*RenderScale;
		}
	}
}