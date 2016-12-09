// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTScoreboard.h"
#include "UTHUDWidget_Spectator.h"
#include "StatNames.h"
#include "UTPickupWeapon.h"
#include "UTWeapon.h"
#include "UTWeap_Enforcer.h"
#include "UTWeap_ImpactHammer.h"
#include "UTWeap_Translocator.h"
#include "UTDemoRecSpectator.h"

UUTScoreboard::UUTScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1920.f;
	Position = FVector2D(0.f, 0.f);
	Size = FVector2D(1920.0f, 1080.0f);
	ScreenPosition = FVector2D(0.f, 0.f);
	Origin = FVector2D(0.f, 0.f);
	bScaleByDesignedResolution = false;

	ColumnHeaderPlayerX = 0.1f;
	ColumnHeaderScoreX = 0.63f;
	ColumnHeaderDeathsX = 0.79f;
	ColumnHeaderPingX = 0.95f;
	ColumnHeaderY = 6.f;
	ColumnY = 12.f;
	ColumnMedalX = 0.55f;
	CellHeight = 32.f;
	CellWidth = 530.f;
	EdgeWidth = 420.f;
	FlagX = 0.01f;
	MinimapCenter = FVector2D(0.75f, 0.5f);

	KillsColumn = 0.4f;
	DeathsColumn = 0.52f;
	ShotsColumn = 0.72f;
	AccuracyColumn = 0.84f;
	ValueColumn = 0.5f;
	ScoreColumn = 0.85f;
	bHighlightStatsLineTopValue = false;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_SpecSwitch01.A_UI_SpecSwitch01'"));
	ScoreUpdateSound = OtherSpreeSoundFinder.Object;

	GameMessageText = NSLOCTEXT("UTScoreboard", "ScoreboardHeader", "{GameName} in {MapName}");
	CH_PlayerName = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerName", "Player");
	CH_Score = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerScore", "Score");
	CH_Kills = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerKills", "K");
	CH_Deaths = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerDeaths", "D");
	CH_Skill = NSLOCTEXT("UTScoreboard", "ColumnHeader_BotSkill", "SKILL");
	CH_Ping = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerPing", "Ping");
	CH_Ready = NSLOCTEXT("UTScoreboard", "ColumnHeader_Ready", "");
	OneSpectatorWatchingText = NSLOCTEXT("UTScoreboard", "OneSpectator", "1 spectator is watching this match");
	SpectatorsWatchingText = NSLOCTEXT("UTScoreboard", "SpectatorFormat", "{0} spectators are watching this match");
	PingFormatText = NSLOCTEXT("UTScoreboard", "PingFormatText", "{0}ms");
	PositionFormatText = NSLOCTEXT("UTScoreboard", "PositionFormatText", "{0}.");
	TeamSwapText = NSLOCTEXT("UTScoreboard", "TEAMSWITCH", "TEAM SWAP");
	ReadyText = NSLOCTEXT("UTScoreboard", "READY", "READY");
	NotReadyText = NSLOCTEXT("UTScoreboard", "NOTREADY", "");
	WarmupText = NSLOCTEXT("UTScoreboard", "WARMUP", "WARMUP");
	WarmupWarningText = NSLOCTEXT("UTScoreboard", "WarmupWarningText", "If players are warming up, will wait for match to fill before starting.");
	MinimapToggleText = NSLOCTEXT("UTScoreboard", "MinimapToggle", "Press {key} to toggle Minimap");
	
	ReadyColor = FLinearColor::White;
	ReadyScale = 1.f;
	bDrawMinimapInScoreboard = true;
	bForceRealNames = false;
	MinimapPadding = 12.f;
}

void UUTScoreboard::PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter)
{
	Super::PreDraw(DeltaTime, InUTHUDOwner, InCanvas, InCanvasCenter);

	ActualPlayerCount=0;
	if (UTGameState)
	{
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			if (UTGameState->PlayerArray[i] && !UTGameState->PlayerArray[i]->bOnlySpectator)
			{
				ActualPlayerCount++;
			}
		}
	}

	RenderScale = Canvas->ClipX / DesignedResolution;
	RenderScale *= GetDrawScaleOverride();

	// Apply any scaling
	RenderSize.Y = Size.Y * RenderScale;
	if (Size.X > 0.f)
	{
		RenderSize.X = (bMaintainAspectRatio ? RenderSize.Y * AspectScale : RenderSize.X * RenderScale);
	}
	ColumnY = 12.f *RenderScale;
	ScaledEdgeSize = EdgeWidth*RenderScale;
	ScaledCellWidth = RenderScale * CellWidth;
	FooterPosY = 1032.f * RenderScale;
}

void UUTScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	bHaveWarmup = false;
	float YOffset = 8.f*RenderScale;
	DrawGamePanel(RenderDelta, YOffset);
	DrawTeamPanel(RenderDelta, YOffset);
	DrawScorePanel(RenderDelta, YOffset);
	if (ShouldDrawScoringStats())
	{
		DrawScoringStats(RenderDelta, YOffset);
	}
	else
	{
		DrawCurrentLifeStats(RenderDelta, YOffset);
	}

	DrawMinimap(RenderDelta);
	if (bHaveWarmup)
	{
		DrawText(WarmupWarningText, 16.f * RenderScale, 16.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	}
}

void UUTScoreboard::DrawMinimap(float RenderDelta)
{
	if (bDrawMinimapInScoreboard && UTGameState && UTHUDOwner && !UTHUDOwner->IsPendingKillPending())
	{
		bool bToggledMinimap = !UTGameState->HasMatchStarted() && (!UTPlayerOwner || !UTPlayerOwner->UTPlayerState || !UTPlayerOwner->UTPlayerState->bIsWarmingUp);
		const float MapSize = (UTGameState && UTGameState->bTeamGame) ? FMath::Min(Canvas->ClipX - 2.f*ScaledEdgeSize - 2.f*ScaledCellWidth - MinimapPadding*RenderScale, 0.9f*Canvas->ClipY - 120.f * RenderScale)
			: FMath::Min(0.5f*Canvas->ClipX, 0.9f*Canvas->ClipY - 120.f * RenderScale);
		if (!bToggledMinimap || !UTHUDOwner->bShowScores)
		{
			float MapYPos = FMath::Max(120.f*RenderScale, MinimapCenter.Y*Canvas->ClipY - 0.5f*MapSize);
			FVector2D LeftCorner = FVector2D(MinimapCenter.X*Canvas->ClipX - 0.5f*MapSize, MapYPos);
			DrawTexture(UTHUDOwner->ScoreboardAtlas, LeftCorner.X, LeftCorner.Y, MapSize, MapSize, 149, 138, 32, 32, 0.5f, FLinearColor::Black);
			UTHUDOwner->DrawMinimap(FColor(192, 192, 192, 220), MapSize, LeftCorner);
		}
		if (bToggledMinimap)
		{
			FFormatNamedArguments Args;
			Args.Add("key", UTHUDOwner->ShowScoresLabel);
			FText MinimapMessage = FText::Format(MinimapToggleText, Args);
			float YPos = (UTGameState && UTGameState->bTeamGame) ? MinimapCenter.Y*Canvas->ClipY + 0.51f*MapSize : MinimapCenter.Y*Canvas->ClipY + 0.49f*MapSize;
			DrawText(MinimapMessage, MinimapCenter.X*Canvas->ClipX - 0.5f*MapSize, YPos, UTHUDOwner->TinyFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		}
	}
}

void UUTScoreboard::DrawGamePanel(float RenderDelta, float& YOffset)
{
	float LeftEdge = 10.f * RenderScale;

	// Draw the Background
	DrawTexture(UTHUDOwner->ScoreboardAtlas,LeftEdge,YOffset, RenderSize.X - 2.f*LeftEdge, 72.f*RenderScale, 4.f*RenderScale,2,124, 128, 1.0);

	// Draw the Logo
	DrawTexture(UTHUDOwner->ScoreboardAtlas, LeftEdge + 80.f * RenderScale, YOffset + 36.f * RenderScale, 150.5f * RenderScale, 49.f * RenderScale, 162,14,301, 98.0, 1.f, FLinearColor::White, FVector2D(0.5f, 0.5f));
	
	// Draw the Spacer Bar
	DrawTexture(UTHUDOwner->ScoreboardAtlas, LeftEdge + 180.f * RenderScale, YOffset + 30.f * RenderScale, 4.f * RenderScale, 72.f * RenderScale, 488, 13, 4, 99, 1.0f, FLinearColor::White, FVector2D(0.0f, 0.5f));
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
	if (UTHUDOwner->ScoreMessageText.IsEmpty())
	{ 
		FFormatNamedArguments Args;
		Args.Add("GameName", FText::AsCultureInvariant(GameName));
		Args.Add("MapName", FText::AsCultureInvariant(MapName));
		FText GameMessage = FText::Format(GameMessageText, Args);
		DrawText(GameMessage, 220.f*RenderScale, YOffset + 36.f*RenderScale, UTHUDOwner->MediumFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center); 
	}
	else
	{
		UTHUDOwner->DrawWinConditions(UTHUDOwner->MediumFont, 220.f*RenderScale, YOffset + 4.f*RenderScale, Canvas->ClipX, RenderScale, false);
	}

	DrawGameOptions(RenderDelta, YOffset);
	YOffset += 72.f*RenderScale;	// The size of this zone.
}

void UUTScoreboard::DrawGameOptions(float RenderDelta, float& YOffset)
{
	if (UTGameState)
	{
		FText StatusText = UTGameState->GetGameStatusText(true);
		if (!StatusText.IsEmpty())
		{
			DrawText(StatusText, Canvas->ClipX - 100.f*RenderScale, YOffset + 48.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		}
		else if (UTGameState->GoalScore > 0)
		{
			// Draw Game Text
			FText Score = FText::Format(UTGameState->GoalScoreText, FText::AsNumber(UTGameState->GoalScore));
			DrawText(Score, Canvas->ClipX - 100.f*RenderScale, YOffset + 48.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		}

		float DisplayedTime = UTGameState ? UTGameState->GetClockTime() : 0.f;
		FText Timer = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), DisplayedTime, false, true, true);
		DrawText(Timer, Canvas->ClipX - 100.f*RenderScale, YOffset + 22.f*RenderScale, UTHUDOwner->NumberFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
	}
}

void UUTScoreboard::DrawTeamPanel(float RenderDelta, float& YOffset)
{
	YOffset += 39.f*RenderScale; // A small gap
}

void UUTScoreboard::DrawScorePanel(float RenderDelta, float& YOffset)
{
	if (bIsInteractive)
	{
		SelectionStack.Empty();
	}
	LastScorePanelYOffset = YOffset;
	if (UTGameState)
	{
		DrawScoreHeaders(RenderDelta, YOffset);
		DrawPlayerScores(RenderDelta, YOffset);
	}
}

void UUTScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float Height = 23.f * RenderScale;
	int32 ColumnCnt = ((UTGameState && UTGameState->bTeamGame) || ActualPlayerCount > 16) ? 2 : 1;
	float ColumnHeaderAdjustY = ColumnHeaderY * RenderScale;
	float XOffset = ScaledEdgeSize;
	for (int32 i = 0; i < ColumnCnt; i++)
	{
		// Draw the background Border
		DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YOffset, ScaledCellWidth, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));
		DrawText(CH_PlayerName, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnHeaderAdjustY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);

		if (UTGameState && UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + (ScaledCellWidth * ColumnHeaderScoreX), YOffset + ColumnHeaderAdjustY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Deaths, XOffset + (ScaledCellWidth * ColumnHeaderDeathsX), YOffset + ColumnHeaderAdjustY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawText(CH_Ready, XOffset + (ScaledCellWidth * ColumnHeaderScoreX), YOffset + ColumnHeaderAdjustY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		DrawText((GetWorld()->GetNetMode() == NM_Standalone) ? CH_Skill : CH_Ping, XOffset + (ScaledCellWidth * ColumnHeaderPingX), YOffset + ColumnHeaderAdjustY, UTHUDOwner->TinyFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		XOffset = Canvas->ClipX - ScaledCellWidth - ScaledEdgeSize;
	}

	YOffset += Height + 4.f * RenderScale;
}

void UUTScoreboard::DrawPlayerScores(float RenderDelta, float& YOffset)
{
	if (!UTGameState)
	{
		return;
	}

	int32 Place = 1;
	int32 NumSpectators = 0;
	int32 ColumnCnt = ((UTGameState && UTGameState->bTeamGame) || ActualPlayerCount > 16) ? 2 : 1;
	float XOffset = ScaledEdgeSize;
	float DrawOffset = YOffset;
	for (int32 i=0; i<UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PlayerState)
		{
			if (!PlayerState->bOnlySpectator)
			{
				DrawPlayer(Place, PlayerState, RenderDelta, XOffset, DrawOffset);
				DrawOffset += CellHeight*RenderScale;
				Place++;
				if (Place == 17)
				{
					XOffset = Canvas->ClipX - ScaledCellWidth - ScaledEdgeSize;
					DrawOffset = YOffset;
				}
			}
			else if (Cast<AUTDemoRecSpectator>(UTPlayerOwner) == nullptr && !PlayerState->bIsDemoRecording)
			{
				NumSpectators++;
			}
		}
	}
	
	if (UTGameState->PlayerArray.Num() <= 28 && NumSpectators > 0)
	{
		FText SpectatorCount = (NumSpectators == 1) 
			? OneSpectatorWatchingText
			: FText::Format(SpectatorsWatchingText, FText::AsNumber(NumSpectators));
		DrawText(SpectatorCount, 635.f*RenderScale, 765.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.0f, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f), ETextHorzPos::Center, ETextVertPos::Bottom);
	}
}

