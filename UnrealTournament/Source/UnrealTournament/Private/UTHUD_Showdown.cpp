// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_Showdown.h"
#include "UTShowdownGameState.h"
#include "UTShowdownGameMessage.h"
#include "UTPlayerStart.h"
#include "UTHUDWidgetMessage_KillIconMessages.h"

AUTHUD_Showdown::AUTHUD_Showdown(const FObjectInitializer& OI)
: Super(OI)
{
	ConstructorHelpers::FObjectFinder<UTexture2D> PlayerStartBGTextureObject(TEXT("/Game/RestrictedAssets/UI/MiniMap/minimap_atlas.minimap_atlas"));
	PlayerStartBGIcon.U = 64;
	PlayerStartBGIcon.V = 192;
	PlayerStartBGIcon.UL = 64;
	PlayerStartBGIcon.VL = 64;
	PlayerStartBGIcon.Texture = PlayerStartBGTextureObject.Object;
	ConstructorHelpers::FObjectFinder<UTexture2D> PlayerStartTextureObject(TEXT("/Game/RestrictedAssets/UI/MiniMap/minimap_atlas.minimap_atlas"));
	PlayerStartIcon.U = 128;
	PlayerStartIcon.V = 192;
	PlayerStartIcon.UL = 64;
	PlayerStartIcon.VL = 64;
	PlayerStartIcon.Texture = PlayerStartTextureObject.Object;
	ConstructorHelpers::FObjectFinder<UTexture2D> SelectedSpawnTextureObject(TEXT("/Game/RestrictedAssets/Weapons/Sniper/Assets/TargetCircle.TargetCircle"));
	SelectedSpawnTexture = SelectedSpawnTextureObject.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> HelpBGTex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	SpawnHelpTextBG.U = 4;
	SpawnHelpTextBG.V = 2;
	SpawnHelpTextBG.UL = 124;
	SpawnHelpTextBG.VL = 128;
	SpawnHelpTextBG.Texture = HelpBGTex.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> MapOpen(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Showdown/A_MapOpen.A_MapOpen'")); 
	MapOpenSound = MapOpen.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> PressedSelect(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Showdown/A_SpawnSelect_Self.A_SpawnSelect_Self'")); 
	SpawnSelectSound = PressedSelect.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSelect(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Showdown/A_SpawnSelect_Other.A_SpawnSelect_Other'")); 
	OtherSelectSound = OtherSelect.Object;

	SpawnPreviewCapture = OI.CreateDefaultSubobject<USceneCaptureComponent2D>(this, TEXT("SpawnPreviewCapture"));
	SpawnPreviewCapture->bCaptureEveryFrame = false;
	SpawnPreviewCapture->SetHiddenInGame(false);
	RootComponent = SpawnPreviewCapture; // just to quiet warning, has no relevance

	LastHoveredActorChangeTime = -1000.0f;
	bNeedOnDeckNotify = true;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Showdown/A_EnemyKilled.A_EnemyKilled'"));
	TeamKillSound = OtherSpreeSoundFinder.Object;
	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSpreeEndedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Showdown/A_TeammateKilled.A_TeammateKilled'"));
	TeamVictimSound = OtherSpreeEndedSoundFinder.Object;
//	static ConstructorHelpers::FObjectFinder<USoundBase> LMSVictimSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Showdown/A_TeammateKilled_LMS.A_TeammateKilled_LMS'"));
//	TeamLMSVictimSound = LMSVictimSoundFinder.Object;
}

void AUTHUD_Showdown::BeginPlay()
{
	Super::BeginPlay();

	SpawnPreviewCapture->TextureTarget = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass());
	SpawnPreviewCapture->TextureTarget->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
	SpawnPreviewCapture->TextureTarget->ClearColor = FLinearColor::Black;
	SpawnPreviewCapture->RegisterComponent();
	AddSpectatorWidgets();
}

void AUTHUD_Showdown::NotifyKill(APlayerState* POVPS, APlayerState* KillerPS, APlayerState* VictimPS)
{
	Super::NotifyKill(POVPS, KillerPS, VictimPS);

	if ((POVPS == KillerPS) || (VictimPS == nullptr))
	{
		// already notified
		return;
	}
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GetGameState());
	if (GS && GS->OnSameTeam(POVPS, VictimPS))
	{
		if (GetWorldTimerManager().IsTimerActive(PlayTeamKillHandle))
		{
			PlayTeamKillNotification();
		}
		GetWorldTimerManager().SetTimer(PlayTeamKillHandle, this, &AUTHUD_Showdown::PlayTeamKillNotification, 0.35f, false);
	}
	else
	{
		if (GetWorldTimerManager().IsTimerActive(PlayTeamVictimHandle))
		{
			PlayTeamVictimNotification();
		}
		GetWorldTimerManager().SetTimer(PlayTeamVictimHandle, this, &AUTHUD_Showdown::PlayTeamVictimNotification, 0.35f, false);
	}
}

void AUTHUD_Showdown::PlayTeamKillNotification()
{
	PlayerOwner->ClientPlaySound(TeamKillSound);
}

void AUTHUD_Showdown::PlayTeamVictimNotification()
{
	PlayerOwner->ClientPlaySound(TeamVictimSound);
}

UUTHUDWidget* AUTHUD_Showdown::AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass)
{
	UUTHUDWidget* Widget = Super::AddHudWidget(NewWidgetClass);
	if (KillIconWidget == nullptr)
	{
		KillIconWidget = Cast<UUTHUDWidgetMessage_KillIconMessages>(Widget);
	}
	return Widget;
}

