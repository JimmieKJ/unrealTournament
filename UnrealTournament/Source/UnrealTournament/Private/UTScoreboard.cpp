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
	DesignedResolution = 1080;
	Position = FVector2D(0.f, 0.f);
	Size = FVector2D(1900.0f, 1080.0f);
	ScreenPosition = FVector2D(0.f, 0.f);
	Origin = FVector2D(0.f, 0.f);
	bScaleByDesignedResolution = false;

	NumPages = 2;
	ColumnHeaderPlayerX = 0.1f;
	ColumnHeaderScoreX = 0.52f;
	ColumnHeaderDeathsX = 0.68f;
	ColumnHeaderPingX = 0.93f;
	ColumnHeaderY = 6.f;
	ColumnY = 12.f;
	ColumnMedalX = 0.55f;
	CellHeight = 32.f;
	CenterBuffer = 420.f;
	FlagX = 0.01f;
	MinimapCenter = FVector2D(0.75f, 0.5f);

	KillsColumn = 0.4f;
	DeathsColumn = 0.52f;
	ShotsColumn = 0.72f;
	AccuracyColumn = 0.84f;
	ValueColumn = 0.5f;
	ScoreColumn = 0.75f;
	bHighlightStatsLineTopValue = false;
	BestWeaponIndex = -1;

	StatsPageTitles.Add(NSLOCTEXT("UTScoreboard", "ScoringBreakDownHeader", "{PlayerName} Scoring Breakdown"));
	StatsPageTitles.Add(NSLOCTEXT("UTScoreboard", "WeaponStatsHeader", "{PlayerName} Weapon Stats"));
	StatsPageTitles.Add(NSLOCTEXT("UTScoreboard", "RewardStatsHeader", "{PlayerName} Reward Stats"));
	StatsPageTitles.Add(NSLOCTEXT("UTScoreboard", "MovementStatsHeader", "{PlayerName} Movement Stats"));

	StatsPageFooters.Add(NSLOCTEXT("UTScoreboard", "ScoringBreakDownFooter", "Press Down Arrow to View Weapon Stats"));
	StatsPageFooters.Add(NSLOCTEXT("UTScoreboard", "WeaponStatsFooter", "Up Arrow to View Game Stats, Down Arrow to View Reward Stats"));
	StatsPageFooters.Add(NSLOCTEXT("UTScoreboard", "RewardStatsFooter", "Press Up Arrow to View Weapon Stats, Down Arrow to View Movement Stats"));
	StatsPageFooters.Add(NSLOCTEXT("UTScoreboard", "MovementStatsFooter", "Press Up Arrow to View Reward Stats"));

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
	ArrowKeysText = NSLOCTEXT("UTScoreboard", "Pages", "Arrow keys to switch page ({0} of {1})");
	ReadyColor = FLinearColor::White;
	ReadyScale = 1.f;
	bDrawMinimapInScoreboard = true;
}

void UUTScoreboard::AdvancePage(int32 Increment)
{
	UTHUDOwner->SetScoreboardPage(uint32(FMath::Clamp<int32>(int32(UTHUDOwner->GetScoreboardPage()) + Increment, 0, NumPages - 1)));
	BestWeaponIndex = -1;
	PageChanged();
}

void UUTScoreboard::OpenScoringPlaysPage()
{
}

AUTPlayerState* UUTScoreboard::GetNextScoringPlayer(int32 dir, int32& PSIndex)
{
	AUTPlayerState* ScoreBreakdownPS = UTHUDOwner->UTPlayerOwner->CurrentlyViewedScorePS;
	if (!ScoreBreakdownPS)
	{
		ScoreBreakdownPS = UTHUDOwner->GetScorerPlayerState();
	}
	TArray<AUTPlayerState*> PlayerArrayCopy;
	for (APlayerState* PS : UTGameState->PlayerArray)
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
		if (UTPS != NULL && !UTPS->bOnlySpectator)
		{
			PlayerArrayCopy.Add(UTPS);
		}
	}
	if (PlayerArrayCopy.Num() == 0)
	{
		return NULL;
	}
	PlayerArrayCopy.Sort([](const AUTPlayerState& A, const AUTPlayerState& B) -> bool
	{
		return A.SpectatingID < B.SpectatingID;
	});

	if (!ScoreBreakdownPS)
	{
		PSIndex = 0;
		return PlayerArrayCopy[0];
	}

	int32 CurrentIndex = 0;
	for (int32 i = 0; i < PlayerArrayCopy.Num(); i++)
	{
		if (PlayerArrayCopy[i] == ScoreBreakdownPS)
		{
			CurrentIndex = i;
			break;
		}
	}
	if (CurrentIndex + dir >= PlayerArrayCopy.Num())
	{
		CurrentIndex = -1;
	}
	else if (CurrentIndex + dir < 0)
	{
		CurrentIndex = PlayerArrayCopy.Num();
	}
	PSIndex = CurrentIndex + dir;
	return PlayerArrayCopy[PSIndex];
}

