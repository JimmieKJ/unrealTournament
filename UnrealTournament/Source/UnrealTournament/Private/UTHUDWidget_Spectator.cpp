// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_Spectator.h"
#include "UTCarriedObject.h"
#include "UTCTFGameState.h"
#include "UTDemoRecSpectator.h"

UUTHUDWidget_Spectator::UUTHUDWidget_Spectator(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position=FVector2D(0,0);
	Size=FVector2D(1920.0f,54.0f);
	ScreenPosition=FVector2D(0.0f, 0.94f);
	Origin=FVector2D(0.0f,0.0f);
}

bool UUTHUDWidget_Spectator::ShouldDraw_Implementation(bool bShowScores)
{
	if (UTHUDOwner && Cast<AUTDemoRecSpectator>(UTHUDOwner->UTPlayerOwner))
	{
		return true;
	}

	if (UTHUDOwner && UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTGameState && (UTGameState->GetMatchState() != MatchState::PlayerIntro))
	{
		AUTPlayerState* PS = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (UTGameState->IsMatchIntermission() || UTGameState->HasMatchEnded() || !UTGameState->HasMatchStarted())
		{
			return true;
		}
		return (PS->bOnlySpectator || PS->bOutOfLives || (UTCharacterOwner ? UTCharacterOwner->IsDead() : (UTHUDOwner->UTPlayerOwner->GetPawn() == NULL)));
	}
	return false;
}

void UUTHUDWidget_Spectator::DrawSimpleMessage(FText SimpleMessage, float DeltaTime, FText ViewingMessage)
{
	if (SimpleMessage.IsEmpty())
	{
		return;
	}
	bool bViewingMessage = !ViewingMessage.IsEmpty();
	float Scaling = bViewingMessage ? FMath::Max(1.f, 3.f - 6.f*(GetWorld()->GetTimeSeconds() - ViewCharChangeTime)) : 0.5f;
	float ScreenWidth = (Canvas->ClipX / RenderScale);
	float BackgroundWidth = ScreenWidth;
	float TextPosition = 200.f;
	float MessageOffset = 0.f;
	float YOffset = 0.f;
	if (bViewingMessage && UTHUDOwner->LargeFont && UTHUDOwner->SmallFont)
	{
		float XL, YL;
		Canvas->StrLen(UTHUDOwner->MediumFont, SimpleMessage.ToString(), BackgroundWidth, YL);
		Canvas->StrLen(UTHUDOwner->SmallFont, ViewingMessage.ToString(), XL, YL);
		BackgroundWidth = FMath::Max(XL, BackgroundWidth);
		BackgroundWidth = Scaling* (FMath::Max(BackgroundWidth, 128.f) + 64.f);
		MessageOffset = (ScreenWidth - BackgroundWidth) * (UTGameState->HasMatchEnded() ? 0.5f : 1.f);
		TextPosition = 32.f + MessageOffset;
		YOffset = -32.f;
	}

	// Draw the Background
	bMaintainAspectRatio = false;
	DrawTexture(UTHUDOwner->ScoreboardAtlas, MessageOffset, YOffset, BackgroundWidth, Scaling * 108.0f, 4, 2, 124, 128, 1.0);
	if (bViewingMessage)
	{
		DrawText(ViewingMessage, TextPosition, YOffset + 14.f, UTHUDOwner->SmallFont, Scaling, 1.f, GetMessageColor(), ETextHorzPos::Left, ETextVertPos::Center);
	}
	else
	{
		bMaintainAspectRatio = true;

		// Draw the Logo
		DrawTexture(UTHUDOwner->ScoreboardAtlas, 20, 27.f, 150.5f, 49.f, 162, 14, 301, 98.0, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));

		// Draw the Spacer Bar
		DrawTexture(UTHUDOwner->ScoreboardAtlas, 190.5f, 27.f, 4.f, 49.5f, 488, 13, 4, 99, 1.0f, FLinearColor::White, FVector2D(0.0, 0.5));
	}
	DrawText(SimpleMessage, TextPosition, YOffset + 20.f, UTHUDOwner->MediumFont, 1.f, 1.f, GetMessageColor(), ETextHorzPos::Left, ETextVertPos::Center);
}