FLinearColor UUTScoreboard::GetPlayerColorFor(AUTPlayerState* InPS) const
{
	return FLinearColor::White;
}

FLinearColor UUTScoreboard::GetPlayerBackgroundColorFor(AUTPlayerState* InPS) const
{
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == InPS)
	{
		return FLinearColor::Gray;
	}
	else
	{
		return FLinearColor::Black;
	}
}

FLinearColor UUTScoreboard::GetPlayerHighlightColorFor(AUTPlayerState* InPS) const
{
	//RedTeam
	if (InPS->GetTeamNum() == 0)
	{
		return FLinearColor(0.5f, 0.f, 0.f, 1.f);
	}
	else // Blue, or unknown team
	{
		return FLinearColor(0.f, 0.f, 0.5f, 1.f);
	}
}

void UUTScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;	

	float BarOpacity = 0.3f;
	bool bIsUnderCursor = false;

	// If we are interactive, store off the bounds of this cell for selection
	if (bIsInteractive)
	{
		FVector4 Bounds = FVector4(RenderPosition.X + XOffset, RenderPosition.Y + YOffset, 
										RenderPosition.X + XOffset + ScaledCellWidth, RenderPosition.Y + YOffset + CellHeight*RenderScale);
		SelectionStack.Add(FSelectionObject(PlayerState, Bounds));
		bIsUnderCursor = (CursorPosition.X >= Bounds.X && CursorPosition.X <= Bounds.Z && CursorPosition.Y >= Bounds.Y && CursorPosition.Y <= Bounds.W);
	}
	PlayerState->ScoreCorner = FVector(RenderPosition.X + XOffset, RenderPosition.Y + YOffset + 0.25f*CellHeight*RenderScale, 0.f);
	if (!PlayerState->Team || (PlayerState->Team->TeamIndex != 1))
	{
		PlayerState->ScoreCorner.X += ScaledCellWidth;
	}

	float NameXL, NameYL;
	float MaxNameWidth = 0.42f*ScaledCellWidth - (PlayerState->bIsFriend ? 30.f*RenderScale : 0.f);
	Canvas->TextSize(UTHUDOwner->SmallFont, PlayerState->PlayerName, NameXL, NameYL, 1.f, 1.f);
	UFont* NameFont = UTHUDOwner->SmallFont;
	FLinearColor DrawColor = GetPlayerColorFor(PlayerState);

	int32 Ping = PlayerState->Ping * 4;
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == PlayerState)
	{
		Ping = PlayerState->ExactPing;
		BarOpacity = 0.5f;
	}
	
	// Draw the background border.
	FLinearColor BarColor = GetPlayerBackgroundColorFor(PlayerState);
	float FinalBarOpacity = BarOpacity;
	if (bIsUnderCursor) 
	{
		BarColor = FLinearColor(0.0,0.3,0.0,1.0);
		FinalBarOpacity = 0.75f;
	}
	if (PlayerState == SelectedPlayer) 
	{
		BarColor = FLinearColor(0.0, 0.3, 0.3, 1.0);
		FinalBarOpacity = 0.75f;
	}

	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YOffset, ScaledCellWidth, 0.9f*CellHeight*RenderScale, 149, 138, 32, 32, FinalBarOpacity, BarColor);	// NOTE: Once I make these interactable.. have a selection color too

	if (PlayerState->KickCount > 0)
	{
		float NumPlayers = 0.0f;

		for (int32 i=0; i < UTGameState->PlayerArray.Num(); i++)
		{
			if (!UTGameState->PlayerArray[i]->bIsSpectator && !UTGameState->PlayerArray[i]->bOnlySpectator)		
			{
				if (!UTGameState->bOnlyTeamCanVoteKick || UTGameState->OnSameTeam(PlayerState,UTGameState->PlayerArray[i]) )
				{
					NumPlayers += 1.0f;
				}
			}
		}

		if (NumPlayers > 0.0f)
		{
			float KickPercent = float(PlayerState->KickCount) / NumPlayers;
			float XL, SmallYL;
			Canvas->TextSize(UTHUDOwner->SmallFont, "Kick", XL, SmallYL, RenderScale, RenderScale);
			DrawText(NSLOCTEXT("UTScoreboard", "Kick", "Kick"), XOffset + (ScaledCellWidth * FlagX), YOffset + ColumnY - 0.27f*SmallYL, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
			FText Kick = FText::Format(NSLOCTEXT("Common", "PercFormat", "{0}%"), FText::AsNumber(int32(KickPercent * 100.0)));
			DrawText(Kick, XOffset + (ScaledCellWidth * FlagX), YOffset + ColumnY + 0.33f*SmallYL, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
		}
	}
	else
	{
		FTextureUVs FlagUV;
		UTexture2D* NewFlagAtlas = UTHUDOwner->ResolveFlag(PlayerState, FlagUV);
		DrawTexture(NewFlagAtlas, XOffset + (ScaledCellWidth * FlagX), YOffset + 14.f*RenderScale, FlagUV.UL*RenderScale, FlagUV.VL*RenderScale, FlagUV.U, FlagUV.V, 36, 26, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));
	}

	FVector2D NameSize;

	TSharedRef<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(*PlayerState->StatsID));
	FText EpicAccountName = UTGameState->GetEpicAccountNameForAccount(UserId);
	float NameScaling = FMath::Min(RenderScale, MaxNameWidth / FMath::Max(NameXL, 1.f));
	if (bForceRealNames && !EpicAccountName.IsEmpty())
	{
		NameSize = DrawText(EpicAccountName, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnY, NameFont, false, FVector2D(0.f, 0.f), FLinearColor::Black, true, GetPlayerHighlightColorFor(PlayerState), NameScaling, 1.0f, DrawColor, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), ETextHorzPos::Left, ETextVertPos::Center);
	}
	else
	{
		const bool bNameMatchesAccount = (EpicAccountName.ToString() == PlayerState->PlayerName);
		if (bNameMatchesAccount)
		{
			NameSize = DrawText(FText::FromString(PlayerState->PlayerName), XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnY, NameFont, false, FVector2D(0.f, 0.f), FLinearColor::Black, true, GetPlayerHighlightColorFor(PlayerState), NameScaling, 1.0f, DrawColor, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f), ETextHorzPos::Left, ETextVertPos::Center);
		}
		else
		{
			NameSize = DrawText(FText::FromString(PlayerState->PlayerName), XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnY, NameFont, NameScaling, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
		}
	}

	if (PlayerState->bIsFriend)
	{
		DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX) + NameSize.X*NameScaling + 5.f*RenderScale, YOffset + 18.f*RenderScale, 30.f*RenderScale, 24.f*RenderScale, 236, 136, 30, 24, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));
	}
	if (UTGameState && UTGameState->HasMatchStarted())
	{
		if (PlayerState->bPendingTeamSwitch)
		{
			DrawText(TeamSwapText, XOffset + (ScaledCellWidth * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->SmallFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawPlayerScore(PlayerState, XOffset, YOffset, ScaledCellWidth, DrawColor);
		}
	}
	else
	{
		DrawReadyText(PlayerState, XOffset, YOffset, ScaledCellWidth);
	}

	FText PingText;
	if (GetWorld()->GetNetMode() == NM_Standalone)
	{
		AUTBot* Bot = Cast<AUTBot>(PlayerState->GetOwner());
		PingText = Bot ? FText::AsNumber(Bot->Skill) : FText::FromString(TEXT("-"));
	}
	else
	{
		PingText = FText::Format(PingFormatText, FText::AsNumber(Ping));
	}
	DrawText(PingText, XOffset + 0.995f*ScaledCellWidth, YOffset + ColumnY, UTHUDOwner->TinyFont, 0.75f*RenderScale, 1.f, DrawColor, ETextHorzPos::Right, ETextVertPos::Center);

	// Strike out players that are out of lives
	if (PlayerState->bOutOfLives)
	{
		float Height = 8.0f;
		float XL, YL;
		Canvas->TextSize(UTHUDOwner->SmallFont, PlayerState->PlayerName, XL, YL, RenderScale, RenderScale);
		float StrikeWidth = FMath::Min(0.475f*ScaledCellWidth, XL);
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnY, StrikeWidth, Height, 185.f, 400.f, 4.f, 4.f, 1.0f, FLinearColor::Red);
	}

	// Draw the talking indicator
	if ( PlayerState->bIsTalking )
	{
		bool bLeft = (XOffset < Canvas->ClipX * 0.5f);
		float TalkingXOffset = bLeft ? ScaledCellWidth + (10.0f *RenderScale) : (-36.0f * RenderScale);
		FTextureUVs ChatIconUVs =  bLeft ? FTextureUVs(497.0f, 965.0f, 35.0f, 31.0f) : FTextureUVs(532.0f, 965.0f, -35.0f, 31.0f);
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset + TalkingXOffset, YOffset + ( (CellHeight * 0.5f - 24.0f) * RenderScale), (26 * RenderScale),(23 * RenderScale), ChatIconUVs.U, ChatIconUVs.V, ChatIconUVs.UL, ChatIconUVs.VL, 1.0f);
	}
}

