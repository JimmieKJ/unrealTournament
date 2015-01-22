// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperTileMapRenderSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "PaperCustomVersion.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// UPaperTileMapComponent

UPaperTileMapComponent::UPaperTileMapComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	MapWidth_DEPRECATED = 4;
	MapHeight_DEPRECATED = 4;
	TileWidth_DEPRECATED = 32;
	TileHeight_DEPRECATED = 32;

	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("/Paper2D/DefaultSpriteMaterial"));
	Material_DEPRECATED = DefaultMaterial.Object;
}

FPrimitiveSceneProxy* UPaperTileMapComponent::CreateSceneProxy()
{
	FPaperTileMapRenderSceneProxy* Proxy = new FPaperTileMapRenderSceneProxy(this);
	RebuildRenderData(Proxy);

	return Proxy;
}

void UPaperTileMapComponent::PostInitProperties()
{
	TileMap = NewObject<UPaperTileMap>(this);
	TileMap->SetFlags(RF_Transactional);
	Super::PostInitProperties();
}

FBoxSphereBounds UPaperTileMapComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (TileMap != nullptr)
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = TileMap->GetRenderBounds().TransformBy(LocalToWorld);

		// Add bounds of collision geometry (if present).
		if (UBodySetup* BodySetup = TileMap->BodySetup)
		{
			const FBox AggGeomBox = BodySetup->AggGeom.CalcAABB(LocalToWorld);
			if (AggGeomBox.IsValid)
			{
				NewBounds = Union(NewBounds, FBoxSphereBounds(AggGeomBox));
			}
		}

		// Apply bounds scale
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UPaperTileMapComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Indicate we need to send new dynamic data.
	MarkRenderDynamicDataDirty();
}

#if WITH_EDITORONLY_DATA
void UPaperTileMapComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FPaperCustomVersion::GUID);
}

void UPaperTileMapComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FPaperCustomVersion::GUID) < FPaperCustomVersion::MovedTileMapDataToSeparateClass)
	{
		// Create a tile map object and move our old properties over to it
		TileMap = NewObject<UPaperTileMap>(this);
		TileMap->SetFlags(RF_Transactional);
		TileMap->MapWidth = MapWidth_DEPRECATED;
		TileMap->MapHeight = MapHeight_DEPRECATED;
		TileMap->TileWidth = TileWidth_DEPRECATED;
		TileMap->TileHeight = TileHeight_DEPRECATED;
		TileMap->PixelsPerUnit = 1.0f;
		TileMap->SelectedTileSet = DefaultLayerTileSet_DEPRECATED;
		TileMap->Material = Material_DEPRECATED;
		TileMap->TileLayers = TileLayers_DEPRECATED;

		// Convert the layers
		for (UPaperTileLayer* Layer : TileMap->TileLayers)
		{
			Layer->Rename(nullptr, TileMap, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
			Layer->ConvertToTileSetPerCell();
		}

		// Remove references in the deprecated variables to prevent issues with deleting referenced assets, etc...
		DefaultLayerTileSet_DEPRECATED = nullptr;
		Material_DEPRECATED = nullptr;
		TileLayers_DEPRECATED.Empty();
	}
#endif
}
#endif

UBodySetup* UPaperTileMapComponent::GetBodySetup()
{
	return (TileMap != nullptr) ? TileMap->BodySetup : nullptr;
}

const UObject* UPaperTileMapComponent::AdditionalStatObject() const
{
	if (TileMap != nullptr)
	{
		if (TileMap->GetOuter() != this)
		{
			return TileMap;
		}
	}

	return nullptr;
}

