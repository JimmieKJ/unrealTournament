// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTScoreboard.h"

UUTScoreboard::UUTScoreboard(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(1440.0f, 1080.0f);
	ScreenPosition = FVector2D(0.5f, 0.5f);
	Origin = FVector2D(0.5f, 0.5f);

	NumPages = 1;
	ColumnHeaderPlayerX = 0.125;
	ColumnHeaderScoreX = 0.7;
	ColumnHeaderDeathsX = 0.84;
	ColumnHeaderPingX = 0.96;
	ColumnHeaderY = 8;
	ColumnY = 12;
	ColumnMedalX = 0.6;
	FooterPosY = 1020.f;
	CellHeight = 40;

	FlagX = 0.065;

	BadgeNumberUVs.Add(FVector2D(248,183));
	BadgeNumberUVs.Add(FVector2D(283,183));
	BadgeNumberUVs.Add(FVector2D(318,183));
	BadgeNumberUVs.Add(FVector2D(353,183));
	BadgeNumberUVs.Add(FVector2D(388,183));
	BadgeNumberUVs.Add(FVector2D(423,183));
	BadgeNumberUVs.Add(FVector2D(458,183));
	BadgeNumberUVs.Add(FVector2D(248,219));
	BadgeNumberUVs.Add(FVector2D(283,219));

	BadgeUVs.Add(FVector2D(388,219));
	BadgeUVs.Add(FVector2D(353,219));
	BadgeUVs.Add(FVector2D(318,219));

	CenterBuffer = 10;

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FlagTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/CountryFlags.CountryFlags'"));
	FlagAtlas = FlagTex.Object;
}

void UUTScoreboard::AdvancePage(int32 Increment)
{
	UTHUDOwner->ScoreboardPage = uint32(FMath::Clamp<int32>(int32(UTHUDOwner->ScoreboardPage) + Increment, 0, NumPages - 1));
	PageChanged();
}

void UUTScoreboard::SetPage(uint32 NewPage)
{
	UTHUDOwner->ScoreboardPage = FMath::Min<uint32>(NewPage, NumPages - 1);
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
}

void UUTScoreboard::Draw_Implementation(float RenderDelta)
{
	Super::Draw_Implementation(RenderDelta);

	float YOffset = 0;
	DrawGamePanel(RenderDelta, YOffset);
	DrawTeamPanel(RenderDelta, YOffset);
	DrawScorePanel(RenderDelta, YOffset);
	DrawServerPanel(RenderDelta, FooterPosY);
}

void UUTScoreboard::DrawGamePanel(float RenderDelta, float& YOffset)
{
	// Draw the Background
	DrawTexture(TextureAtlas,0,0, Size.X, 128, 4,2,124, 128, 1.0);

	// Draw the Logo
	DrawTexture(TextureAtlas, 165, 60, 301, 98, 162,14,301, 98.0, 1.0f, FLinearColor::White, FVector2D(0.5, 0.5));
	
	// Draw the Spacer Bar
	DrawTexture(TextureAtlas, 325, 60, 4, 99, 488,13,4,99, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));
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
		DrawText(GameName, 355, 28, UTHUDOwner->LargeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center); // 470
	}
	if (!MapName.IsEmpty())
	{
		DrawText(MapName, 355, 80, UTHUDOwner->LargeFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center); // 470
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
			DrawText(Score, Size.X * 0.985, 28, UTHUDOwner->MediumFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
		}

		FText StatusText = UTGameState->GetGameStatusText();
		if (!StatusText.IsEmpty())
		{
			DrawText(StatusText, Size.X * 0.985, 90, UTHUDOwner->MediumFont, 1.0, 1.0, FLinearColor::Yellow, ETextHorzPos::Right, ETextVertPos::Center);
		}
		else
		{
			float RemainingTime = UTGameState ? UTGameState->GetClockTime() : 0.f;
			FText Timer = UTHUDOwner->ConvertTime(FText::GetEmpty(), FText::GetEmpty(), RemainingTime, true, true, true);
			DrawText(Timer, Size.X * 0.985, 88, UTHUDOwner->NumberFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
		}
	}
}

void UUTScoreboard::DrawTeamPanel(float RenderDelta, float& YOffset)
{
	YOffset += 39.0; // A small gap
}

void UUTScoreboard::DrawScorePanel(float RenderDelta, float& YOffset)
{
	if (bIsInteractive)
	{
		SelectionStack.Empty();
	}
	if (UTGameState)
	{
		float DrawY = YOffset;
		DrawScoreHeaders(RenderDelta, DrawY);
		DrawPlayerScores(RenderDelta, DrawY);
	}
}