void UUTScoreboard::SetPage(int32 NewPage)
{
	if (UTHUDOwner)
	{
		UTHUDOwner->SetScoreboardPage(FMath::Clamp<int32>(NewPage, 0, NumPages - 1));
	}
	PageChanged();
}

void UUTScoreboard::PageChanged_Implementation()
{
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

	RenderScale = Canvas->ClipY / DesignedResolution;
	RenderScale *= GetDrawScaleOverride();

	// Apply any scaling
	RenderSize.Y = Size.Y * RenderScale;
	if (Size.X > 0.f)
	{
		RenderSize.X = (bMaintainAspectRatio ? RenderSize.Y * AspectScale : RenderSize.X * RenderScale);
	}
	ColumnY = 12.f *RenderScale;
	ScaledEdgeSize = 10.f*RenderScale;
	ScaledCellWidth = RenderScale * ((Size.X * 0.5f) - CenterBuffer);
	FooterPosY = 996.f * RenderScale;
}

void UUTScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	float YOffset = 8.f*RenderScale;
	DrawGamePanel(RenderDelta, YOffset);
	DrawTeamPanel(RenderDelta, YOffset);
	DrawScorePanel(RenderDelta, YOffset);
	if (UTHUDOwner->GetScoreboardPage() > 0)
	{
		DrawScoringStats(RenderDelta, YOffset);
	}
	DrawServerPanel(RenderDelta, FooterPosY);

	if ((UTHUDOwner->GetScoreboardPage() == 0) && UTGameState && UTGameState->IsMatchInProgress() && !UTGameState->IsMatchIntermission() && bDrawMinimapInScoreboard)
	{
		float MapScale = 0.65f;
		const float MapSize = float(Canvas->SizeY) * MapScale;
		FVector2D LeftCorner = FVector2D(MinimapCenter.X*Canvas->ClipX - 0.5f*MapSize, MinimapCenter.Y*Canvas->ClipY - 0.5f*MapSize);
		DrawTexture(UTHUDOwner->ScoreboardAtlas, LeftCorner.X, LeftCorner.Y, MapSize, MapSize, 149, 138, 32, 32, 0.5f, FLinearColor::Black);
		UTHUDOwner->DrawMinimap(FColor(192, 192, 192, 220), MapSize, LeftCorner);
	}
}

void UUTScoreboard::DrawGamePanel(float RenderDelta, float& YOffset)
{
	float LeftEdge = 10.f * RenderScale;

	// Draw the Background
	DrawTexture(UTHUDOwner->ScoreboardAtlas,LeftEdge,YOffset, RenderSize.X - 2.f*LeftEdge, 80.f*RenderScale, 4.f*RenderScale,2,124, 128, 1.0);

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
	if ((!UTGameState || (UTGameState->IsMatchInProgress() && !UTGameState->IsMatchIntermission())) || ScoreMessageText.IsEmpty())
	{ 
		FFormatNamedArguments Args;
		Args.Add("GameName", FText::AsCultureInvariant(GameName));
		Args.Add("MapName", FText::AsCultureInvariant(MapName));
		FText GameMessage = FText::Format(GameMessageText, Args);
		DrawText(GameMessage, 220.f*RenderScale, YOffset + 36.f*RenderScale, UTHUDOwner->MediumFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center); 
	}
	else
	{
		DrawText(ScoreMessageText, 220.f*RenderScale, YOffset + 36.f*RenderScale, UTHUDOwner->MediumFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center); 
	}

	DrawGameOptions(RenderDelta, YOffset);
	YOffset += 80.f*RenderScale;	// The size of this zone.
}