void UUTScoreboard::DrawReadyText(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width)
{
	FText PlayerReady = NotReadyText;
	if (PlayerState->bReadyToPlay)
	{
		PlayerReady = PlayerState->bIsWarmingUp ? WarmupText : ReadyText;
		bHaveWarmup = bHaveWarmup || PlayerState->bIsWarmingUp;
	}
		
	float ReadyX = XOffset + ScaledCellWidth * ColumnHeaderScoreX;
	if (PlayerState && PlayerState->bPendingTeamSwitch)
	{
		PlayerReady = TeamSwapText;
	}
	ReadyColor = FLinearColor::White;
	ReadyScale = 1.f;
	if (PlayerState && (PlayerState->ReadyMode > 0) && PlayerState->bReadyToPlay)
	{
		int32 ReadyColorState = 2.f * GetWorld()->GetTimeSeconds() + PlayerState->PlayerId;
		if ((ReadyColorState & 14) == 0)
		{
			ReadyColorState += 2;
		}
		ReadyColor.R = (ReadyColorState & 2) ? 1.f : 0.f;
		ReadyColor.G = (ReadyColorState & 4) ? 1.f : 0.f;
		ReadyColor.B = (ReadyColorState & 8) ? 1.f : 0.f;
		float Speed = (PlayerState->ReadyMode == 4) ? 1.f : 2.f;
		float ScaleTime = Speed*GetWorld()->GetTimeSeconds() - int32(Speed*GetWorld()->GetTimeSeconds());
		float Scaling = (ScaleTime < 0.5f)
			? ScaleTime
			: 1.f - ScaleTime;
		if (PlayerState->ReadyMode == 4)
		{
			if (PlayerState->PlayerId % 2 == 0)
			{
				Scaling = 0.5f - Scaling;
			}
			ReadyScale = 1.15f;
			float Dist = -0.35f * ScaledCellWidth;
			ReadyX = ReadyX + Dist * Scaling + 0.1f*ScaledCellWidth;
			if (ScaleTime < 0.5f)
			{
				ReadyColor.B = 0.5f;
				ReadyColor.G = 0.5f;
			}
		}
		else
		{
			if (PlayerState->PlayerId % 2 == 0)
			{
				Scaling = 1.f - Scaling;
			}
			if ((PlayerState->ReadyMode == 2) || (PlayerState->ReadyMode == 3) || (PlayerState->ReadyMode == 5))
			{
				ReadyScale = Scaling * 1.2f + 0.7f;
			}
			if (PlayerState->ReadyMode == 3)
			{
				PlayerReady = NSLOCTEXT("UTScoreboard", "Plead", "COME ON!");
			}
			else if (PlayerState->ReadyMode == 5)
			{
				PlayerReady = NSLOCTEXT("UTScoreboard", "PleadRekt", "GET REKT!");
			}
		}
	}
	DrawText(PlayerReady, ReadyX, YOffset + ColumnY, UTHUDOwner->SmallFont, ReadyScale * RenderScale, 1.0f, ReadyColor, ETextHorzPos::Center, ETextVertPos::Center);
}

