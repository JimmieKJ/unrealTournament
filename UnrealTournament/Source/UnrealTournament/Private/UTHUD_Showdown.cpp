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

void AUTHUD_Showdown::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer) // FIXME: shouldn't have HUD on server
	{
		MinimapTexture = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), 1024, 1024);
		MinimapTexture->ClearColor = FLinearColor::Black;//FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		MinimapTexture->OnCanvasRenderTargetUpdate.AddDynamic(this, &AUTHUD_Showdown::UpdateMinimapTexture);
		MinimapTexture->UpdateResource();
	}
}

void AUTHUD_Showdown::CalcMinimapTransform(const FBox& LevelBox, int32 MapWidth, int32 MapHeight)
{
	const bool bLargerXAxis = LevelBox.GetExtent().X > LevelBox.GetExtent().Y;
	const float LevelRadius = bLargerXAxis ? LevelBox.GetExtent().X : LevelBox.GetExtent().Y;
	const float ScaleFactor = float(MapWidth) / (LevelRadius * 2.0f);
	const FVector CenteringAdjust = bLargerXAxis ? FVector(0.0f, (LevelBox.GetExtent().X - LevelBox.GetExtent().Y), 0.0f) : FVector((LevelBox.GetExtent().Y - LevelBox.GetExtent().X), 0.0f, 0.0f);
	MinimapTransform = FTranslationMatrix(-LevelBox.Min + CenteringAdjust) * FScaleMatrix(FVector(ScaleFactor));
}

void AUTHUD_Showdown::UpdateMinimapTexture(UCanvas* C, int32 Width, int32 Height)
{
	FBox LevelBox(0);
	AUTRecastNavMesh* NavMesh = GetUTNavData(GetWorld());
	if (NavMesh != NULL)
	{
		TMap<const UUTPathNode*, FNavMeshTriangleList> TriangleMap;
		NavMesh->GetNodeTriangleMap(TriangleMap);
		// calculate a bounding box for the level
		for (TMap<const UUTPathNode*, FNavMeshTriangleList>::TConstIterator It(TriangleMap); It; ++It)
		{
			const FNavMeshTriangleList& TriList = It.Value();
			for (const FVector& Vert : TriList.Verts)
			{
				LevelBox += Vert;
			}
		}
		LevelBox = LevelBox.ExpandBy(LevelBox.GetSize() * 0.01f); // extra so edges aren't right up against the texture
		CalcMinimapTransform(LevelBox, Width, Height);
		for (TMap<const UUTPathNode*, FNavMeshTriangleList>::TConstIterator It(TriangleMap); It; ++It)
		{
			const FNavMeshTriangleList& TriList = It.Value();
			
			for (const FNavMeshTriangleList::FTriangle& Tri : TriList.Triangles)
			{
				FCanvasTriangleItem Item(FVector2D(MinimapTransform.TransformPosition(TriList.Verts[Tri.Indices[0]])), FVector2D(MinimapTransform.TransformPosition(TriList.Verts[Tri.Indices[1]])), FVector2D(MinimapTransform.TransformPosition(TriList.Verts[Tri.Indices[2]])), C->DefaultTexture->Resource);
				C->DrawItem(Item);
			}
		}
	}
	else
	{
		// set minimap scale based on colliding geometry so map has some functionality without a working navmesh
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			TArray<UPrimitiveComponent*> Components;
			It->GetComponents(Components);
			for (UPrimitiveComponent* Prim : Components)
			{
				if (Prim->IsCollisionEnabled())
				{
					LevelBox += Prim->Bounds.GetBox();
				}
			}
		}
		LevelBox = LevelBox.ExpandBy(LevelBox.GetSize() * 0.01f); // extra so edges aren't right up against the texture
		CalcMinimapTransform(LevelBox, Width, Height);
	}
}

void AUTHUD_Showdown::DrawHUD()
{
	Super::DrawHUD();

	AUTShowdownGameState* GS = GetWorld()->GetGameState<AUTShowdownGameState>();
	if (GS != NULL && GS->GetMatchState() == MatchState::MatchIntermission && (GS->SpawnSelector != NULL || GS->bFinalIntermissionDelay))
	{
		if (!bLockedLookInput)
		{
			PlayerOwner->SetIgnoreLookInput(true);
			bLockedLookInput = true;
		}

		const float MapSize = float(Canvas->SizeY) * 0.75f;
		MapToScreen = FTranslationMatrix(FVector((Canvas->SizeX - MapSize) * 0.5f * (MinimapTexture->GetSurfaceWidth() / MapSize), (Canvas->SizeY - MapSize) * 0.5f, 0.0f)) * FScaleMatrix(FVector(MapSize / MinimapTexture->GetSurfaceWidth(), MapSize / MinimapTexture->GetSurfaceHeight(), 1.0f));
		if (MinimapTexture != NULL)
		{
			Canvas->DrawTile(MinimapTexture, MapToScreen.GetOrigin().X, MapToScreen.GetOrigin().Y, MapSize, MapSize, 0.0f, 0.0f, MinimapTexture->GetSurfaceWidth(), MinimapTexture->GetSurfaceHeight());
		}
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
	else if (bLockedLookInput)
	{
		PlayerOwner->SetIgnoreLookInput(false);
		bLockedLookInput = false;
	}

	if (GS != NULL && GS->bBroadcastPlayerHealth && !bShowScores && GS->IsMatchInProgress())
	{
		for (int32 i = 0; i < 2; i++)
		{
			float YPos = 1.0f;
			for (APlayerState* PS : GS->PlayerArray)
			{
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
				if (UTPS->GetTeamNum() == i)
				{
					float XL, YL;
					Canvas->DrawColor = UTPS->Team->TeamColor;
					Canvas->TextSize(LargeFont, UTPS->PlayerName, XL, YL);
					Canvas->DrawText(LargeFont, UTPS->PlayerName, i == 0 ? 1.0f : Canvas->SizeX - XL - 1.0f, YPos);
					YPos += YL;
					int32 Health = 0;
					int32 Armor = 0;
					for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
					{
						if (It->IsValid() && It->Get()->PlayerState == UTPS)
						{
							AUTCharacter* UTC = Cast<AUTCharacter>(It->Get());
							if (UTC != NULL)
							{
								Health = UTC->Health;
								Armor = UTC->ArmorAmount;
								break;
							}
						}
					}
					FString StatusText = FString::Printf(TEXT("HEALTH: %i | ARMOR: %i"), Health, Armor);
					Canvas->TextSize(LargeFont, StatusText, XL, YL);
					Canvas->DrawText(LargeFont, StatusText, i == 0 ? 1.0f : Canvas->SizeX - XL - 1.0f, YPos);
					YPos += YL;
				}
			}
		}
	}
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