void UUTScoreboard::DrawGameOptions(float RenderDelta, float& YOffset)
{
	if (UTGameState)
	{
		FText StatusText = UTGameState->GetGameStatusText(true);
		if (!StatusText.IsEmpty())
		{
			DrawText(StatusText, Canvas->ClipX - 100.f*RenderScale, YOffset + 50.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		}
		else if (UTGameState->GoalScore > 0)
		{
			// Draw Game Text
			FText Score = FText::Format(UTGameState->GoalScoreText, FText::AsNumber(UTGameState->GoalScore));
			DrawText(Score, Canvas->ClipX - 100.f*RenderScale, YOffset + 50.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		}

		float DisplayedTime = UTGameState ? UTGameState->GetClockTime() : 0.f;
		FText Timer = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), DisplayedTime, false, true, true);
		DrawText(Timer, Canvas->ClipX - 100.f*RenderScale, YOffset + 20.f*RenderScale, UTHUDOwner->NumberFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
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
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == InPS)
	{
		return FLinearColor(0.0f, 0.92f, 1.0f, 1.0f);
	}
	else if (InPS->bIsFriend)
	{
		return FLinearColor(FColor(254, 255, 174, 255));
	}
	else
	{
		return FLinearColor::White;
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

	float NameXL, NameYL;
	float MaxNameWidth = 0.45f*ScaledCellWidth;
	Canvas->TextSize(UTHUDOwner->SmallFont, PlayerState->PlayerName, NameXL, NameYL, RenderScale, RenderScale);
	UFont* NameFont = (NameXL < MaxNameWidth) ? UTHUDOwner->SmallFont : UTHUDOwner->TinyFont;
	FText PlayerName = (NameXL < MaxNameWidth) ? FText::FromString(PlayerState->PlayerName) : FText::FromString(GetClampedName(PlayerState, UTHUDOwner->TinyFont, RenderScale, MaxNameWidth));
	FLinearColor DrawColor = GetPlayerColorFor(PlayerState);

	int32 Ping = PlayerState->Ping * 4;
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == PlayerState)
	{
		Ping = PlayerState->ExactPing;
		BarOpacity = 0.5f;
	}

	FText PlayerPing;
	if (GetWorld()->GetNetMode() == NM_Standalone)
	{
		AUTBot* Bot = Cast<AUTBot>(PlayerState->GetOwner());
		PlayerPing = Bot ? FText::AsNumber(Bot->Skill) : FText::FromString(TEXT("-"));
	}
	else
	{
		PlayerPing = FText::Format(PingFormatText, FText::AsNumber(Ping));
	}
	
	// Draw the background border.
	FLinearColor BarColor = FLinearColor::Black;
	float FinalBarOpacity = BarOpacity;
	if (bIsUnderCursor) 
	{
		BarColor = FLinearColor(0.0,0.3,0.0,1.0);
		FinalBarOpacity = 0.75f;
	}
	if (PlayerState == SelectedPlayer) 
	{
		BarColor = FLinearColor(0.0,0.3,0.3,1.0);
		FinalBarOpacity = 0.75f;
	}

	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset, YOffset, ScaledCellWidth, 0.9f*CellHeight*RenderScale, 149, 138, 32, 32, FinalBarOpacity, BarColor);	// NOTE: Once I make these interactable.. have a selection color too

	if (PlayerState->KickCount > 0)
	{
		float NumPlayers = 0.0f;

		for (int32 i=0; i < UTGameState->PlayerArray.Num(); i++)
		{
			if (!UTGameState->PlayerArray[i]->bIsSpectator)		
			{
				NumPlayers += 1.0f;
			}
		}

		if (NumPlayers > 0.0f)
		{
			float KickPercent = float(PlayerState->KickCount) / NumPlayers;
			float XL, SmallYL;
			Canvas->TextSize(UTHUDOwner->SmallFont, "Kick", XL, SmallYL, RenderScale, RenderScale);
			DrawText(NSLOCTEXT("UTScoreboard", "Kick", "Kick"), XOffset + (ScaledCellWidth * FlagX), YOffset + ColumnY - 0.27f*SmallYL, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
			FText Kick = FText::Format(NSLOCTEXT("Common", "PercFormat", "{0}%"), FText::AsNumber(KickPercent));
			DrawText(Kick, XOffset + (ScaledCellWidth * FlagX), YOffset + ColumnY + 0.33f*SmallYL, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
		}
	}
	else
	{
		FTextureUVs FlagUV;
		UTexture2D* NewFlagAtlas = UTHUDOwner->ResolveFlag(PlayerState, FlagUV);
		DrawTexture(NewFlagAtlas, XOffset + (ScaledCellWidth * FlagX), YOffset + 14.f*RenderScale, FlagUV.UL*RenderScale, FlagUV.VL*RenderScale, FlagUV.U, FlagUV.V, 36, 26, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));
	}

	// Draw the Text
	FVector2D NameSize = DrawText(PlayerName, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnY, NameFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (PlayerState->bIsFriend)
	{
		DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX) + (NameSize.X+5.f)*RenderScale, YOffset + 18.f*RenderScale, 30.f*RenderScale, 24.f*RenderScale, 236, 136, 30, 24, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));
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
	DrawText(PlayerPing, XOffset + (ScaledCellWidth * ColumnHeaderPingX), YOffset + ColumnY, UTHUDOwner->TinyFont, RenderScale, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);

	// Strike out players that are out of lives
	if (PlayerState->bOutOfLives)
	{
		float Height = 8.0f;
		float XL, YL;
		Canvas->TextSize(UTHUDOwner->SmallFont, PlayerState->PlayerName, XL, YL, RenderScale, RenderScale);
		float StrikeWidth = FMath::Min(0.475f*ScaledCellWidth, XL);
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + ColumnY, StrikeWidth, Height, 185.f, 400.f, 4.f, 4.f, 1.0f, FLinearColor::Red);

		// draw skull here
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset + (ScaledCellWidth * ColumnHeaderPlayerX), YOffset + 0.35f*CellHeight*RenderScale, 0.5f*CellHeight*RenderScale, 0.5f*CellHeight*RenderScale, 725, 0, 28, 36, 1.0, FLinearColor::White);
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
	FText PlayerReady = PlayerState->bReadyToPlay ? ReadyText : NotReadyText;
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