void AUTHUD_Showdown::DrawMinimap(const FColor& DrawColor, float MapSize, FVector2D DrawPos)
{
	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();

	Super::DrawMinimap(DrawColor, MapSize, DrawPos);

	const float RenderScale = float(Canvas->SizeY) / 1080.0f;
	// draw PlayerStart icons
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		AUTPlayerStart* UTStart = Cast<AUTPlayerStart>(*It);
		if (UTStart == NULL || !UTStart->bIgnoreInShowdown)
		{
			FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
			AUTPlayerState* OwningPS = NULL;
			for (APlayerState* PS : GS->PlayerArray)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
				if (UTPS != NULL && UTPS->RespawnChoiceA == *It && UTPS->Team != NULL)
				{
					OwningPS = UTPS;
					break;
				}
			}

			if (OwningPS != NULL || GS->IsAllowedSpawnPoint(GS->SpawnSelector, *It))
			{
				Canvas->DrawColor = FColor::White;
			}
			else
			{
				Canvas->DrawColor = FColor(128, 128, 128, 192);
			}
			const float IconSize = (LastHoveredActor == *It) ? (40.0f * RenderScale * FMath::InterpEaseOut<float>(1.0f, 1.5f, FMath::Min<float>(0.2f, GetWorld()->RealTimeSeconds - LastHoveredActorChangeTime) * 5.0f, 2.0f)) : (40.0f * RenderScale);
			const FVector2D NormalizedUV(PlayerStartBGIcon.U / PlayerStartBGIcon.Texture->GetSurfaceWidth(), PlayerStartBGIcon.V / PlayerStartBGIcon.Texture->GetSurfaceHeight());
			const FVector2D NormalizedULVL(PlayerStartBGIcon.UL / PlayerStartBGIcon.Texture->GetSurfaceWidth(), PlayerStartBGIcon.VL / PlayerStartBGIcon.Texture->GetSurfaceHeight());
			Canvas->K2_DrawTexture(PlayerStartBGIcon.Texture, Pos - FVector2D(IconSize * 0.5f, IconSize * 0.5f), FVector2D(IconSize, IconSize), NormalizedUV, NormalizedULVL, Canvas->DrawColor, BLEND_Translucent, It->GetActorRotation().Yaw + 90.0f);
			if (OwningPS != NULL && OwningPS->Team != NULL)
			{
				Canvas->DrawColor = OwningPS->Team->TeamColor.ToFColor(false);
			}
			Canvas->DrawTile(PlayerStartIcon.Texture, Pos.X - IconSize * 0.25f, Pos.Y - IconSize * 0.25f, IconSize * 0.5f, IconSize * 0.5f, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL);
			if (OwningPS != NULL)
			{
				float XL, YL;
				FColor TextColor = Canvas->DrawColor;
				Canvas->DrawColor = FColor(0, 0, 0, 64);
				Canvas->TextSize(TinyFont, OwningPS->PlayerName, XL, YL);
				Canvas->DrawTile(SpawnHelpTextBG.Texture, Pos.X - XL * 0.5f, Pos.Y - 20.0f * RenderScale - 0.8f*YL, XL, 0.8f*YL, 149, 138, 32, 32, BLEND_Translucent);
				Canvas->DrawColor = TextColor;
				Canvas->DrawText(TinyFont, OwningPS->PlayerName, Pos.X - XL * 0.5f, Pos.Y - IconSize * 0.5f - 2.0f - YL);
			}
			Canvas->DrawColor = FColor::White;
		}
	}
	// draw pickup icons
	AUTPickup* NamedPickup = NULL;
	FVector2D NamedPickupPos = FVector2D::ZeroVector;
	for (TActorIterator<AUTPickup> It(GetWorld()); It; ++It)
	{
		FCanvasIcon Icon = It->GetMinimapIcon();
		if (Icon.Texture != NULL)
		{
			FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
			const float Ratio = Icon.UL / Icon.VL;
			Canvas->DrawColor = It->IconColor.ToFColor(false);
			const float IconSize = (LastHoveredActor == *It) ? (48.0f * RenderScale * FMath::InterpEaseOut<float>(1.0f, 1.25f, FMath::Min<float>(0.2f, GetWorld()->RealTimeSeconds - LastHoveredActorChangeTime) * 5.0f, 2.0f)) : (48.0f * RenderScale);
			Canvas->DrawTile(Icon.Texture, Pos.X - 0.5f * Ratio * IconSize, Pos.Y - 0.5f * IconSize, Ratio * IconSize, IconSize, Icon.U, Icon.V, Icon.UL, Icon.VL);
			if (LastHoveredActor == *It)
			{
				NamedPickup = *It;
				NamedPickupPos = Pos;
			}
		}
	}
	// draw name last so it is on top of any conflicting icons
	if (NamedPickup != NULL)
	{
		float XL, YL;
		Canvas->DrawColor = NamedPickup->IconColor.ToFColor(false);
		Canvas->TextSize(TinyFont, NamedPickup->GetDisplayName().ToString(), XL, YL);
		FColor TextColor = Canvas->DrawColor;
		Canvas->DrawColor = FColor(0, 0, 0, 64);
		Canvas->DrawTile(SpawnHelpTextBG.Texture, NamedPickupPos.X - XL * 0.5f, NamedPickupPos.Y - 26.0f * RenderScale - 0.8f*YL, XL, 0.8f*YL, 149, 138, 32, 32, BLEND_Translucent);
		Canvas->DrawColor = TextColor;
		Canvas->DrawText(TinyFont, NamedPickup->GetDisplayName(), NamedPickupPos.X - XL * 0.5f, NamedPickupPos.Y - 26.0f * RenderScale - YL);
	}
	Canvas->DrawColor = FColor::White;
}

