// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUD_Showdown.h"
#include "UTShowdownGameState.h"

AUTHUD_Showdown::AUTHUD_Showdown(const FObjectInitializer& OI)
: Super(OI)
{
	ConstructorHelpers::FObjectFinder<UTexture2D> PlayerStartTextureObject(TEXT("/Engine/EditorResources/S_Player"));
	PlayerStartTexture = PlayerStartTextureObject.Object;
	ConstructorHelpers::FObjectFinder<UTexture2D> SelectedSpawnTextureObject(TEXT("/Game/RestrictedAssets/Weapons/Sniper/Assets/TargetCircle.TargetCircle"));
	SelectedSpawnTexture = SelectedSpawnTextureObject.Object;
}

void AUTHUD_Showdown::DrawMinimap(const FColor& DrawColor, float MapSize, FVector2D DrawPos)
{
	Super::DrawMinimap(DrawColor, MapSize, DrawPos);

	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();

	const float RenderScale = float(Canvas->SizeY) / 1080.0f;
	// draw PlayerStart icons
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
		AUTTeamInfo* OwningTeam = NULL;
		for (APlayerState* PS : GS->PlayerArray)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL && UTPS->RespawnChoiceA == *It && UTPS->Team != NULL)
			{
				OwningTeam = UTPS->Team;
				break;
			}
		}

		if (OwningTeam != NULL || GS->IsAllowedSpawnPoint(GS->SpawnSelector, *It))
		{
			Canvas->DrawColor = FColor::White;
		}
		else
		{
			Canvas->DrawColor = FColor(128, 128, 128, 192);
		}
		Canvas->DrawTile(PlayerStartTexture, Pos.X - 16.0f * RenderScale, Pos.Y - 16.0f * RenderScale, 32.0f * RenderScale, 32.0f * RenderScale, 0.0f, 0.0f, PlayerStartTexture->GetSurfaceWidth(), PlayerStartTexture->GetSurfaceHeight());
		// draw circle on selected spawn points
		if (OwningTeam != NULL)
		{
			Canvas->DrawColor = OwningTeam->TeamColor.ToFColor(false);
			Canvas->DrawTile(SelectedSpawnTexture, Pos.X - 24.0f * RenderScale, Pos.Y - 24.0f * RenderScale, 48.0f * RenderScale, 48.0f * RenderScale, 0.0f, 0.0f, SelectedSpawnTexture->GetSurfaceWidth(), SelectedSpawnTexture->GetSurfaceHeight());
		}
		Canvas->DrawColor = FColor::White;
	}
	// draw pickup icons
	Canvas->DrawColor = FColor::White;
	for (TActorIterator<AUTPickup> It(GetWorld()); It; ++It)
	{
		if (It->HUDIcon.Texture != NULL)
		{
			FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
			const float Ratio = It->HUDIcon.UL / It->HUDIcon.VL;
			Canvas->DrawTile(It->HUDIcon.Texture, Pos.X - 16.0f * Ratio * RenderScale, Pos.Y - 16.0f * RenderScale, 32.0f * Ratio * RenderScale, 32.0f * RenderScale, It->HUDIcon.U, It->HUDIcon.V, It->HUDIcon.UL, It->HUDIcon.VL);
		}
	}
}