void UUTScoreboard::DrawServerPanel(float RenderDelta, float YOffset)
{
	if (UTGameState)
	{
		FText SpectatorMessage;
		bool bShortMessage = false;
		if (UTHUDOwner->SpectatorMessageWidget)
		{
			SpectatorMessage = UTHUDOwner->SpectatorMessageWidget->GetSpectatorMessageText(bShortMessage);
		}
		if (!SpectatorMessage.IsEmpty() && !bShortMessage && (UTGameState->PlayerArray.Num() < 26) && (UTHUDOwner->GetScoreboardPage() == 0))
		{
			// Only draw if there is room above spectator panel
			UTHUDOwner->SpectatorMessageWidget->PreDraw(RenderDelta, UTHUDOwner, Canvas, CanvasCenter);
			UTHUDOwner->SpectatorMessageWidget->DrawSimpleMessage(SpectatorMessage, RenderDelta, bShortMessage);
			return;
		}
		if (SpectatorMessage.IsEmpty() || bShortMessage)
		{
			SpectatorMessage = FText::FromString(UTGameState->ServerName);
		}

		// Draw the Background
		DrawTexture(UTHUDOwner->ScoreboardAtlas, ScaledEdgeSize, YOffset, Canvas->ClipX - 2.f*ScaledEdgeSize, 38.f*RenderScale, 4, 132, 30, 38, 1.0);
		DrawText(SpectatorMessage, ScaledEdgeSize + 10.f*RenderScale, YOffset + 13.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawText(FText::FromString(UTGameState->ServerDescription), Canvas->ClipX - 200.f*RenderScale, YOffset + 13.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
		if ((NumPages > 1) && UTGameState->HasMatchStarted() && !bIsInteractive)
		{
			FText PageText = FText::Format(ArrowKeysText, FText::AsNumber(UTHUDOwner->GetScoreboardPage() + 1), FText::AsNumber(NumPages));
			DrawText(PageText, Canvas->ClipX * 0.5f, YOffset + 13.f*RenderScale, UTHUDOwner->SmallFont, RenderScale, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
		}
	}
}

int32 UUTScoreboard::SelectionHitTest(FVector2D Position)
{
	if (bIsInteractive)
	{
		for (int32 i = 0; i < SelectionStack.Num(); i++)
		{
			if (Position.X >= SelectionStack[i].ScoreBounds.X && Position.X <= SelectionStack[i].ScoreBounds.Z &&
				  Position.Y >= SelectionStack[i].ScoreBounds.Y && Position.Y <= SelectionStack[i].ScoreBounds.W && SelectionStack[i].ScoreOwner.IsValid())
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
		Canvas->SetLinearDrawColor((HighlightIndex & 1) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, StatValue, XOffset + ValueColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	if (!ScoreValue.IsEmpty())
	{
		Canvas->SetLinearDrawColor((HighlightIndex & 2) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, ScoreValue, XOffset + ScoreColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	YPos += StatsFontInfo.TextHeight;
}

void UUTScoreboard::DrawWeaponStatsLine(FText StatsName, int32 StatValue, int32 ScoreValue, int32 Shots, float Accuracy, float DeltaTime, float XOffset, float& YPos, const FStatsFontInfo& StatsFontInfo, float ScoreWidth, bool bIsBestWeapon)
{
	Canvas->SetLinearDrawColor(bIsBestWeapon ? FLinearColor::Yellow : FLinearColor::White);
	Canvas->DrawText(StatsFontInfo.TextFont, StatsName, XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);

	if (StatValue >= 0)
	{
		Canvas->SetLinearDrawColor((StatValue >= 15) ? FLinearColor::Yellow : FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, FString::Printf(TEXT(" %i"), StatValue), XOffset + KillsColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	if (ScoreValue >= 0)
	{
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, FString::Printf(TEXT(" %i"), ScoreValue), XOffset + DeathsColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	if (Shots >= 0)
	{
		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, FString::Printf(TEXT(" %i"), Shots), XOffset + ShotsColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);

		Canvas->SetLinearDrawColor(FLinearColor::White);
		Canvas->DrawText(StatsFontInfo.TextFont, FString::Printf(TEXT(" %3.1f%%"), Accuracy), XOffset + AccuracyColumn*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	}
	YPos += StatsFontInfo.TextHeight;
}

void UUTScoreboard::DrawWeaponStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float MaxHeight, const FStatsFontInfo& StatsFontInfo)
{
	Canvas->SetLinearDrawColor(FLinearColor::White);
	Canvas->DrawText(UTHUDOwner->TinyFont, "Kills with", XOffset + (KillsColumn-0.05f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	Canvas->DrawText(UTHUDOwner->TinyFont, "Deaths by", XOffset + (DeathsColumn - 0.05f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	Canvas->DrawText(UTHUDOwner->TinyFont, "Shots", XOffset + (ShotsColumn - 0.02f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	Canvas->DrawText(UTHUDOwner->TinyFont, "Accuracy", XOffset + (AccuracyColumn - 0.03f)*ScoreWidth, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;

	/** List of weapons to display stats for. */
	if (StatsWeapons.Num() == 0)
	{
		// add default weapons - needs to be automated
		StatsWeapons.AddUnique(AUTWeap_ImpactHammer::StaticClass()->GetDefaultObject<AUTWeapon>());
		StatsWeapons.AddUnique(AUTWeap_Enforcer::StaticClass()->GetDefaultObject<AUTWeapon>());

		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTPickupWeapon* Pickup = Cast<AUTPickupWeapon>(*It);
			if (Pickup && Pickup->GetInventoryType())
			{
				StatsWeapons.AddUnique(Pickup->GetInventoryType()->GetDefaultObject<AUTWeapon>());
			}
		}

		StatsWeapons.AddUnique(AUTWeap_Translocator::StaticClass()->GetDefaultObject<AUTWeapon>());
	}

	float BestWeaponKills = (BestWeaponIndex == FMath::Clamp(BestWeaponIndex, 0, StatsWeapons.Num() - 1)) ? StatsWeapons[BestWeaponIndex]->GetWeaponKillStats(PS) : 0;
	for (int32 i = 0; i < StatsWeapons.Num(); i++)
	{
		int32 Kills = StatsWeapons[i]->GetWeaponKillStats(PS);
		float Shots = StatsWeapons[i]->GetWeaponShotsStats(PS);
		float Accuracy = (Shots > 0) ? 100.f * StatsWeapons[i]->GetWeaponHitsStats(PS)/ Shots : 0.f;
		DrawWeaponStatsLine(StatsWeapons[i]->DisplayName, Kills, StatsWeapons[i]->GetWeaponDeathStats(PS), Shots, Accuracy, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, (i == BestWeaponIndex));
		if (Kills > BestWeaponKills)
		{
			BestWeaponKills = Kills;
			BestWeaponIndex = i;
		}
	}
	YPos -= 0.2f*StatsFontInfo.TextHeight;

	Canvas->DrawText(StatsFontInfo.TextFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += 0.7f*StatsFontInfo.TextHeight;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "ShockComboKills", "Shock Combo Kills"), PS->GetStatsValue(NAME_ShockComboKills), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "AmazingCombos", "Amazing Combos"), PS->GetStatsValue(NAME_AmazingCombos), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "HeadShots", "Sniper Headshots"), PS->GetStatsValue(NAME_SniperHeadshotKills), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "AirRox", "Air Rocket Kills"), PS->GetStatsValue(NAME_AirRox), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "FlakShreds", "Flak Shreds"), PS->GetStatsValue(NAME_FlakShreds), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "AirSnot", "Air Snot Kills"), PS->GetStatsValue(NAME_AirSnot), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	float BestComboRating = PS->GetStatsValue(NAME_BestShockCombo);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "ShockComboRating", "Best Shock Combo Rating"), FString::Printf(TEXT(" %4.1f"), BestComboRating), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, (BestComboRating > 8.f));
}

void UUTScoreboard::DrawScoringStats(float DeltaTime, float& YPos)
{
	FVector2D SavedRenderPosition = RenderPosition;
	RenderPosition = FVector2D(0.f, 0.f);
	float TopYPos = YPos;

	// draw left side
	float XOffset = ScaledEdgeSize;
	float ScoreWidth = ScaledCellWidth;
	float MaxHeight = FooterPosY + SavedRenderPosition.Y - YPos;
	float PageBottom = TopYPos + MaxHeight;

	FLinearColor PageColor = FLinearColor::Black;
	PageColor.A = 0.5f;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset - 0.05f*ScoreWidth, YPos, 1.1f*ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, PageColor);
	DrawStatsLeft(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);

	// draw right side
	XOffset = Canvas->ClipX - ScoreWidth - ScaledEdgeSize;
	YPos = TopYPos;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, XOffset - 0.05f*ScoreWidth, YPos, 1.1f*ScoreWidth, MaxHeight, 149, 138, 32, 32, 0.5f, PageColor);
	DrawStatsRight(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);

	RenderPosition = SavedRenderPosition;
}

void UUTScoreboard::DrawStatsLeft(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
}

void UUTScoreboard::DrawStatsRight(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
//	DrawScoreBreakdown(DeltaTime, YPos, XOffset, ScoreWidth, PageBottom);
}

void UUTScoreboard::DrawScoreBreakdown(float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom)
{
	Canvas->SetLinearDrawColor(FLinearColor::White);
	FStatsFontInfo StatsFontInfo;
	StatsFontInfo.TextRenderInfo.bEnableShadow = true;
	StatsFontInfo.TextRenderInfo.bClipText = true;
	StatsFontInfo.TextFont = UTHUDOwner->TinyFont;
	bHighlightStatsLineTopValue = false;

	if (!UTPlayerOwner->CurrentlyViewedScorePS)
	{
		UTPlayerOwner->SetViewedScorePS(UTHUDOwner->GetScorerPlayerState(), UTPlayerOwner->CurrentlyViewedStatsTab);
	}
	AUTPlayerState* PS = UTPlayerOwner->CurrentlyViewedScorePS;
	if (!PS)
	{
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("PlayerName"), FText::FromString(PS->PlayerName));

	int32 StatsPageIndex = (UTHUDOwner && UTHUDOwner->UTPlayerOwner) ? UTHUDOwner->UTPlayerOwner->CurrentlyViewedStatsTab : 0;

	FText CombinedHeader = (StatsPageIndex < StatsPageTitles.Num())
		? FText::Format(StatsPageTitles[StatsPageIndex], Args)
		: FText::Format(NSLOCTEXT("UTScoreboard", "GenericStatsHeader", "{PlayerName} Stats"), Args);
	float XL, SmallYL;
	Canvas->TextSize(UTHUDOwner->TinyFont, "TEST", XL, SmallYL, RenderScale, RenderScale);
	StatsFontInfo.TextHeight = SmallYL;
	float MedYL;
	Canvas->TextSize(UTHUDOwner->SmallFont, CombinedHeader.ToString(), XL, MedYL, RenderScale, RenderScale);

	if (PS->Team)
	{
		// draw team icon
		float IconHeight = MedYL;
		int32 TeamIndex = PS->Team->TeamIndex;
		DrawTexture(UTHUDOwner->HUDAtlas, XOffset, YPos, IconHeight, IconHeight, UTHUDOwner->TeamIconUV[TeamIndex].X, UTHUDOwner->TeamIconUV[TeamIndex].Y, 72, 72, 1.f, PS->Team->TeamColor);
	}

	Canvas->DrawText(UTHUDOwner->MediumFont, CombinedHeader, XOffset + 0.5f*(ScoreWidth - XL), YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += MedYL;

	if (StatsPageIndex == 0)
	{
		DrawPlayerStats(PS, DeltaTime, YPos, XOffset, ScoreWidth, PageBottom, StatsFontInfo);
	}
	else if (StatsPageIndex == 1)
	{
		DrawWeaponStats(PS, DeltaTime, YPos, XOffset, ScoreWidth, PageBottom, StatsFontInfo);
	}
	else if (StatsPageIndex == 2)
	{
		DrawRewardStats(PS, DeltaTime, YPos, XOffset, ScoreWidth, PageBottom, StatsFontInfo);
	}
	else if (StatsPageIndex == 3)
	{
		DrawMovementStats(PS, DeltaTime, YPos, XOffset, ScoreWidth, PageBottom, StatsFontInfo);
	}
	FString TabInstruction = (StatsPageIndex < StatsPageFooters.Num()) ? StatsPageFooters[StatsPageIndex].ToString() : "";
	Canvas->DrawText(UTHUDOwner->SmallFont, TabInstruction, XOffset, PageBottom - StatsFontInfo.TextHeight, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
}

void UUTScoreboard::DrawPlayerStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo)
{
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Kills", "Kills"), PS->Kills, PS->Kills, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Deaths", "Deaths"), PS->Deaths, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Suicides", "Suicides"), PS->GetStatsValue(NAME_Suicides), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "ScorePM", "Score Per Minute"), FString::Printf(TEXT(" %6.2f"), ((PS->StartTime <  GetWorld()->GameState->ElapsedTime) ? PS->Score * 60.f / (GetWorld()->GameState->ElapsedTime - PS->StartTime) : 0.f)), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "KDRatio", "K/D Ratio"), FString::Printf(TEXT(" %6.2f"), ((PS->Deaths > 0) ? float(PS->Kills) / PS->Deaths : 0.f)), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	Canvas->DrawText(UTHUDOwner->SmallFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "BeltPickups", "Shield Belt Pickups"), PS->GetStatsValue(NAME_ShieldBeltCount), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "VestPickups", "Armor Vest Pickups"), PS->GetStatsValue(NAME_ArmorVestCount), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "PadPickups", "Thigh Pad Pickups"), PS->GetStatsValue(NAME_ArmorPadsCount), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "HelmetPickups", "Helmet Pickups"), PS->GetStatsValue(NAME_HelmetCount), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);

	int32 PickupCount = PS->GetStatsValue(NAME_UDamageCount);
	if (PickupCount > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "UDamagePickups", "UDamage Pickups"), PickupCount, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	PickupCount = PS->GetStatsValue(NAME_BerserkCount);
	if (PickupCount > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "BerserkPickups", "Berserk Pickups"), PickupCount, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	PickupCount = PS->GetStatsValue(NAME_InvisibilityCount);
	if (PickupCount > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "InvisibilityPickups", "Invisibility Pickups"), PickupCount, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	PickupCount = PS->GetStatsValue(NAME_KegCount);
	if (PickupCount > 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "KegPickups", "Keg Pickups"), PickupCount, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}

	int32 ClockVal = PS->GetStatsValue(NAME_UDamageTime);
	if (ClockVal > 0)
	{
		FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), ClockVal, false);
		DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "UDamage", "UDamage Control"), ClockString.ToString(), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	}
	ClockVal = PS->GetStatsValue(NAME_BerserkTime);
	if (ClockVal > 0)
	{
		FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), ClockVal, false);
		DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "Berserk", "Berserk Control"), ClockString.ToString(), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	}
	ClockVal = PS->GetStatsValue(NAME_InvisibilityTime);
	if (ClockVal > 0)
	{
		FText ClockString = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), ClockVal, false);
		DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "Invisibility", "Invisibility Control"), ClockString.ToString(), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	}

	int32 BootJumps = PS->GetStatsValue(NAME_BootJumps);
	if (BootJumps != 0)
	{
		DrawStatsLine(NSLOCTEXT("UTScoreboard", "JumpBootJumps", "JumpBoot Jumps"), BootJumps, -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	}
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "Scoring", "SCORE"), -1, PS->Score, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
}