void UUTHUDWidget_Spectator::DrawSpawnPacks(float DeltaTime)
{
	if (UTGameState && UTGameState->SpawnPacks.Num() > 0 )
	{
		FVector2D TextSize = DrawText(NSLOCTEXT("UUTHUDWidget_Spectator","SpawnPackTitle","Loadouts:"), 20.0f, -50.0f, UTHUDOwner->MediumFont, 1.0, 1.f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);

		float XPos = 60 + TextSize.X;
		AUTPlayerState* PlayerState = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		for (int32 i = 0 ; i < UTGameState->SpawnPacks.Num(); i++)
		{
			FLinearColor DrawColor = UTGameState->SpawnPacks[i].PackTag == PlayerState->CurrentLoadoutPackTag ? FLinearColor(0.0f,0.5f,1.0f,1.0f) : FLinearColor::White;
			TArray<FString> Keys;
			UTHUDOwner->UTPlayerOwner->ResolveKeybind(FString::Printf(TEXT("switchweapon %i"), i+1), Keys, false, false);
			if (Keys.Num() > 0)
			{
				FText OutputText = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator","TitleTextFormat","({0}) {1}"), FText::FromString(UTHUDOwner->UTPlayerOwner->FixedupKeyname(Keys[0])), FText::FromString(UTGameState->SpawnPacks[i].PackTitle));
				TextSize = DrawText(OutputText, XPos, -50.0f, UTHUDOwner->MediumFont, 1.0, 1.f, DrawColor, ETextHorzPos::Left, ETextVertPos::Center);		
				XPos += 400 - TextSize.X;
			}
		}
	}
}

void UUTHUDWidget_Spectator::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	FText ShortMessage;
	FText SpectatorMessage = GetSpectatorMessageText(ShortMessage);
	DrawSimpleMessage(SpectatorMessage, DeltaTime, ShortMessage);
	DrawSpawnPacks(DeltaTime);
}