void AUTHUD_Showdown::DrawHUD()
{
	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();
	bool bRealShowScores = bShowScores;
	bShowScores = bShowScores || (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && !GS->bStartedSpawnSelection);
	bool bDrewSpawnMap = false;
	if (KillIconWidget)
	{
		KillIconWidget->ScreenPosition = bShowScores ? FVector2D(0.52f, 0.635f) : FVector2D(0.0f, 0.0f);
		if (GS && GS->bFinalIntermissionDelay)
		{
			KillIconWidget->ClearMessages();
		}
	}
	if (!bShowScores)
	{
		if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && GS->bStartedSpawnSelection)
		{
			if (GS->bStartingSpawnSelection)
			{
				GS->bStartingSpawnSelection = false;
				UTPlayerOwner->ClientPlaySound(MapOpenSound);
			}
			bDrewSpawnMap = true;
			if (!bLockedLookInput)
			{
				PlayerOwner->SetIgnoreLookInput(true);
				bLockedLookInput = true;
			}

			AActor* NewHoveredActor = FindHoveredIconActor();
			if (NewHoveredActor != LastHoveredActor)
			{
				if (UTPlayerOwner != nullptr)
				{
					UTPlayerOwner->PlayMenuSelectSound();
				}
				LastHoveredActorChangeTime = GetWorld()->RealTimeSeconds;
				LastHoveredActor = NewHoveredActor;
			}

			if (MinimapTexture == NULL)
			{
				CreateMinimapTexture(); // because we're using the size below
			}
			const float MapSize = float(Canvas->SizeY) * 0.75f;
			DrawMinimap(FColor(164, 164, 164, 255), MapSize, FVector2D((Canvas->SizeX - MapSize) * 0.5f, (Canvas->SizeY - MapSize) * 0.5f));

			// draw preview for hovered spawn point
			if (!GS->bFinalIntermissionDelay)
			{
				APlayerStart* HoveredStart = Cast<APlayerStart>(LastHoveredActor);
				if (HoveredStart == PreviewPlayerStart)
				{
					if (HoveredStart != NULL)
					{
						if (bPendingSpawnPreview)
						{
							bPendingSpawnPreview = false;
						}
						else
						{
							// draw it
							const float Ratio = float(SpawnPreviewCapture->TextureTarget->SizeX) / float(SpawnPreviewCapture->TextureTarget->SizeY);
							FVector2D PreviewSize = FVector2D((Canvas->SizeX - MapSize) * 0.5f, (Canvas->SizeX - MapSize) * 0.5f / Ratio);
							FVector2D PreviewPos = FVector2D(Canvas->SizeX - PreviewSize.X - 0.01f*Canvas->SizeX, 0.01f*Canvas->SizeX);
							DrawTexture(SpawnHelpTextBG.Texture, PreviewPos.X - 0.05f*PreviewSize.X, PreviewPos.Y - 0.15f*PreviewSize.Y, 1.1f*PreviewSize.X, 4, 4, 2, 124, 8, FLinearColor::White);
							DrawTexture(SpawnHelpTextBG.Texture, PreviewPos.X - 0.05f*PreviewSize.X, PreviewPos.Y - 0.15f*PreviewSize.Y + 4, 1.1f*PreviewSize.X, 1.3f*PreviewSize.Y - 8, 4, 10, 124, 112, FLinearColor::White);
							DrawTexture(SpawnHelpTextBG.Texture, PreviewPos.X - 0.05f*PreviewSize.X, PreviewPos.Y + 1.15f*PreviewSize.Y - 4, 1.1f*PreviewSize.X, 4, 4, 122, 124, 8, FLinearColor::White);
							Canvas->DrawColor = WhiteColor;
							FVector2D Pos(WorldToMapToScreen(HoveredStart->GetActorLocation()));
							Canvas->DrawTile(SpawnHelpTextBG.Texture, PreviewPos.X - 4.f, PreviewPos.Y - 4.f, PreviewSize.X + 8.f, PreviewSize.Y + 8.f, 4, 4, 2, 124, BLEND_Opaque);
							DrawLine(Pos.X, Pos.Y, PreviewPos.X - 4.f, PreviewPos.Y - 4.f, FLinearColor::White);
							DrawLine(Pos.X, Pos.Y, PreviewPos.X - 4.f, PreviewPos.Y + PreviewSize.Y + 4.f, FLinearColor::White);
							Canvas->DrawTile(SpawnPreviewCapture->TextureTarget, PreviewPos.X, PreviewPos.Y, PreviewSize.X, PreviewSize.Y, 0.0f, 0.0f, SpawnPreviewCapture->TextureTarget->SizeX, SpawnPreviewCapture->TextureTarget->SizeY, BLEND_Opaque);
						}
					}
				}
				else
				{
					if (HoveredStart != NULL)
					{
						SpawnPreviewCapture->SetWorldLocationAndRotation(HoveredStart->GetActorLocation(), HoveredStart->GetActorRotation());
						bPendingSpawnPreview = true;
					}
					PreviewPlayerStart = HoveredStart;
				}
			}

			// move spectating camera to selected spot (if any)
			AUTPlayerState* OwnerPS = Cast<AUTPlayerState>(PlayerOwner->PlayerState);
			if (OwnerPS != NULL && OwnerPS->RespawnChoiceA != NULL && PlayerOwner->GetSpectatorPawn() != NULL)
			{
				PlayerOwner->GetSpectatorPawn()->TeleportTo(OwnerPS->RespawnChoiceA->GetActorLocation(), OwnerPS->RespawnChoiceA->GetActorRotation(), false, true);
				PlayerOwner->SetControlRotation(OwnerPS->RespawnChoiceA->GetActorRotation());
			}

			DrawPlayerList();
		}
		else
		{
			LastPlayerSelect = NULL;
			bNeedOnDeckNotify = true;
			if (bLockedLookInput)
			{
				PlayerOwner->SetIgnoreLookInput(false);
				bLockedLookInput = false;
			}
			if (GS && GS->bTeamGame && GS->GetMatchState() == MatchState::InProgress)
			{
				// draw pips for players alive on each team @TODO move to widget
				TArray<AUTPlayerState*> LivePlayers;
				int32 OldRedCount = RedPlayerCount;
				int32 OldBlueCount = BluePlayerCount;
				RedPlayerCount = 0;
				BluePlayerCount = 0;
				for (APlayerState* PS : GS->PlayerArray)
				{
					AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
					if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator && !UTPS->bOutOfLives && !UTPS->bIsInactive)
					{
						if (UTPS->Team->TeamIndex == 0)
						{
							RedPlayerCount++;
						}
						else
						{
							BluePlayerCount++;
						}
					}
				}
				if (OldRedCount > RedPlayerCount)
				{
					RedDeathTime = GetWorld()->GetTimeSeconds();
				}
				if (OldBlueCount > BluePlayerCount)
				{
					BlueDeathTime = GetWorld()->GetTimeSeconds();
				}

				float XAdjust = 0.03f * Canvas->ClipX * HUDWidgetScaleOverride;
				float PipSize = 0.025f * Canvas->ClipX * HUDWidgetScaleOverride;
				float XOffset = 0.5f * Canvas->ClipX - XAdjust - PipSize;
				float YOffset = 0.09f * Canvas->ClipY * HUDWidgetScaleOverride;

				Canvas->SetLinearDrawColor(FLinearColor::Red, 0.5f);
				for (int32 i = 0; i < RedPlayerCount; i++)
				{
					Canvas->DrawTile(PlayerStartIcon.Texture, XOffset, YOffset, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
					XOffset -= 1.2f*PipSize;
				}
				float TimeSinceRedDeath = GetWorld()->GetTimeSeconds() - RedDeathTime;
				if (TimeSinceRedDeath < 0.5f)
				{
					Canvas->SetLinearDrawColor(FLinearColor::Red, 0.5f - TimeSinceRedDeath);
					float ScaledSize = 1.f + 2.f*TimeSinceRedDeath;
					Canvas->DrawTile(PlayerStartIcon.Texture, XOffset - 0.5f*(ScaledSize - 1.f)*PipSize, YOffset - 0.5f*(ScaledSize - 1.f)*PipSize, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
				}

				XOffset = 0.5f * Canvas->ClipX + XAdjust;
				Canvas->SetLinearDrawColor(FLinearColor::Blue, 0.5f);
				for (int32 i = 0; i < BluePlayerCount; i++)
				{
					Canvas->DrawTile(PlayerStartIcon.Texture, XOffset, YOffset, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
					XOffset += 1.2f*PipSize;
				}
				float TimeSinceBlueDeath = GetWorld()->GetTimeSeconds() - BlueDeathTime;
				if (TimeSinceBlueDeath < 0.5f)
				{
					Canvas->SetLinearDrawColor(FLinearColor::Blue, 0.5f - TimeSinceBlueDeath);
					float ScaledSize = 1.f + 2.f*TimeSinceBlueDeath;
					Canvas->DrawTile(PlayerStartIcon.Texture, XOffset - 0.5f*(ScaledSize - 1.f)*PipSize, YOffset - 0.5f*(ScaledSize - 1.f)*PipSize, PipSize, PipSize, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL, BLEND_Translucent);
				}
			}
		}
	}

	bool bRealDrawMinimap = bDrawMinimap;
	bDrawMinimap = bDrawMinimap && !bDrewSpawnMap;
	Super::DrawHUD();
	bDrawMinimap = bRealDrawMinimap;
	bShowScores = bRealShowScores;

	if (bNeedOnDeckNotify && GS != NULL && GS->SpawnSelector == PlayerOwner->PlayerState)
	{
		bNeedOnDeckNotify = false;
		PlayerOwner->ClientReceiveLocalizedMessage(UUTShowdownGameMessage::StaticClass(), 2, PlayerOwner->PlayerState, NULL, NULL);
	}
}

void AUTHUD_Showdown::DrawPlayerList()
{
	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();
	TArray<AUTPlayerState*> LivePlayers;
	for (APlayerState* PS : GS->PlayerArray)
	{
		AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
		if (UTPS != NULL && UTPS->Team != NULL && !UTPS->bOnlySpectator)
		{
			LivePlayers.Add(UTPS);
		}
	}
	if (LastPlayerSelect != GS->SpawnSelector)
	{
		LastPlayerSelect = GS->SpawnSelector;
		if (UTPlayerOwner && UTPlayerOwner->GetViewTarget())
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), OtherSelectSound, UTPlayerOwner->GetViewTarget()->GetActorLocation(), 1.f, 1.0f, 0.0f);
		}
	}

	LivePlayers.Sort([](AUTPlayerState& A, AUTPlayerState& B){ return A.SelectionOrder < B.SelectionOrder; });

	float XPos = 0.005f*Canvas->ClipX;
	float YPos = Canvas->ClipY * 0.1f;
	Canvas->DrawColor = FColor(200, 200, 200, 80);
	FText Title = NSLOCTEXT("UnrealTournament", "SelectionOrder", "PICK ORDER");
	float XL, YL;
	Canvas->TextSize(MediumFont, Title.ToString(), XL, YL);
	XL = FMath::Max(XL, 1.f);
	Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos, YPos, 0.2f*Canvas->ClipX, YL, 149, 138, 32, 32, BLEND_Translucent);
	Canvas->DrawColor = FColor(255, 255, 255, 255);
	YPos += Canvas->DrawText(MediumFont, Title, 0.1f*Canvas->ClipX - 0.5f*XL, YPos) * 1.2f;
	for (AUTPlayerState* UTPS : LivePlayers)
	{
		UFont* NameFont = SmallFont;
		Canvas->TextSize(NameFont, UTPS->PlayerName, XL, YL);
		float TileWidth = 0.22f*Canvas->ClipX;
		float NameStart = 0.01f*Canvas->ClipX;
		float NameScale = FMath::Min(1.f, (TileWidth - NameStart + XPos) / XL);
		float ExtraWidth = NameStart;
		Canvas->DrawColor = UTPS->Team->TeamColor.ToFColor(false);
		Canvas->DrawColor.A = 70;
		Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos, YPos, TileWidth + ExtraWidth, YL, 149, 138, 32, 32, BLEND_Translucent);
		Canvas->DrawColor = FColor(255, 255, 255, 255);
		Canvas->DrawText(NameFont, UTPS->PlayerName, XPos+NameStart, YPos - 0.1f*YL, NameScale, 1.f);
		if (UTPS == GS->SpawnSelector)
		{
			Canvas->DrawColor = FColor(255, 255, 140, 80);
			Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos, YPos-0.1f*YL, TileWidth + ExtraWidth, 0.1f*YL, 149, 138, 32, 32, BLEND_Translucent);
			Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos, YPos+1.f*YL, TileWidth + ExtraWidth, 0.1f*YL, 149, 138, 32, 32, BLEND_Translucent);
			Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos + TileWidth + NameStart, YPos-0.1f*YL, 0.03f*Canvas->ClipX, 1.2f*YL, 149, 138, 32, 32, BLEND_Translucent);
			Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos-0.1f*YL, YPos-0.1f*YL, 0.1f*YL, 1.2f*YL, 149, 138, 32, 32, BLEND_Translucent);
			Canvas->DrawColor = FColor(255, 255, 255, 255);
			Canvas->DrawText(NameFont, *FText::AsNumber(GS->IntermissionStageTime).ToString(), XPos + TileWidth + 2.f*NameStart, YPos - 0.1f*YL);
		}
		YPos += YL * 1.3f;
	}
}

