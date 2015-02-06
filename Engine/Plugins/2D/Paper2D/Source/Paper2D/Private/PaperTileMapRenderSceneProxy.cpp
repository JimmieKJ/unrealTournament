// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperTileMapRenderSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodySetup2D.h"

//////////////////////////////////////////////////////////////////////////
// FPaperTileMapRenderSceneProxy

FPaperTileMapRenderSceneProxy::FPaperTileMapRenderSceneProxy(const UPaperTileMapComponent* InComponent)
	: FPaperRenderSceneProxy(InComponent)
	, TileMap(nullptr)
{
	if (const UPaperTileMapComponent* InTileComponent = Cast<const UPaperTileMapComponent>(InComponent))
	{
		TileMap = InTileComponent->TileMap;
		Material = InTileComponent->GetMaterial(0);
		MaterialRelevance = InTileComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());
	}
}

void FPaperTileMapRenderSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FPaperTileMapRenderSceneProxy_GetDynamicMeshElements);
	checkSlow(IsInRenderingThread());

	// Slight depth bias so that the wireframe grid overlay doesn't z-fight with the tiles themselves
	const float DepthBias = 0.0001f;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			// Draw the tile maps
			//@TODO: RenderThread race condition
			if (TileMap != nullptr)
			{
				FColor WireframeColor = FColor(0, 255, 255, 255);

				if ((View->Family->EngineShowFlags.Collision /*@TODO: && bIsCollisionEnabled*/) && AllowDebugViewmodes())
				{
					if (UBodySetup2D* BodySetup2D = Cast<UBodySetup2D>(TileMap->BodySetup))
					{
						//@TODO: Draw 2D debugging geometry
					}
					else if (UBodySetup* BodySetup = TileMap->BodySetup)
					{
						if (FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER)
						{
							// Catch this here or otherwise GeomTransform below will assert
							// This spams so commented out
							//UE_LOG(LogStaticMesh, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
						}
						else
						{
							// Make a material for drawing solid collision stuff
							const UMaterial* LevelColorationMaterial = View->Family->EngineShowFlags.Lighting
								? GEngine->ShadedLevelColorationLitMaterial : GEngine->ShadedLevelColorationUnlitMaterial;

							auto CollisionMaterialInstance = new FColoredMaterialRenderProxy(
								LevelColorationMaterial->GetRenderProxy(IsSelected(), IsHovered()),
								WireframeColor
								);

							// Draw the static mesh's body setup.

							// Get transform without scaling.
							FTransform GeomTransform(GetLocalToWorld());

							// In old wireframe collision mode, always draw the wireframe highlighted (selected or not).
							bool bDrawWireSelected = IsSelected();
							if (View->Family->EngineShowFlags.Collision)
							{
								bDrawWireSelected = true;
							}

							// Differentiate the color based on bBlockNonZeroExtent.  Helps greatly with skimming a level for optimization opportunities.
							FColor CollisionColor = FColor(157, 149, 223, 255);

							const bool bPerHullColor = false;
							const bool bDrawSimpleSolid = false;
							BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, bDrawWireSelected, IsHovered()), CollisionMaterialInstance, bPerHullColor, bDrawSimpleSolid, UseEditorDepthTest(), ViewIndex, Collector);
						}
					}
				}

				// Draw the bounds
				RenderBounds(PDI, View->Family->EngineShowFlags, GetBounds(), IsSelected());

#if WITH_EDITOR
				// Draw the debug outline
				if (View->Family->EngineShowFlags.Grid)
				{
					const uint8 DPG = SDPG_Foreground;//GetDepthPriorityGroup(View);

					// Draw separation wires if selected
					FLinearColor OverrideColor;
					bool bUseOverrideColor = false;

					const bool bShowAsSelected = !(GIsEditor && View->Family->EngineShowFlags.Selection) || IsSelected();
					if (bShowAsSelected || IsHovered())
					{
						bUseOverrideColor = true;
						OverrideColor = GetSelectionColor(FLinearColor::White, bShowAsSelected, IsHovered());
					}

					FTransform LocalToWorld(GetLocalToWorld());

					if (bUseOverrideColor)
					{
						const int32 SelectedLayerIndex = TileMap->SelectedLayerIndex;

						// Draw a bound for any invisible layers
						for (int32 LayerIndex = 0; LayerIndex < TileMap->TileLayers.Num(); ++LayerIndex)
						{
							if (LayerIndex != SelectedLayerIndex)
							{
								const FVector TL(LocalToWorld.TransformPosition(TileMap->GetTilePositionInLocalSpace(0, 0, LayerIndex)));
								const FVector TR(LocalToWorld.TransformPosition(TileMap->GetTilePositionInLocalSpace(TileMap->MapWidth, 0, LayerIndex)));
								const FVector BL(LocalToWorld.TransformPosition(TileMap->GetTilePositionInLocalSpace(0, TileMap->MapHeight, LayerIndex)));
								const FVector BR(LocalToWorld.TransformPosition(TileMap->GetTilePositionInLocalSpace(TileMap->MapWidth, TileMap->MapHeight, LayerIndex)));

								PDI->DrawLine(TL, TR, OverrideColor, DPG, 0.0f, DepthBias);
								PDI->DrawLine(TR, BR, OverrideColor, DPG, 0.0f, DepthBias);
								PDI->DrawLine(BR, BL, OverrideColor, DPG, 0.0f, DepthBias);
								PDI->DrawLine(BL, TL, OverrideColor, DPG, 0.0f, DepthBias);
							}
						}

						if (SelectedLayerIndex != INDEX_NONE)
						{
							// Draw horizontal lines on the selection
							for (int32 Y = 0; Y <= TileMap->MapHeight; ++Y)
							{
								int32 X = 0;
								const FVector Start(TileMap->GetTilePositionInLocalSpace(X, Y, SelectedLayerIndex));

								X = TileMap->MapWidth;
								const FVector End(TileMap->GetTilePositionInLocalSpace(X, Y, SelectedLayerIndex));

								PDI->DrawLine(LocalToWorld.TransformPosition(Start), LocalToWorld.TransformPosition(End), OverrideColor, DPG, 0.0f, DepthBias);
							}

							// Draw vertical lines
							for (int32 X = 0; X <= TileMap->MapWidth; ++X)
							{
								int32 Y = 0;
								const FVector Start(TileMap->GetTilePositionInLocalSpace(X, Y, SelectedLayerIndex));

								Y = TileMap->MapHeight;
								const FVector End(TileMap->GetTilePositionInLocalSpace(X, Y, SelectedLayerIndex));

								PDI->DrawLine(LocalToWorld.TransformPosition(Start), LocalToWorld.TransformPosition(End), OverrideColor, DPG, 0.0f, DepthBias);
							}
						}
					}
				}
#endif
			}
		}
	}

	// Draw all of the queued up sprites
	FPaperRenderSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
}

