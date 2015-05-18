// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFScoreboard.h"
#include "UTTeamScoreboard.h"
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"

UUTCTFScoreboard::UUTCTFScoreboard(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ScoringPlaysHeader = NSLOCTEXT("CTF", "ScoringPlaysHeader", "SCORING PLAYS");
	AssistedByText = NSLOCTEXT("CTF", "AssistedBy", "Assisted by");
	UnassistedText = NSLOCTEXT("CTF", "Unassisted", "Unassisted");
	NoScoringText = NSLOCTEXT("CTF", "NoScoring", "No Scoring");
	PeriodText[0] = NSLOCTEXT("UTScoreboard", "FirstHalf", "First Half");
	PeriodText[1] = NSLOCTEXT("UTScoreboard", "SecondHalf", "Second Half");
	PeriodText[2] = NSLOCTEXT("UTScoreboard", "Overtime", "Overtime");

	ColumnHeaderScoreX = 0.54;
	ColumnHeaderCapsX = 0.66;
	ColumnHeaderAssistsX = 0.73;
	ColumnHeaderReturnsX = 0.79;
	ReadyX = 0.7f;
	NumPages = 2;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_EnemySpree01.A_UI_EnemySpree01'"));
	ScoreUpdateSound = OtherSpreeSoundFinder.Object;
	ValueColumn = 0.5f;
	ScoreColumn = 0.75f;
}

void UUTCTFScoreboard::OpenScoringPlaysPage()
{
	SetPage(1);
}

void UUTCTFScoreboard::PageChanged_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(OpenScoringPlaysHandle);
	TimeLineOffset = (UTGameState && (UTGameState->IsMatchAtHalftime() || UTGameState->HasMatchEnded())) ? -0.15f : 99999.f;
}

void UUTCTFScoreboard::Draw_Implementation(float DeltaTime)
{
	if (UTHUDOwner->ScoreboardPage > 0)
	{
		float YOffset = 0.0f;
		DrawGamePanel(DeltaTime, YOffset);
		DrawScoringStats(DeltaTime, YOffset);
		DrawServerPanel(DeltaTime, FooterPosY);
	}
	else
	{
		Super::Draw_Implementation(DeltaTime);
	}
}

void UUTCTFScoreboard::DrawGameOptions(float RenderDelta, float& YOffset)
{
	if (UTGameState)
	{
		FText StatusText = UTGameState->GetGameStatusText();
		if (!StatusText.IsEmpty())
		{
			DrawText(StatusText, 1255, 38, UTHUDOwner->MediumFont, 1.0, 1.0, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		} 
		DrawText(UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), UTGameState->RemainingTime, true, true, true), 1255, 88, UTHUDOwner->NumberFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
	}
}

void UUTCTFScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float XOffset = 0.f;
	float Width = (Size.X * 0.5f) - CenterBuffer;
	float Height = 23;

	FText CH_PlayerName = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerName", "Player");
	FText CH_Score = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerScore", "Score");
	FText CH_Caps = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerCaps", "C");
	FText CH_Assists = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerAssists", "A");
	FText CH_Returns = NSLOCTEXT("UTCTFScoreboard", "ColumnHeader_PlayerReturns", "R");
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
			DrawText(CH_Caps, XOffset + (Width * ColumnHeaderCapsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Assists, XOffset + (Width * ColumnHeaderAssistsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Returns, XOffset + (Width * ColumnHeaderReturnsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawText(CH_Ready, XOffset + (Width * ReadyX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}

		DrawText(CH_Ping, XOffset + (Width * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);

		XOffset = Size.X - Width;
	}

	YOffset += Height + 4;
}

void UUTCTFScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;	// Safeguard

	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3f;
	float Width = (Size.X * 0.5f) - CenterBuffer;

	bool bIsUnderCursor = false;
	// If we are interactive, store off the bounds of this cell for selection
	if (bIsInteractive)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + (XOffset * RenderScale), RenderPosition.Y + (YOffset * RenderScale),
										RenderPosition.X + ((XOffset + Width) * RenderScale), RenderPosition.Y + ((YOffset + CellHeight) * RenderScale));

		SelectionStack.Add(FSelectionObject(PlayerState, Bounds));
		bIsUnderCursor = (CursorPosition.X >= Bounds.X && CursorPosition.X <= Bounds.Z && CursorPosition.Y >= Bounds.Y && CursorPosition.Y <= Bounds.W);
	}

	FText PlayerName = FText::FromString(GetClampedName(PlayerState, UTHUDOwner->MediumFont, 1.f, 0.475f*Width));
	FText PlayerScore = FText::AsNumber(int32(PlayerState->Score));
	FText PlayerCaps = FText::AsNumber(PlayerState->FlagCaptures);
	FText PlayerAssists = FText::AsNumber(PlayerState->Assists);
	FText PlayerReturns = FText::AsNumber(PlayerState->FlagReturns);
	int32 Ping = PlayerState->Ping * 4;
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == PlayerState)
	{
		Ping = PlayerState->ExactPing;
		DrawColor = FLinearColor(0.0f, 0.92f, 1.0f, 1.0f);
		BarOpacity = 0.5;
	}
	else if (PlayerState->bIsFriend)
	{
		DrawColor = FLinearColor(FColor(254,255,174,255));
	}

	FText PlayerPing;
	if (GetWorld()->GetNetMode() == NM_Standalone)
	{
		AUTBot* Bot = Cast<AUTBot>(PlayerState->GetOwner());
		PlayerPing = Bot ? FText::AsNumber(Bot->Skill) : FText::FromString(TEXT("-"));
	}
	else
	{
		PlayerPing = FText::Format(NSLOCTEXT("UTScoreboard", "PingFormatText", "{0}ms"), FText::AsNumber(Ping));
	}

	// Draw the background border.
	FLinearColor BarColor = FLinearColor::Black;
	float FinalBarOpacity = BarOpacity;
	if (bIsUnderCursor) 
	{
		BarColor = FLinearColor(0.0, 0.3, 0.0, 1.0);
		FinalBarOpacity = 0.75f;
	}
	if (PlayerState == SelectedPlayer) 
	{
		BarColor = FLinearColor(0.0, 0.3, 0.3, 1.0);
		FinalBarOpacity = 0.75f;
	}

	if (PlayerState->KickPercent > 0)
	{
		FText Kick = FText::Format(NSLOCTEXT("Common","PercFormat","{0}%"),FText::AsNumber(PlayerState->KickPercent));
		DrawText(Kick, XOffset, YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
	}

	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);

	int32 FlagU = (PlayerState->CountryFlag % 8) * 32;
	int32 FlagV = (PlayerState->CountryFlag / 8) * 24;

	DrawTexture(FlagAtlas, XOffset + (Width * FlagX), YOffset + 18, 32,24, FlagU,FlagV,32,24,1.0, FLinearColor::White, FVector2D(0.0f,0.5f));	// Add a function to support additional flags

	// Draw the Text
	FVector2D NameSize = DrawText(PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (PlayerState->bIsFriend)
	{
		DrawTexture(TextureAtlas, XOffset + (Width * ColumnHeaderPlayerX) + NameSize.X + 5, YOffset + 18, 30, 24, 236, 136, 30, 24, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));
	}

	if (UTGameState && UTGameState->HasMatchStarted())
	{
		DrawText(PlayerScore, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(PlayerCaps, XOffset + (Width * ColumnHeaderCapsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(PlayerAssists, XOffset + (Width * ColumnHeaderAssistsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(PlayerReturns, XOffset + (Width * ColumnHeaderReturnsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	else
	{
		PlayerState->UpdateReady();
		FText PlayerReady = PlayerState->bReadyToPlay ? NSLOCTEXT("UTScoreboard", "READY", "READY") : NSLOCTEXT("UTScoreboard", "NOTREADY", "");
		if (PlayerState->bPendingTeamSwitch)
		{
			PlayerReady = NSLOCTEXT("UTScoreboard", "TEAMSWITCH", "TEAM SWAP");
		}
		DrawText(PlayerReady, XOffset + (Width * ReadyX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, PlayerState->ReadyColor, ETextHorzPos::Center, ETextVertPos::Center);
	}

	DrawText(PlayerPing, XOffset + (Width * ColumnHeaderPingX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}

void UUTCTFScoreboard::DrawScoringStats(float DeltaTime, float& YPos)
{
	FVector2D SavedRenderPosition = RenderPosition;
	RenderPosition = FVector2D(0.f, 0.f);
	bScaleByDesignedResolution = false;
	float TopYPos = YPos;

	// draw left side
	float XOffset = Canvas->ClipX * 0.06f;
	float ScoreWidth = 0.5f * (Canvas->ClipX - 3.f*XOffset);
	float MaxHeight = FooterPosY + SavedRenderPosition.Y - YPos;
	DrawScoringPlays(DeltaTime, YPos, XOffset, ScoreWidth, MaxHeight);

	// draw right side
	XOffset = ScoreWidth + 2.f*XOffset;
	YPos = TopYPos;
	DrawScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, MaxHeight);

	bScaleByDesignedResolution = true;
	RenderPosition = SavedRenderPosition;
}

void UUTCTFScoreboard::DrawScoringPlays(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	AUTCTFGameState* CTFState = Cast<AUTCTFGameState>(UTGameState);
	int32 CurrentPeriod = -1;

	Canvas->SetLinearDrawColor(FLinearColor::White);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;

	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, ScoringPlaysHeader.ToString(), XL, MedYL, RenderScale, RenderScale);

	float ScoreHeight = MedYL + SmallYL;
	float ScoringOffsetX, ScoringOffsetY;
	Canvas->TextSize(UTHUDOwner->MediumFont, "99 - 99", ScoringOffsetX, ScoringOffsetY, RenderScale, RenderScale);
	int32 TotalPlays = CTFState->GetScoringPlays().Num();

	// draw background
	FLinearColor DrawColor = FLinearColor::Black;
	DrawColor.A = 0.5f;
	DrawTexture(TextureAtlas, XOffset - 0.05f*ScoreWidth, YPos, 1.1f*ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, DrawColor);

	Canvas->DrawText(UTHUDOwner->MediumFont, ScoringPlaysHeader, XOffset + (ScoreWidth - XL) * 0.5f, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += 1.2f * MedYL;
	if (CTFState->GetScoringPlays().Num() == 0)
	{
		float YL;
		Canvas->TextSize(UTHUDOwner->MediumFont, NoScoringText.ToString(), XL, YL, RenderScale, RenderScale);
		DrawText(NoScoringText, XOffset + (ScoreWidth - XL) * 0.5f, YPos, UTHUDOwner->MediumFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top, TextRenderInfo);
	}

	float OldTimeLineOffset = TimeLineOffset;
	TimeLineOffset += 2.f*DeltaTime;
	float TimeFloor = FMath::FloorToInt(TimeLineOffset);
	if (UTPlayerOwner && (TimeFloor + 1 <= TotalPlays) && (TimeFloor != FMath::FloorToInt(OldTimeLineOffset)))
	{
		UTPlayerOwner->ClientPlaySound(ScoreUpdateSound);
	}
	int32 NumPlays = FMath::Min(TotalPlays, int32(TimeFloor) + 1);
	int32 SmallPlays = FMath::Clamp(2*(NumPlays - 7), 0, NumPlays-1);
	int32 SkippedPlays = FMath::Max(SmallPlays - 10, 0);
	int32 DrawnPlays = 0;
	float PctOffset = 1.f + TimeFloor - TimeLineOffset;
	for (const FCTFScoringPlay& Play : CTFState->GetScoringPlays())
	{
		DrawnPlays++;
		if (DrawnPlays > NumPlays)
		{
			break;
		}
		if (Play.Team != NULL)
		{
			if (Play.Period > CurrentPeriod)
			{
				CurrentPeriod++;
				if (CurrentPeriod < 3)
				{
					DrawText(PeriodText[CurrentPeriod], XOffset + 0.2f*ScoreWidth, YPos, UTHUDOwner->MediumFont, true, FVector2D(1.f, 1.f), FLinearColor::Black, false, FLinearColor::Black, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top, TextRenderInfo);
					YPos += MedYL;
				}
			}
			if (SkippedPlays > 0)
			{
				SkippedPlays--;
				continue;
			}
			float BoxYPos = YPos;
			bool bIsSmallPlay = (SmallPlays > 0);
			if (bIsSmallPlay)
			{
				SmallPlays--;
			}
			// draw background
			FLinearColor DrawColor = FLinearColor::White;
			float CurrentScoreHeight = bIsSmallPlay ? 0.5f*ScoreHeight : ScoreHeight;
			float IconHeight = 0.8f*CurrentScoreHeight;
			float IconOffset = bIsSmallPlay ? 0.12f*ScoreWidth : 0.1f*ScoreWidth;
			float BackAlpha = ((DrawnPlays == NumPlays) && (NumPlays == TimeFloor + 1)) ? FMath::Max(0.5f, PctOffset) : 0.5f;
			DrawTexture(TextureAtlas, XOffset, YPos, ScoreWidth, CurrentScoreHeight, 149, 138, 32, 32, BackAlpha, DrawColor);

			// draw scoring team icon
			int32 IconIndex = Play.Team->TeamIndex;
			IconIndex = FMath::Min(IconIndex, 1);
			DrawTexture(UTHUDOwner->HUDAtlas, XOffset + IconOffset, YPos + 0.1f*CurrentScoreHeight, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[IconIndex].X, UTHUDOwner->TeamIconUV[IconIndex].Y, 72, 72, 1.f, Play.Team->TeamColor);

			FString ScoredByLine = Play.ScoredBy.GetPlayerName();
			if (Play.ScoredByCaps > 1)
			{
				ScoredByLine += FString::Printf(TEXT(" (%i)"), Play.ScoredByCaps);
			}

			// time of game
			FString TimeStampLine = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), Play.RemainingTime, false, true, false).ToString();
			Canvas->SetLinearDrawColor(FLinearColor::White);
			Canvas->DrawText(UTHUDOwner->SmallFont, TimeStampLine, XOffset + 0.05f*ScoreWidth, YPos + 0.5f*CurrentScoreHeight - 0.5f*SmallYL, RenderScale, RenderScale, TextRenderInfo);

			// scored by
			Canvas->SetLinearDrawColor(Play.Team->TeamColor);
			float ScorerNameYOffset = bIsSmallPlay ? BoxYPos + 0.5f*CurrentScoreHeight - 0.6f*MedYL : YPos;
			Canvas->DrawText(UTHUDOwner->MediumFont, ScoredByLine, XOffset + 0.2f*ScoreWidth, ScorerNameYOffset, RenderScale, RenderScale, TextRenderInfo);
			YPos += MedYL;

			if (!bIsSmallPlay)
			{
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
				Canvas->DrawText(UTHUDOwner->SmallFont, AssistLine, XOffset + 0.2f*ScoreWidth, YPos, RenderScale, RenderScale, TextRenderInfo);
			}

			float SingleXL, SingleYL;
			YPos = BoxYPos + 0.5f*CurrentScoreHeight - 0.6f*ScoringOffsetY;
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
			YPos = BoxYPos + CurrentScoreHeight + 8.f*RenderScale;
		}
	}
}

void UUTCTFScoreboard::DrawScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight)
{
	Canvas->SetLinearDrawColor(FLinearColor::White);
	FFontRenderInfo TextRenderInfo;
	TextRenderInfo.bEnableShadow = true;
	TextRenderInfo.bClipText = true;
	FVector2D SavedRenderPosition = RenderPosition;
	RenderPosition = FVector2D(0.f, 0.f);
	bScaleByDesignedResolution = false;

	FLinearColor DrawColor = FLinearColor::Black;
	DrawColor.A = 0.5f;
	DrawTexture(TextureAtlas, XOffset - 0.05f*ScoreWidth, YPos, 1.1f*ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, DrawColor);

	if (!UTPlayerOwner->CurrentlyViewedScorePS)
	{
		UTPlayerOwner->SetViewedScorePS(UTHUDOwner->GetScorerPlayerState());
	}
	AUTPlayerState* PS = UTPlayerOwner->CurrentlyViewedScorePS;
	if (!PS)
	{
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("PlayerName"), FText::FromString(PS->PlayerName));
	FText CombinedHeader = FText::Format(NSLOCTEXT("UTCTFScoreboard", "ScoringBreakDownHeader", "{PlayerName} Scoring Breakdown"), Args);

	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, CombinedHeader.ToString(), XL, MedYL, RenderScale, RenderScale);

	Canvas->DrawText(UTHUDOwner->MediumFont, CombinedHeader, XOffset + 0.5f*(ScoreWidth - XL), YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += 1.2f * MedYL;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Kills", "Kills"), PS->Kills, -1, DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	int32 FCKills = PS->GetStatsValue(NAME_FCKills);
	int32 FlagSupportKills = PS->GetStatsValue(NAME_FlagSupportKills);
	int32 RegularKills = PS->Kills - FCKills - FlagSupportKills;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "RegKills", " - Regular Kills"), RegularKills, PS->GetStatsValue(NAME_RegularKillPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagSupportKills", " - FC Support Kills"), FlagSupportKills, PS->GetStatsValue(NAME_FlagSupportKillPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "EnemyFCKills", " - Enemy FC Kills"), FCKills, PS->GetStatsValue(NAME_FCKillPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "EnemyFCDamage", " Enemy FC Damage Bonus"), -1, PS->GetStatsValue(NAME_EnemyFCDamage), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Deaths", "Deaths"), PS->Deaths, -1, DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);

	Canvas->DrawText(UTHUDOwner->SmallFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += SmallYL;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagGrabs", "Flag Grabs"), PS->GetStatsValue(NAME_FlagGrabs), -1, DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), PS->GetStatsValue(NAME_FlagHeldTime), false);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "FlagHeldTime", "Flag Held Time"), ClockString, -1, DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), PS->GetStatsValue(NAME_FlagHeldDenyTime), false);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "FlagDenialTime", "Flag Denial Time"), ClockString, PS->GetStatsValue(NAME_FlagHeldDeny), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);

	Canvas->DrawText(UTHUDOwner->SmallFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);
	YPos += SmallYL;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagCaps", "Flag Captures"), PS->FlagCaptures, PS->GetStatsValue(NAME_FlagCapPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagReturns", "Flag Returns"), PS->FlagReturns, PS->GetStatsValue(NAME_FlagReturnPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlagAssists", "Assists"), PS->Assists, -1, DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "CarryAssists", " - Carry Assists"), PS->GetStatsValue(NAME_CarryAssist), PS->GetStatsValue(NAME_CarryAssistPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "ReturnAssists", " - Return Assists"), PS->GetStatsValue(NAME_ReturnAssist), PS->GetStatsValue(NAME_ReturnAssistPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "DefendAssists", " - Support Assists"), PS->GetStatsValue(NAME_DefendAssist), PS->GetStatsValue(NAME_DefendAssistPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);
	int32 TeamCaps = PS->Team ? PS->Team->Score - PS->FlagCaptures : 0;
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "TeamCaps", " - Team Cap Bonus"), TeamCaps, PS->GetStatsValue(NAME_TeamCapPoints), DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);

	Canvas->SetLinearDrawColor(FLinearColor::Yellow);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Scoring", "SCORE"), -1, PS->Score, DeltaTime, XOffset, YPos, TextRenderInfo, ScoreWidth, SmallYL);

	bScaleByDesignedResolution = true;
	RenderPosition = SavedRenderPosition;
}

void UUTCTFScoreboard::DrawStatsLine(FText StatsName, int32 StatValue, int32 ScoreValue, float DeltaTime, float XOffset, float& YPos, const FFontRenderInfo& TextRenderInfo, float ScoreWidth, float LineIncrement)
{
	Canvas->DrawText(UTHUDOwner->SmallFont, StatsName, XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);

	if (StatValue >= 0)
	{
		Canvas->DrawText(UTHUDOwner->SmallFont, FString::Printf(TEXT(" %i"), StatValue), XOffset + ValueColumn*ScoreWidth, YPos, RenderScale, RenderScale, TextRenderInfo);
	}
	if (ScoreValue >= 0)
	{
		Canvas->DrawText(UTHUDOwner->SmallFont, FString::Printf(TEXT(" %i"), ScoreValue), XOffset + ScoreColumn*ScoreWidth, YPos, RenderScale, RenderScale, TextRenderInfo);
	}
	YPos += LineIncrement;
}

void UUTCTFScoreboard::DrawTextStatsLine(FText StatsName, FText StatValue, int32 ScoreValue, float DeltaTime, float XOffset, float& YPos, const FFontRenderInfo& TextRenderInfo, float ScoreWidth, float SmallYL)
{
	Canvas->DrawText(UTHUDOwner->SmallFont, StatsName, XOffset, YPos, RenderScale, RenderScale, TextRenderInfo);

	if (!StatValue.IsEmpty())
	{
		Canvas->DrawText(UTHUDOwner->SmallFont, StatValue, XOffset + ValueColumn*ScoreWidth, YPos, RenderScale, RenderScale, TextRenderInfo);
	}
	if (ScoreValue >= 0)
	{
		Canvas->DrawText(UTHUDOwner->SmallFont, FString::Printf(TEXT(" %i"), ScoreValue), XOffset + ScoreColumn*ScoreWidth, YPos, RenderScale, RenderScale, TextRenderInfo);
	}
	YPos += SmallYL;
}
/*
NEED Replication of stats array
*/