void UUTScoreboard::DrawPlayerScore(AUTPlayerState* PlayerState, float XOffset, float YOffset, float Width, FLinearColor DrawColor)
{
	DrawText(FText::AsNumber(int32(PlayerState->Score)), XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->SmallFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText(FText::AsNumber(PlayerState->Deaths), XOffset + (Width * ColumnHeaderDeathsX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
}

int32 UUTScoreboard::SelectionHitTest(FVector2D InPosition)
{
	if (bIsInteractive)
	{
		for (int32 i = 0; i < SelectionStack.Num(); i++)
		{
			if (InPosition.X >= SelectionStack[i].ScoreBounds.X && InPosition.X <= SelectionStack[i].ScoreBounds.Z &&
				InPosition.Y >= SelectionStack[i].ScoreBounds.Y && InPosition.Y <= SelectionStack[i].ScoreBounds.W && SelectionStack[i].ScoreOwner.IsValid())
			{
				return i;
			}
		}
	}
	return -1;
}

void UUTScoreboard::TrackMouseMovement(FVector2D NewMousePosition)
{
	if (bIsInteractive)
	{
		CursorPosition = NewMousePosition;
	}
}

bool UUTScoreboard::AttemptSelection(FVector2D SelectionPosition)
{
	if (bIsInteractive)
	{
		int32 SelectionIndex = SelectionHitTest(SelectionPosition);
		if (SelectionIndex >=0 && SelectionIndex < SelectionStack.Num())
		{
			SelectedPlayer = SelectionStack[SelectionIndex].ScoreOwner;
			return true;
		}
	}
	return false;
}

void UUTScoreboard::ClearSelection()
{
	SelectedPlayer.Reset();
}

void UUTScoreboard::BecomeInteractive()
{
	bIsInteractive = true;
}

void UUTScoreboard::BecomeNonInteractive()
{
	bIsInteractive = false;
	ClearSelection();
}

void UUTScoreboard::DefaultSelection(AUTGameState* GS, uint8 TeamIndex)
{
	if (GS != NULL)
	{
		for (int32 i=0; i < GS->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
			if (PS && !PS->bIsSpectator && !PS->bOnlySpectator && (TeamIndex == 255 || PS->GetTeamNum() == TeamIndex))
			{
				SelectedPlayer = PS;
				return;
			}
		}
	}
	SelectedPlayer.Reset();
}

void UUTScoreboard::SelectNext(int32 Offset, bool bDoNoWrap)
{
	AUTGameState* GS = UTHUDOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL) return;

	GS->SortPRIArray();
	int32 SelectedIndex = GS->PlayerArray.Find(SelectedPlayer.Get());
	
	if (SelectedIndex >= 0 && SelectedIndex < GS->PlayerArray.Num())
	{
		AUTPlayerState* Next = NULL;
		int32 Step = Offset > 0 ? 1 : -1;
		do 
		{
			SelectedIndex += Step;
			if (SelectedIndex < 0) 
			{
				if (bDoNoWrap) return;
				SelectedIndex = GS->PlayerArray.Num() -1;
			}
			if (SelectedIndex >= GS->PlayerArray.Num()) 
			{
				if (bDoNoWrap) return;
				SelectedIndex = 0;
			}

			Next = Cast<AUTPlayerState>(GS->PlayerArray[SelectedIndex]);
			if (Next && !Next->bOnlySpectator && !Next->bIsSpectator)
			{
				// Valid potential player.
				Offset -= Step;
				if (Offset == 0)
				{
					SelectedPlayer = Next;
					return;
				}
			}

		} while (Next != SelectedPlayer.Get());
	}
	else
	{
		DefaultSelection(GS);
	}

}

void UUTScoreboard::SelectionUp()
{
	SelectNext(-1);
}

void UUTScoreboard::SelectionDown()
{
	SelectNext(1);
}

void UUTScoreboard::SelectionLeft()
{
	SelectNext(-16,true);
}

void UUTScoreboard::SelectionRight()
{
	SelectNext(16,true);
}

void UUTScoreboard::SelectionClick()
{
	if (SelectedPlayer.IsValid())
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(UTHUDOwner->UTPlayerOwner->Player);
		if (LP)
		{
			LP->ShowPlayerInfo(SelectedPlayer);
			ClearSelection();
		}
	}
}

void UUTScoreboard::DrawStatsLine(FText StatsName, int32 StatValue, int32 ScoreValue, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth)
{
	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(StatsFontInfo.TextFont, StatsName, XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);

	if (StatValue >= 0)
	{
		Canvas->SetLinearDrawColor((bHighlightStatsLineTopValue && (StatValue > ScoreValue)) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, FString::Printf(TEXT(" %i"), StatValue), XOffset + ValueColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	if (ScoreValue >= 0)
	{
		Canvas->SetLinearDrawColor((bHighlightStatsLineTopValue && (ScoreValue > StatValue)) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, FString::Printf(TEXT(" %i"), ScoreValue), XOffset + ScoreColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	YPos += StatsFontInfo.TextHeight;
}

void UUTScoreboard::DrawPlayerStatsLine(FText StatsName, AUTPlayerState* FirstPS, AUTPlayerState* SecondPS, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, int32 HighlightIndex)
{
	if ((HighlightIndex == 0) && UTHUDOwner && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && !UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{
		if (FirstPS == UTHUDOwner->UTPlayerOwner->UTPlayerState)
		{
			HighlightIndex = 1;
		}
		else if (SecondPS == UTHUDOwner->UTPlayerOwner->UTPlayerState)
		{
			HighlightIndex = 2;
		}
	}
	DrawTextStatsLine(StatsName, GetPlayerNameFor(FirstPS), GetPlayerNameFor(SecondPS), DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, HighlightIndex);
}

void UUTScoreboard::DrawTextStatsLine(FText StatsName, FString StatValue, FString ScoreValue, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, int32 HighlightIndex)
{
	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(StatsFontInfo.TextFont, StatsName, XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	if (!StatValue.IsEmpty())
	{
		float XL, YL;
		Canvas->StrLen(StatsFontInfo.TextFont, StatValue, XL, YL);
		float NameScale = FMath::Clamp(0.96f*ScoreWidth * (ScoreColumn - ValueColumn)/ FMath::Max(XL, 1.f), 0.5f, 1.f);
		Canvas->SetLinearDrawColor((HighlightIndex & 1) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, StatValue, XOffset + ValueColumn*ScoreWidth, YPos, NameScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	if (!ScoreValue.IsEmpty())
	{
		float XL, YL;
		Canvas->StrLen(StatsFontInfo.TextFont, ScoreValue, XL, YL);
		float NameScale = FMath::Clamp(0.96f*ScoreWidth * (ScoreColumn - ValueColumn) / FMath::Max(XL, 1.f), 0.5f, 1.f);
		Canvas->SetLinearDrawColor((HighlightIndex & 2) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, ScoreValue, XOffset + ScoreColumn*ScoreWidth, YPos, NameScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	YPos += StatsFontInfo.TextHeight;
}

void UUTScoreboard::DrawCurrentLifeStats(float DeltaTime, float& YPos)
{
	return;

	FVector2D SavedRenderPosition = RenderPosition;
	RenderPosition = FVector2D(0.f, 0.f);
	float TopYPos = YPos;
	float ScoreWidth = 1.2f*ScaledCellWidth;
	float MaxHeight = FooterPosY + SavedRenderPosition.Y - YPos;
	float PageBottom = TopYPos + MaxHeight;

	// draw left side
	float XOffset = ScaledEdgeSize + ScaledCellWidth - ScoreWidth;
	FLinearColor PageColor = FLinearColor::Black;
	PageColor.A = 0.5f;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YPos, ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, PageColor);

	DrawText(NSLOCTEXT("UTScoreboard", "THISLIFE", "This Life"), XOffset, YPos, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	float TinyYL;
	Canvas->TextSize(UTHUDOwner->TinyFont, "TEST", XL, TinyYL, RenderScale, RenderScale);
	float MedYL;
	Canvas->TextSize(UTHUDOwner->MediumFont, "TEST", XL, MedYL, RenderScale, RenderScale);

	DrawText(NSLOCTEXT("UTScoreboard", "Kills", "Kills"), XOffset, YPos + MedYL, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
	DrawText( FText::AsNumber(UTHUDOwner->UTPlayerOwner->UTPlayerState->Kills), XOffset + 0.3f*ScoreWidth, YPos + MedYL, UTHUDOwner->MediumFont, RenderScale, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
}

void UUTScoreboard::DrawScoringStats(float DeltaTime, float& YPos)
{
	FVector2D SavedRenderPosition = RenderPosition;
	RenderPosition = FVector2D(0.f, 0.f);
	float TopYPos = YPos;
	float ScoreWidth = 1.15f*ScaledCellWidth;
	float MaxHeight = FooterPosY + SavedRenderPosition.Y - YPos;
	float PageBottom = TopYPos + MaxHeight;

	// draw left side
	float XOffset = ScaledEdgeSize + ScaledCellWidth - ScoreWidth;
	DrawStatsLeft(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);

	// draw right side
	XOffset = Canvas->ClipX - XOffset - ScoreWidth;
	YPos = TopYPos;
	DrawStatsRight(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);

	RenderPosition = SavedRenderPosition;
}

void UUTScoreboard::DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
}

void UUTScoreboard::DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
}

float UUTScoreboard::GetDrawScaleOverride()
{
	return 1.f;
}

TWeakObjectPtr<AUTPlayerState> UUTScoreboard::GetSelectedPlayer()
{
	return SelectedPlayer;
}

void UUTScoreboard::GetContextMenuItems_Implementation(TArray<FScoreboardContextMenuItem>& MenuItems)
{
}

bool UUTScoreboard::HandleContextCommand_Implementation(uint8 ContextId, AUTPlayerState* InSelectedPlayer)
{
	return false;
}