void UPaperTileMapComponent::RebuildRenderData(FPaperTileMapRenderSceneProxy* Proxy)
{
	TArray<FSpriteDrawCallRecord> BatchedSprites;
	
	if (TileMap == nullptr)
	{
		return;
	}


	FVector WorldOffsetNoY;
	FVector WorldOffsetYFactor = FVector::ZeroVector;
	FVector WorldStepX;
	FVector WorldStepY;

	switch (TileMap->ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		WorldOffsetNoY = -(TileMap->TileWidth * PaperAxisX * 0.5f) - (TileMap->TileHeight * PaperAxisY * 0.5f);
		WorldStepX = PaperAxisX * TileMap->TileWidth;
		WorldStepY = -PaperAxisY * TileMap->TileHeight;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		WorldOffsetNoY = (TileMap->MapWidth * TileMap->TileWidth * 0.5f * PaperAxisX);
		WorldOffsetYFactor = -(TileMap->TileHeight * 0.5f * PaperAxisY);
		WorldStepX = (TileMap->TileWidth * PaperAxisX * 0.5f) - (TileMap->TileHeight * PaperAxisY * 0.5f);
		WorldStepY = (TileMap->TileWidth * PaperAxisX * -0.5f) - (TileMap->TileHeight * PaperAxisY * 0.5f);
		break;
	case ETileMapProjectionMode::IsometricStaggered:
		WorldOffsetNoY = 0.5f * TileMap->TileHeight * PaperAxisY;
		WorldOffsetYFactor = 0.5f * TileMap->TileWidth * PaperAxisX;
		WorldStepX = PaperAxisX * TileMap->TileWidth;
		WorldStepY = -PaperAxisY * TileMap->TileHeight;
		break;
	}

	UTexture2D* LastSourceTexture = nullptr;
	FVector TileSetOffset = FVector::ZeroVector;
	FVector2D InverseTextureSize(1.0f, 1.0f);
	FVector2D SourceDimensionsUV(1.0f, 1.0f);
	FVector2D TileSizeXY(0.0f, 0.0f);

	for (int32 Z = 0; Z < TileMap->TileLayers.Num(); ++Z)
	{
		UPaperTileLayer* Layer = TileMap->TileLayers[Z];

		if (Layer == nullptr)
		{
			continue;
		}

		FLinearColor DrawColor = FLinearColor::White;
#if WITH_EDITORONLY_DATA
		if (Layer->bHiddenInEditor)
		{
			continue;
		}

		DrawColor.A = Layer->LayerOpacity;
#endif

		FSpriteDrawCallRecord* CurrentBatch = nullptr;

		for (int32 Y = 0; Y < TileMap->MapHeight; ++Y)
		{
			// In pixels
			FVector WorldOffset;

			switch (TileMap->ProjectionMode)
			{
			case ETileMapProjectionMode::Orthogonal:
			default:
				WorldOffset = WorldOffsetNoY;
				break;
			case ETileMapProjectionMode::IsometricDiamond:
				WorldOffset = WorldOffsetNoY + Y * WorldOffsetYFactor;
				break;
			case ETileMapProjectionMode::IsometricStaggered:
				WorldOffset = WorldOffsetNoY + (Y & 1) * WorldOffsetYFactor;
				break;
			}

			for (int32 X = 0; X < TileMap->MapWidth; ++X)
			{
				const FPaperTileInfo TileInfo = Layer->GetCell(X, Y);

				// do stuff
				const float TotalSeparation = (TileMap->SeparationPerLayer * Z) + (TileMap->SeparationPerTileX * X) + (TileMap->SeparationPerTileY * Y);
				FVector WorldPos = (WorldStepX * X) + (WorldStepY * Y) + WorldOffset;
				WorldPos += TotalSeparation * PaperAxisZ;

				const int32 TileWidth = TileMap->TileWidth;
				const int32 TileHeight = TileMap->TileHeight;


				{
					UTexture2D* SourceTexture = nullptr;

					FVector2D SourceUV = FVector2D::ZeroVector;
					if (Layer->bCollisionLayer)
					{
						if (TileInfo.PackedTileIndex == 0)
						{
							continue;
						}
						SourceTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;
					}
					else
					{
						if (TileInfo.TileSet == nullptr)
						{
							continue;
						}

						if (!TileInfo.TileSet->GetTileUV(TileInfo.PackedTileIndex, /*out*/ SourceUV))
						{
							continue;
						}

						SourceTexture = TileInfo.TileSet->TileSheet;
						if (SourceTexture == nullptr)
						{
							continue;
						}
					}

					if ((SourceTexture != LastSourceTexture) || (CurrentBatch == nullptr))
					{
						CurrentBatch = (new (BatchedSprites) FSpriteDrawCallRecord());
						CurrentBatch->Texture = SourceTexture;
						CurrentBatch->Color = DrawColor;
						CurrentBatch->Destination = WorldPos.ProjectOnTo(PaperAxisZ);
					}

					if (SourceTexture != LastSourceTexture)
					{
						InverseTextureSize = FVector2D(1.0f / SourceTexture->GetSizeX(), 1.0f / SourceTexture->GetSizeY());

						if (TileInfo.TileSet != nullptr)
						{
							SourceDimensionsUV = FVector2D(TileInfo.TileSet->TileWidth * InverseTextureSize.X, TileInfo.TileSet->TileHeight * InverseTextureSize.Y);
							TileSizeXY = FVector2D(TileInfo.TileSet->TileWidth, TileInfo.TileSet->TileHeight);
							TileSetOffset = (TileInfo.TileSet->DrawingOffset.X * PaperAxisX) + (TileInfo.TileSet->DrawingOffset.Y * PaperAxisY);
						}
						else
						{
							SourceDimensionsUV = FVector2D(TileWidth * InverseTextureSize.X, TileHeight * InverseTextureSize.Y);
							TileSizeXY = FVector2D(TileWidth, TileHeight);
							TileSetOffset = FVector::ZeroVector;
						}
						LastSourceTexture = SourceTexture;
					}
					WorldPos += TileSetOffset;

					SourceUV.X *= InverseTextureSize.X;
					SourceUV.Y *= InverseTextureSize.Y;

					FSpriteDrawCallRecord& NewTile = *CurrentBatch;

					const float WX0 = FVector::DotProduct(WorldPos, PaperAxisX);
					const float WY0 = FVector::DotProduct(WorldPos, PaperAxisY);

					const FVector4 BottomLeft(WX0, WY0, SourceUV.X, SourceUV.Y + SourceDimensionsUV.Y);
					const FVector4 BottomRight(WX0 + TileSizeXY.X, WY0, SourceUV.X + SourceDimensionsUV.X, SourceUV.Y + SourceDimensionsUV.Y);
					const FVector4 TopRight(WX0 + TileSizeXY.X, WY0 + TileSizeXY.Y, SourceUV.X + SourceDimensionsUV.X, SourceUV.Y);
					const FVector4 TopLeft(WX0, WY0 + TileSizeXY.Y, SourceUV.X, SourceUV.Y);

					new (NewTile.RenderVerts) FVector4(BottomLeft);
					new (NewTile.RenderVerts) FVector4(TopRight);
					new (NewTile.RenderVerts) FVector4(BottomRight);

					new (NewTile.RenderVerts) FVector4(BottomLeft);
					new (NewTile.RenderVerts) FVector4(TopLeft);
					new (NewTile.RenderVerts) FVector4(TopRight);
				}
			}
		}
	}

	Proxy->SetBatchesHack(BatchedSprites);
}

bool UPaperTileMapComponent::OwnsTileMap() const
{
	return (TileMap != nullptr) && (TileMap->GetOuter() == this);
}

#undef LOCTEXT_NAMESPACE