EInputMode::Type AUTHUD_Showdown::GetInputMode_Implementation() const
{
	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && GS->bStartedSpawnSelection && !GS->bFinalIntermissionDelay)
	{
		return EInputMode::EIM_GameAndUI;
	}
	else
	{
		return Super::GetInputMode_Implementation();
	}
}

AActor* AUTHUD_Showdown::FindHoveredIconActor() const
{
	AActor* BestHovered = NULL;
	float BestHoverDist = 40.f;
	if (GetInputMode() == EInputMode::EIM_GameAndUI)
	{
		FVector2D ClickPos;
		UTPlayerOwner->GetMousePosition(ClickPos.X, ClickPos.Y);
		APlayerStart* ClickedStart = NULL;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			AUTPlayerStart* UTStart = Cast<AUTPlayerStart>(*It);
			if (UTStart == NULL || !UTStart->bIgnoreInShowdown)
			{
				FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
				float NewHoverDist = (ClickPos - Pos).Size();
				if (NewHoverDist < BestHoverDist)
				{
					BestHovered = *It;
					BestHoverDist = NewHoverDist;
				}
			}
		}
		for (TActorIterator<AUTPickup> It(GetWorld()); It; ++It)
		{
			FCanvasIcon Icon = It->GetMinimapIcon();
			if (Icon.Texture != NULL)
			{
				FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
				float NewHoverDist = (ClickPos - Pos).Size();
				if (NewHoverDist < BestHoverDist)
				{
					BestHovered = *It;
					BestHoverDist = NewHoverDist;
				}
			}
		}
	}
	return BestHovered;
}

bool AUTHUD_Showdown::OverrideMouseClick(FKey Key, EInputEvent EventType)
{
	if (Key.GetFName() == EKeys::LeftMouseButton && EventType == IE_Pressed)
	{
		APlayerStart* ClickedStart = Cast<APlayerStart>(FindHoveredIconActor());
		if (ClickedStart != NULL)
		{
			if (UTPlayerOwner->GetViewTarget())
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), SpawnSelectSound, UTPlayerOwner->GetViewTarget()->GetActorLocation(), 1.f, 1.0f, 0.0f);
			}
			UTPlayerOwner->ServerSelectSpawnPoint(ClickedStart);
		}
		
		return true;
	}
	else
	{
		return false;
	}
}