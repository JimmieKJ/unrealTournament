// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

	static ConstructorHelpers::FObjectFinder<USoundBase> PressedSelect(TEXT("SoundCue'/Game/RestrictedAssets/UI/UT99UI_BigSelect_Cue.UT99UI_BigSelect_Cue'"));
	SpawnSelectSound = PressedSelect.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> OtherSelect(TEXT("SoundCue'/Game/RestrictedAssets/Audio/UI/OtherSelect_Cue.OtherSelect_Cue'"));
	OtherSelectSound = OtherSelect.Object;

	SpawnPreviewCapture = OI.CreateDefaultSubobject<USceneCaptureComponent2D>(this, TEXT("SpawnPreviewCapture"));
	SpawnPreviewCapture->bCaptureEveryFrame = false;
	SpawnPreviewCapture->SetHiddenInGame(false);
	RootComponent = SpawnPreviewCapture; // just to quiet warning, has no relevance

	LastHoveredActorChangeTime = -1000.0f;
	bNeedOnDeckNotify = true;
}

void AUTHUD_Showdown::BeginPlay()
{
	Super::BeginPlay();

	SpawnPreviewCapture->TextureTarget = NewObject<UTextureRenderTarget2D>(this, UTextureRenderTarget2D::StaticClass());
	SpawnPreviewCapture->TextureTarget->InitCustomFormat(1280, 720, PF_B8G8R8A8, false);
	SpawnPreviewCapture->TextureTarget->ClearColor = FLinearColor::Black;
	SpawnPreviewCapture->RegisterComponent();
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
				Canvas->DrawTile(SpawnHelpTextBG.Texture, Pos.X - XL * 0.5f, Pos.Y - 20.0f * RenderScale - 0.8f*YL, 0.9f*XL, 0.8f*YL, 149, 138, 32, 32, BLEND_Translucent);
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
	KillIconWidget->ScreenPosition = bShowScores ? FVector2D(0.52f, 0.6f)  : FVector2D(0.0f, 0.0f);
	if (!bShowScores)
	{
		if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && GS->bStartedSpawnSelection)
		{
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
							FVector2D PreviewPos = FVector2D((0.95f*Canvas->SizeX + MapSize) * 0.5f, Canvas->SizeY * 0.2f);
							FVector2D PreviewSize = FVector2D((Canvas->SizeX - MapSize) * 0.5f, (Canvas->SizeX - MapSize) * 0.5f / Ratio);
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
					if (KillIconWidget)
					{
						KillIconWidget->ClearMessages();
					}
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
		}
	}
	bool bRealDrawMinimap = bDrawMinimap;
	bDrawMinimap = bDrawMinimap && !bDrewSpawnMap;
	Super::DrawHUD();
	bDrawMinimap = bRealDrawMinimap;
	bShowScores = bRealShowScores;

	if (GS != NULL && GS->SpawnSelector == PlayerOwner->PlayerState)
	{
		if (bNeedOnDeckNotify)
		{
			bNeedOnDeckNotify = false;
			PlayerOwner->ClientReceiveLocalizedMessage(UUTShowdownGameMessage::StaticClass(), 2, PlayerOwner->PlayerState, NULL, NULL);
		}

		// draw help text
		Canvas->DrawColor = WhiteColor;
		float TextScale = 1.0f;
		float XL, YL;
		Canvas->TextSize(MediumFont, TEXT("Click on the spawn point you want to use"), XL, YL, TextScale, TextScale);
		float IconSize = YL;
		XL += IconSize;
		Canvas->DrawTile(SpawnHelpTextBG.Texture, Canvas->SizeX * 0.5f - XL * 0.6f, Canvas->SizeY * 0.08f, XL * 1.2f, YL * 1.1f, SpawnHelpTextBG.U, SpawnHelpTextBG.V, SpawnHelpTextBG.UL, SpawnHelpTextBG.VL);
		float TextXPos = Canvas->SizeX * 0.5f - XL * 0.5f;
		Canvas->DrawText(MediumFont, TEXT("Click on the spawn point you want to use"), TextXPos, Canvas->SizeY * 0.08f, TextScale, TextScale);
		Canvas->DrawTile(PlayerStartBGIcon.Texture, TextXPos + XL - IconSize, Canvas->SizeY * 0.08f + YL * 0.1f, YL, YL, PlayerStartBGIcon.U, PlayerStartBGIcon.V, PlayerStartBGIcon.UL, PlayerStartBGIcon.VL);
		Canvas->DrawTile(PlayerStartIcon.Texture, TextXPos + XL - IconSize * 0.75f, Canvas->SizeY * 0.08f + YL * 0.1f + IconSize * 0.25f, YL * 0.5f, YL * 0.5f, PlayerStartIcon.U, PlayerStartIcon.V, PlayerStartIcon.UL, PlayerStartIcon.VL);
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

	if (LivePlayers.Num() > 2)
	{
		LivePlayers.Sort([](AUTPlayerState& A, AUTPlayerState& B){ return A.SelectionOrder < B.SelectionOrder; });

		float XPos = 6.f;
		float YPos = Canvas->ClipY * 0.1f;
		Canvas->DrawColor = FColor(200, 200, 200, 80);
		FText Title = NSLOCTEXT("UnrealTournament", "SelectionOrder", "PICK ORDER");
		float XL, YL;
		Canvas->TextSize(MediumFont, Title.ToString(), XL, YL);
		Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos, YPos, 0.2f*Canvas->ClipX, YL, 149, 138, 32, 32, BLEND_Translucent);
		Canvas->DrawColor = FColor(255, 255, 255, 255);
		YPos += Canvas->DrawText(MediumFont, Title, 0.1f*Canvas->ClipX - 0.5f*XL, YPos) * 1.2f;
		for (AUTPlayerState* UTPS : LivePlayers)
		{
			UFont* NameFont = (UTPS == GS->SpawnSelector) ? MediumFont : SmallFont;
			Canvas->TextSize(NameFont, UTPS->PlayerName, XL, YL);
			Canvas->DrawColor = (UTPS == GS->SpawnSelector) ? FColor(255, 255, 140, 80) : UTPS->Team->TeamColor.ToFColor(false);
			Canvas->DrawColor.A = 70;
			Canvas->DrawTile(SpawnHelpTextBG.Texture, XPos, YPos, FMath::Max(0.2f*Canvas->ClipX, 1.2f*XL), YL, 149, 138, 32, 32, BLEND_Translucent);
			Canvas->DrawColor = FColor(255, 255, 255, 255);
			Canvas->DrawText(NameFont, UTPS->PlayerName, XPos+0.1f*XL, YPos - 0.1f*YL);
			YPos += YL * 1.1f;
		}
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