FText UUTHUDWidget_Spectator::GetSpectatorMessageText(FText& ShortMessage)
{
	FText SpectatorMessage;
	ShortMessage = FText::GetEmpty();
	if (UTGameState)
	{
		bool bDemoRecSpectator = UTHUDOwner->UTPlayerOwner && Cast<AUTDemoRecSpectator>(UTHUDOwner->UTPlayerOwner);
		AUTPlayerState* UTPS = UTHUDOwner->UTPlayerOwner->UTPlayerState;

		if (bDemoRecSpectator)
		{
			AActor* ViewActor = UTHUDOwner->UTPlayerOwner->GetViewTarget();
			AUTCharacter* ViewCharacter = Cast<AUTCharacter>(ViewActor);
			if (!ViewCharacter)
			{
				AUTCarriedObject* Flag = Cast<AUTCarriedObject>(ViewActor);
				if (Flag && Flag->Holder)
				{
					ViewCharacter = Cast<AUTCharacter>(Flag->GetAttachmentReplication().AttachParent);
				}
			}
			if (ViewCharacter && ViewCharacter->PlayerState)
			{
				FFormatNamedArguments Args;
				Args.Add("PlayerName", FText::AsCultureInvariant(ViewCharacter->PlayerState->PlayerName));
				ShortMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "NowViewing", "Now viewing");
				SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatching", "{PlayerName}"), Args);
			}
		}
		else if (!UTGameState->HasMatchStarted())
		{
			// Look to see if we are waiting to play and if we must be ready.  If we aren't, just exit cause we don
			if (UTGameState->IsMatchInCountdown())
			{
				if (UTPS && UTPS->RespawnChoiceA && UTPS->RespawnChoiceB)
				{
					SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "Choose Start", "Choose your start position");
				}
				else
				{
					SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "MatchStarting", "Match is about to start");
				}
			}
			else if (UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bIsWarmingUp)
			{
				if (UTCharacterOwner ? UTCharacterOwner->IsDead() : (UTHUDOwner->UTPlayerOwner->GetPawn() == NULL))
				{
					if (UTPS->RespawnTime > 0.0f)
					{
						FFormatNamedArguments Args;
						static const FNumberFormattingOptions RespawnTimeFormat = FNumberFormattingOptions()
							.SetMinimumFractionalDigits(0)
							.SetMaximumFractionalDigits(0);
						Args.Add("RespawnTime", FText::AsNumber(UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime + 0.5f, &RespawnTimeFormat));
						SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "RespawnWaitMessage", "You can respawn in {RespawnTime}..."), Args);
					}
					else
					{
						SpectatorMessage = (UTGameState->ForceRespawnTime > 0.3f) ? NSLOCTEXT("UUTHUDWidget_Spectator", "RespawnMessage", "Press [FIRE] to respawn...") : FText::GetEmpty();
					}
				}
				else
				{
					ShortMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "PressEnter", "Press [ENTER] to leave");
					SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "Warmup", "Warm Up");
				}
			}
			else if (UTPS && UTPS->bReadyToPlay && (UTPS->GetNetMode() != NM_Standalone))
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "IsReadyTeam", "You are ready, press [ENTER] to warm up.");
			}
			else if (UTGameState->PlayersNeeded > 0)
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForPlayers", "Waiting for players to join. Press [FIRE] to ready up.");
			}
			else if (UTPS && UTPS->bCaster)
			{
				SpectatorMessage = (UTGameState->AreAllPlayersReady())
					? NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForCaster", "All players are ready. Press [Enter] to start match.")
					: NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForReady", "Waiting for players to ready up.");
			}
			else if (UTGameState->bRankedSession)
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "RankedSessionStart", "Your game will start shortly.");
			}
			else if (UTPS && UTPS->bOnlySpectator)
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForReady", "Waiting for players to ready up.");
			}
			else if (UTHUDOwner->GetScoreboard() && UTHUDOwner->GetScoreboard()->IsInteractive())
			{
				SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "CloseMenu", "Press [ESC] to close menu.");
			}
			else
			{
				SpectatorMessage = (UTGameState->bTeamGame && UTGameState->bAllowTeamSwitches)
					? NSLOCTEXT("UUTHUDWidget_Spectator", "GetReadyTeam", "Press [FIRE] to ready up, [ALTFIRE] to change teams.")
					: NSLOCTEXT("UUTHUDWidget_Spectator", "GetReady", "Press [FIRE] when you are ready.");
			}
		}
		else if (!UTGameState->HasMatchEnded())
		{
			if (UTGameState->IsMatchIntermission())
			{
				if (UTGameState->bCasterControl && UTGameState->bStopGameClock == true && UTPS != nullptr)
				{
					SpectatorMessage = (UTPS->bCaster)
						? NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingCasterHalfTime", "Press [Enter] to start next half.")
						: NSLOCTEXT("UUTHUDWidget_Spectator", "WaitingForCasterHalfTime", "Waiting for caster to start next half.");
				}
				else
				{
					int32 IntermissionTime = UTGameState->GetIntermissionTime();
					if (IntermissionTime > 0)
					{
						FFormatNamedArguments Args;
						Args.Add("Time", FText::AsNumber(IntermissionTime));
						AUTCTFGameState* CTFGameState = Cast<AUTCTFGameState>(UTGameState);
						SpectatorMessage = !CTFGameState || (CTFGameState->CTFRound == 0)
							? FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "HalfTime", "HALFTIME - Game resumes in {Time}"), Args)
							: FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "Intermission", "Game resumes in {Time}"), Args);
					}
				}
			}
			else if (UTPS && (UTPS->bOnlySpectator || UTPS->bOutOfLives))
			{
				AActor* ViewActor = UTHUDOwner->UTPlayerOwner->GetViewTarget();
				AUTCharacter* ViewCharacter = Cast<AUTCharacter>(ViewActor);
				if (!ViewCharacter)
				{
					AUTCarriedObject* Flag = Cast<AUTCarriedObject>(ViewActor);
					if (Flag && Flag->Holder)
					{
						ViewCharacter = Cast<AUTCharacter>(Flag->GetAttachmentReplication().AttachParent);
					}
				}
				if (ViewCharacter && ViewCharacter->PlayerState)
				{
					if (LastViewedPS != ViewCharacter->PlayerState)
					{
						ViewCharChangeTime = ViewCharacter->GetWorld()->GetTimeSeconds();
						LastViewedPS = Cast<AUTPlayerState>(ViewCharacter->PlayerState);
					}
					FFormatNamedArguments Args;
					Args.Add("PlayerName", FText::AsCultureInvariant(ViewCharacter->PlayerState->PlayerName));
					ShortMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "NowViewing", "Now viewing");
					SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatching", "{PlayerName}"), Args);
				}
				else
				{
					LastViewedPS = NULL;
				}
			}
			else if (UTPS && (UTCharacterOwner ? UTCharacterOwner->IsDead() : (UTHUDOwner->UTPlayerOwner->GetPawn() == NULL)))
			{
				if (UTPS->RespawnTime > 0.0f)
				{
					FFormatNamedArguments Args;
					static const FNumberFormattingOptions RespawnTimeFormat = FNumberFormattingOptions()
						.SetMinimumFractionalDigits(0)
						.SetMaximumFractionalDigits(0);
					Args.Add("RespawnTime", FText::AsNumber(UTHUDOwner->UTPlayerOwner->UTPlayerState->RespawnTime + 0.5f, &RespawnTimeFormat));
					SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "RespawnWaitMessage", "You can respawn in {RespawnTime}..."), Args);
				}
				else if (!UTPS->bOutOfLives)
				{
					if (UTPS->RespawnChoiceA != nullptr)
					{
						SpectatorMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "ChooseRespawnMessage", "Choose a respawn point with [FIRE] or [ALT-FIRE]");
					}
					else
					{
						SpectatorMessage = ((UTGameState->ForceRespawnTime > 0.3f) || (UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->bPlayerIsWaiting)) ? NSLOCTEXT("UUTHUDWidget_Spectator", "RespawnMessage", "Press [FIRE] to respawn...") : FText::GetEmpty();
					}
				}
			}
		}
		else
		{
			AUTCharacter* ViewCharacter = Cast<AUTCharacter>(UTHUDOwner->UTPlayerOwner->GetViewTarget());
			AUTPlayerState* PS = ViewCharacter ? Cast<AUTPlayerState>(ViewCharacter->PlayerState) : NULL;
			if (PS)
			{
				FFormatNamedArguments Args;
				Args.Add("PlayerName", FText::AsCultureInvariant(PS->PlayerName));
				ShortMessage = NSLOCTEXT("UUTHUDWidget_Spectator", "NowViewing", "Now viewing");
				if (UTGameState->bTeamGame && PS && PS->Team && (!UTGameState->GameModeClass || !UTGameState->GameModeClass->GetDefaultObject<AUTTeamGameMode>() || UTGameState->GameModeClass->GetDefaultObject<AUTTeamGameMode>()->bAnnounceTeam))
				{
					SpectatorMessage = (PS->Team->TeamIndex == 0)
						? FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatchingRed", "Red Team Led by {PlayerName}"), Args)
						: FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatchingBlue", "Blue Team Led by {PlayerName}"), Args);
				}
				else
				{
					SpectatorMessage = FText::Format(NSLOCTEXT("UUTHUDWidget_Spectator", "SpectatorPlayerWatching", "{PlayerName}"), Args);
				}
			}
		}
	}
	return SpectatorMessage;
}

float UUTHUDWidget_Spectator::GetDrawScaleOverride()
{
	return 1.0;
}