void UUTScoreboard::DrawScoreHeaders(float RenderDelta, float& YOffset)
{
	float Width = (Size.X * 0.5) - CenterBuffer;  
	float Height = 23;

	FText CH_PlayerName = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerName", "PLAYER");
	FText CH_Score = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerScore", "SCORE");
	FText CH_Deaths = NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerDeaths", "DEATHS");
	FText CH_Ping = (GetWorld()->GetNetMode() == NM_Standalone) ? NSLOCTEXT("UTScoreboard", "ColumnHeader_BotSkill", "SKILL") : NSLOCTEXT("UTScoreboard", "ColumnHeader_PlayerPing", "PING");
	FText CH_Ready = NSLOCTEXT("UTScoreboard", "ColumnHeader_Ready", "");

	int32 ColumnCnt = ((UTGameState && UTGameState->bTeamGame) || ActualPlayerCount > 16) ? 2 : 1;
	float XOffset = ColumnCnt > 1 ? 0 : (Size.X * 0.5) - (Width * 0.5);

	for (int32 i = 0; i < ColumnCnt; i++)
	{
		// Draw the background Border
		DrawTexture(TextureAtlas, XOffset, YOffset, Width, Height, 149, 138, 32, 32, 1.0, FLinearColor(0.72f, 0.72f, 0.72f, 0.85f));

		DrawText(CH_PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Left, ETextVertPos::Center);

		if (UTGameState && UTGameState->HasMatchStarted())
		{
			DrawText(CH_Score, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
			DrawText(CH_Deaths, XOffset + (Width * ColumnHeaderDeathsX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		else
		{
			DrawText(CH_Ready, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);
		}
		DrawText(CH_Ping, XOffset + (Width * ColumnHeaderPingX), YOffset + ColumnHeaderY, UTHUDOwner->TinyFont, 1.0f, 1.0f, FLinearColor::Black, ETextHorzPos::Center, ETextVertPos::Center);

		XOffset = Size.X - Width;
	}

	YOffset += Height + 4;
}

void UUTScoreboard::DrawPlayerScores(float RenderDelta, float& YOffset)
{
	if (!UTGameState)
	{
		return;
	}

	int32 Place = 1;
	int32 NumSpectators = 0;
	int32 XOffset = ActualPlayerCount > 16 ? 0 : (Size.X * 0.5) - ( ((Size.X * 0.5) - CenterBuffer) * 0.5);
	float DrawOffset = YOffset;
	for (int32 i=0; i<UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PlayerState)
		{
			if (!PlayerState->bOnlySpectator)
			{
				DrawPlayer(Place, PlayerState, RenderDelta, XOffset, DrawOffset);
				DrawOffset += CellHeight;
				Place++;
				if (Place == 17)
				{
					XOffset = Size.X - ((Size.X * 0.5) - CenterBuffer);
					DrawOffset = YOffset;
				}
			}
			else
			{
				NumSpectators++;
			}
		}
	}
	
	if (UTGameState->PlayerArray.Num() <= 28 && NumSpectators > 0)
	{
		FText SpectatorCount = (NumSpectators == 1) 
			? NSLOCTEXT("UTScoreboard", "OneSpectator", "1 spectator is watching this match")
			: FText::Format(NSLOCTEXT("UTScoreboard","SpectatorFormat","{0} spectators are watching this match"), FText::AsNumber(NumSpectators));
		DrawText(SpectatorCount, 635, 765, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor(0.75f, 0.75f, 0.75f, 1.0f), ETextHorzPos::Center, ETextVertPos::Bottom);
	}
}

void UUTScoreboard::DrawPlayer(int32 Index, AUTPlayerState* PlayerState, float RenderDelta, float XOffset, float YOffset)
{
	if (PlayerState == NULL) return;	// Safeguard

	FLinearColor DrawColor = FLinearColor::White;
	float BarOpacity = 0.3;
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

	FText Position = FText::Format(NSLOCTEXT("UTScoreboard", "PositionFormatText", "{0}."), FText::AsNumber(Index));
	FText PlayerName = FText::FromString(GetClampedName(PlayerState, UTHUDOwner->MediumFont, 1.f, 0.475f*Width));
	FText PlayerScore = FText::AsNumber(int32(PlayerState->Score));
	FText PlayerDeaths = FText::AsNumber(PlayerState->Deaths);

	int32 Ping = PlayerState->Ping * 4;
	if (UTHUDOwner->UTPlayerOwner->UTPlayerState == PlayerState)
	{
		Ping = PlayerState->ExactPing;
		DrawColor = FLinearColor(0.0f,0.92f,1.0f,1.0f);
		BarOpacity = 0.5;
	}

	else if (PlayerState->bIsFriend)
	{
		DrawColor = FLinearColor(FColor(254, 255, 174, 255));
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

	// Draw the position
	
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

	if (PlayerState->KickPercent > 0)
	{
		FText Kick = FText::Format(NSLOCTEXT("Common","PercFormat","{0}%"),FText::AsNumber(PlayerState->KickPercent));
		DrawText(Kick, XOffset, YOffset + ColumnY, UTHUDOwner->TinyFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);
	}

	DrawTexture(TextureAtlas, XOffset, YOffset, Width, 36, 149, 138, 32, 32, FinalBarOpacity, BarColor);	// NOTE: Once I make these interactable.. have a selection color too

	int32 FlagU = (PlayerState->CountryFlag % 8) * 32;
	int32 FlagV = (PlayerState->CountryFlag / 8) * 24;

	DrawTexture(FlagAtlas, XOffset + (Width * FlagX), YOffset + 18, 32,24, FlagU,FlagV,32,24,1.0, FLinearColor::White, FVector2D(0.0f,0.5f));	// Add a function to support additional flags

	// Draw the Text
	DrawText(Position, XOffset + (Width * FlagX - 5), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Right, ETextVertPos::Center);
	FVector2D NameSize = DrawText(PlayerName, XOffset + (Width * ColumnHeaderPlayerX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);

	if (PlayerState->bIsFriend)
	{
		DrawTexture(TextureAtlas, XOffset + (Width * ColumnHeaderPlayerX) + NameSize.X + 5, YOffset + 18, 30, 24, 236, 136, 30, 24, 1.0, FLinearColor::White, FVector2D(0.0f, 0.5f));
	}

	if (UTGameState && UTGameState->HasMatchStarted())
	{
		DrawText(PlayerScore, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
		DrawText(PlayerDeaths, XOffset + (Width * ColumnHeaderDeathsX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	else
	{
		PlayerState->UpdateReady();
		FText PlayerReady = PlayerState->bReadyToPlay ? NSLOCTEXT("UTScoreboard", "READY", "READY") : NSLOCTEXT("UTScoreboard", "NOTREADY", "");
		if (PlayerState->bPendingTeamSwitch)
		{
			PlayerReady = NSLOCTEXT("UTScoreboard", "TEAMSWITCH", "TEAM SWAP");
		}
		DrawText(PlayerReady, XOffset + (Width * ColumnHeaderScoreX), YOffset + ColumnY, UTHUDOwner->MediumFont, 1.0f, 1.0f, PlayerState->ReadyColor, ETextHorzPos::Center, ETextVertPos::Center);
	}
	DrawText(PlayerPing, XOffset + (Width * ColumnHeaderPingX), YOffset + ColumnY, UTHUDOwner->SmallFont, 1.0f, 1.0f, DrawColor, ETextHorzPos::Center, ETextVertPos::Center);

	if (!PlayerState->bIsABot)
	{
		int32 Badge;
		int32 Level;

		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(UTHUDOwner->UTPlayerOwner->Player);
		if (LP)
		{
			UUTLocalPlayer::GetBadgeFromELO(PlayerState->AverageRank, Badge, Level);
			Badge = FMath::Clamp<int32>(Badge,0, 3);
			Level = FMath::Clamp<int32>(Level,0, 8);
			DrawTexture(TextureAtlas, XOffset + (Width * ColumnMedalX), YOffset + 16, 32, 32, BadgeUVs[Badge].X, BadgeUVs[Badge].Y, 32, 32, 1.0, FLinearColor::White, FVector2D(0.5f, 0.5f));
			DrawTexture(TextureAtlas, XOffset + (Width * ColumnMedalX), YOffset + 16, 32, 32, BadgeNumberUVs[Level].X, BadgeNumberUVs[Level].Y, 32, 32, 1.0, FLinearColor::White, FVector2D(0.5f, 0.5f));
		}
	}
}

void UUTScoreboard::DrawServerPanel(float RenderDelta, float YOffset)
{
	if (UTGameState)
	{
		if (!UTGameState->HasMatchStarted())
		{
			// Only draw if there is room above spectator panel
			if (UTGameState->PlayerArray.Num() > 26)
			{
				return;
			}
		}

		// Draw the Background
		DrawTexture(TextureAtlas, 0, YOffset, 1269, 38, 4, 132, 30, 38, 1.0);
		DrawText(FText::FromString(UTGameState->ServerName), 10, YOffset + 13, UTHUDOwner->SmallFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawText(FText::FromString(UTGameState->ServerDescription), 1259, YOffset + 13, UTHUDOwner->SmallFont, 1.0, 1.0, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Center);
		if (NumPages > 1)
		{
			FText PageText = FText::Format(NSLOCTEXT("UTScoreboard", "Pages", "Arrow keys to switch page ({0} of {1})"), FText::AsNumber(UTHUDOwner->ScoreboardPage + 1), FText::AsNumber(NumPages));
			DrawText(PageText, Size.X * 0.5f, YOffset + 13, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Center, ETextVertPos::Center);
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
		}
	}
}