void AUTHUD_Showdown::DrawHUD()
{
	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && (GS->SpawnSelector != NULL || GS->bFinalIntermissionDelay))
	{
		if (!bLockedLookInput)
		{
			PlayerOwner->SetIgnoreLookInput(true);
			bLockedLookInput = true;
		}

		if (MinimapTexture == NULL)
		{
			CreateMinimapTexture(); // because we're using the size below
		}
		const float MapSize = float(Canvas->SizeY) * 0.75f;
		DrawMinimap(FColor(192, 192, 192, 255), MapSize, FVector2D((Canvas->SizeX - MapSize) * 0.5f, (Canvas->SizeY - MapSize) * 0.5f));
	}
	else if (bLockedLookInput)
	{
		PlayerOwner->SetIgnoreLookInput(false);
		bLockedLookInput = false;
	}

	if ( GS != NULL && !bShowScores && PlayerOwner->PlayerState != NULL && !PlayerOwner->PlayerState->bOnlySpectator &&
		((GS->bBroadcastPlayerHealth && GS->IsMatchInProgress()) || (GS->GetMatchState() == MatchState::MatchIntermission && !GS->bFinalIntermissionDelay)) )
	{
		// don't bother displaying if there are no live characters (e.g. match start)
		bool bAnyPawns = false;
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			if (Cast<AUTCharacter>(It->Get()) != NULL && !It->Get()->bTearOff)
			{
				bAnyPawns = true;
				break;
			}
		}

		if (bAnyPawns)
		{
			for (int32 i = 0; i < 2; i++)
			{
				float YPos = Canvas->ClipY * 0.05f;
				for (APlayerState* PS : GS->PlayerArray)
				{
					AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
					if (UTPS->GetTeamNum() == i)
					{
						float XL, YL;
						Canvas->DrawColor = UTPS->Team->TeamColor;
						Canvas->TextSize(MediumFont, UTPS->PlayerName, XL, YL);
						Canvas->DrawText(MediumFont, UTPS->PlayerName, i == 0 ? 1.0f : Canvas->SizeX - XL - 1.0f, YPos);
						YPos += YL;
						float HealthPct = 0.0f;
						float ArmorPct = 0.0f;
						for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
						{
							if (It->IsValid() && It->Get()->PlayerState == UTPS)
							{
								AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
								if (UTC != NULL)
								{
									HealthPct = float(UTC->Health) / float(UTC->SuperHealthMax);
									ArmorPct = float(UTC->ArmorAmount) / float(UTC->MaxStackedArmor);
									break;
								}
							}
						}
						YL = 26.0f;
						XL = 200.0f;
						float StartX = (i == 0) ? 0.0f : Canvas->SizeX - XL * 2.25f - 2.0f;
						// health BG
						Canvas->DrawColor = FColor(128, 128, 128, 192);
						Canvas->DrawTile(Canvas->DefaultTexture, StartX + 1.0f, YPos, XL, YL, 0.0f, 0.0f, 1.0f, 1.0f);
						// armor BG
						Canvas->DrawTile(Canvas->DefaultTexture, StartX + 1.0f + XL * 1.25f, YPos, XL, YL, 0.0f, 0.0f, 1.0f, 1.0f);
						// health bar
						Canvas->DrawColor = FColor::Green;
						Canvas->DrawTile(Canvas->DefaultTexture, StartX + 2.0f, YPos + 1.0f, (XL - 2.0f) * HealthPct, YL - 2.0f, 0.0f, 0.0f, 1.0f, 1.0f);
						// armor bar
						Canvas->DrawColor = FColor::Yellow;
						Canvas->DrawTile(Canvas->DefaultTexture, StartX + 2.0f + XL * 1.25f, YPos + 1.0f, (XL - 2.0f) * ArmorPct, YL - 2.0f, 0.0f, 0.0f, 1.0f, 1.0f);
						YPos += YL;
					}
				}
			}
		}
	}

	Super::DrawHUD();
}

EInputMode::Type AUTHUD_Showdown::GetInputMode_Implementation()
{
	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && GS->SpawnSelector == PlayerOwner->PlayerState)
	{
		return EInputMode::EIM_GameAndUI;
	}
	else
	{
		return Super::GetInputMode_Implementation();
	}
}

bool AUTHUD_Showdown::OverrideMouseClick(FKey Key, EInputEvent EventType)
{
	if (Key.GetFName() == EKeys::LeftMouseButton && EventType == IE_Pressed)
	{
		FVector2D ClickPos;
		UTPlayerOwner->GetMousePosition(ClickPos.X, ClickPos.Y);
		APlayerStart* ClickedStart = NULL;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			FVector2D Pos(WorldToMapToScreen(It->GetActorLocation()));
			if ((ClickPos - Pos).Size() < 32.0f)
			{
				ClickedStart = *It;
				break;
			}
		}
		if (ClickedStart != NULL)
		{
			UTPlayerOwner->ServerSelectSpawnPoint(ClickedStart);
		}
		
		return true;
	}
	else
	{
		return false;
	}
}