void UUTScoreboard::DrawRewardStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo)
{
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "DoubleKills", "Double Kill"), PS->GetStatsValue(NAME_MultiKillLevel0), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "MultiKills", "Multi Kill"), PS->GetStatsValue(NAME_MultiKillLevel1), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "UltraKills", "Ultra Kill"), PS->GetStatsValue(NAME_MultiKillLevel2), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "MonsterKills", "Monster Kill"), PS->GetStatsValue(NAME_MultiKillLevel3), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	Canvas->DrawText(UTHUDOwner->SmallFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;
	Canvas->DrawText(UTHUDOwner->SmallFont, "SPREES", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "KillingSprees", "Killing Spree"), PS->GetStatsValue(NAME_SpreeKillLevel0), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "RampageSprees", "Rampage"), PS->GetStatsValue(NAME_SpreeKillLevel1), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "DominatingSprees", "Dominating"), PS->GetStatsValue(NAME_SpreeKillLevel2), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "UnstoppableSprees", "Unstoppable"), PS->GetStatsValue(NAME_SpreeKillLevel3), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "GodlikeSprees", "Godlike"), PS->GetStatsValue(NAME_SpreeKillLevel4), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
}

void UUTScoreboard::DrawMovementStats(AUTPlayerState* PS, float DeltaTime, float& YPos, float XOffset, float ScoreWidth, float PageBottom, const FStatsFontInfo& StatsFontInfo)
{
	Canvas->DrawText(UTHUDOwner->TinyFont, "Distances in meters", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;
	float RunDistance = PS->GetStatsValue(NAME_RunDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "RunDistance", "Run Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*RunDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	float SprintDistance = PS->GetStatsValue(NAME_SprintDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "SprintDistance", "Sprint Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*SprintDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	float SlideDistance = PS->GetStatsValue(NAME_SlideDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "SlideDistance", "Slide Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*SlideDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	float WallRunDistance = PS->GetStatsValue(NAME_WallRunDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "WallRunDistance", "WallRun Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*WallRunDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	float FallDistance = PS->GetStatsValue(NAME_InAirDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "FallDistance", "Fall Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*FallDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	float SwimDistance = PS->GetStatsValue(NAME_SwimDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "SwimDistance", "Swim Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*SwimDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	float TranslocDistance = PS->GetStatsValue(NAME_TranslocDist);
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "TranslocDistance", "Teleport Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*TranslocDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	Canvas->DrawText(UTHUDOwner->SmallFont, "                                                             ----", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;
	float TotalDistance = RunDistance + SprintDistance + FallDistance + SwimDistance + TranslocDistance + SlideDistance + WallRunDistance;
	DrawTextStatsLine(NSLOCTEXT("UTScoreboard", "Total Dist", "Total Distance"), FString::Printf(TEXT(" %8.1f"), 0.01f*TotalDistance), "", DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth, 0);
	Canvas->DrawText(UTHUDOwner->SmallFont, "----------------------------------------------------------------", XOffset, YPos, RenderScale, RenderScale, StatsFontInfo.TextRenderInfo);
	YPos += StatsFontInfo.TextHeight;

	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumJumps", "Jumps"), PS->GetStatsValue(NAME_NumJumps), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumDodges", "Dodges"), PS->GetStatsValue(NAME_NumDodges), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumWallDodges", "Wall Dodges"), PS->GetStatsValue(NAME_NumWallDodges), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumLiftJumps", "Lift Jumps"), PS->GetStatsValue(NAME_NumLiftJumps), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumFloorSlides", "Floor Slides"), PS->GetStatsValue(NAME_NumFloorSlides), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumWallRuns", "Wall Runs"), PS->GetStatsValue(NAME_NumWallRuns), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumImpactJumps", "Impact Jumps"), PS->GetStatsValue(NAME_NumImpactJumps), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
//	DrawStatsLine(NSLOCTEXT("UTScoreboard", "NumRocketJumps", "Rocket Jumps"), PS->GetStatsValue(NAME_NumRocketJumps), -1, DeltaTime, XOffset, YPos, StatsFontInfo, ScoreWidth);
}

float UUTScoreboard::GetDrawScaleOverride()
{
	return 1.f;
}

TWeakObjectPtr<AUTPlayerState> UUTScoreboard::GetSelectedPlayer()
{
	return SelectedPlayer;
}