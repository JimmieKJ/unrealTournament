// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "SpriteEditorViewportClient.h"
#include "SceneViewport.h"

#include "PreviewScene.h"
#include "ScopedTransaction.h"
#include "Runtime/Engine/Public/ComponentReregisterContext.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"

#define LOCTEXT_NAMESPACE "SpriteEditor"

//////////////////////////////////////////////////////////////////////////
// Polygon Vertex Hashing

static int32 GetPolygonVertexHash(int32 PolygonIndex, int32 VertexIndex)
{
	check(PolygonIndex <= 0x7fff && VertexIndex < 0xffff);
	return (PolygonIndex << 16) + VertexIndex;
}

static int32 GetPolygonIndexFromHash(int32 PolygonVertexHash)
{
	return PolygonVertexHash >> 16;
}

static int32 GetVertexIndexFromHash(int32 PolygonVertexHash)
{
	return PolygonVertexHash & 0xffff;
}

//////////////////////////////////////////////////////////////////////////
// FSelectionTypes

const FName FSelectionTypes::Vertex(TEXT("Vertex"));
const FName FSelectionTypes::Edge(TEXT("Edge"));
const FName FSelectionTypes::Pivot(TEXT("Pivot"));
const FName FSelectionTypes::Socket(TEXT("Socket"));
const FName FSelectionTypes::SourceRegion(TEXT("SourceRegion"));

//////////////////////////////////////////////////////////////////////////
// HSpriteSelectableObjectHitProxy

struct HSpriteSelectableObjectHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( PAPER2DEDITOR_API );

	TSharedPtr<FSelectedItem> Data;

	HSpriteSelectableObjectHitProxy(TSharedPtr<FSelectedItem> InData)
		: HHitProxy(HPP_UI)
		, Data(InData)
	{
	}
};
IMPLEMENT_HIT_PROXY(HSpriteSelectableObjectHitProxy, HHitProxy);

//////////////////////////////////////////////////////////////////////////
// FSpriteEditorViewportClient

FSpriteEditorViewportClient::FSpriteEditorViewportClient(TWeakPtr<FSpriteEditor> InSpriteEditor, TWeakPtr<class SSpriteEditorViewport> InSpriteEditorViewportPtr)
	: CurrentMode(ESpriteEditorMode::ViewMode)
	, SpriteEditorPtr(InSpriteEditor)
	, SpriteEditorViewportPtr(InSpriteEditorViewportPtr)
{
	check(SpriteEditorPtr.IsValid() && SpriteEditorViewportPtr.IsValid());

	PreviewScene = &OwnedPreviewScene;

	SetRealtime(true);

	WidgetMode = FWidget::WM_Translate;
	bManipulating = false;
	bManipulationDirtiedSomething = false;
	ScopedTransaction = nullptr;

	bShowSourceTexture = false;
	bShowSockets = true;
	bShowNormals = true;
	bShowPivot = true;
	bShowRelatedSprites = true;

	bDeferZoomToSprite = true;

	bIsMarqueeTracking = false;

	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.CompositeEditorPrimitives = true;

	// Create a render component for the sprite being edited
	{
		RenderSpriteComponent = NewObject<UPaperSpriteComponent>();
		UPaperSprite* Sprite = GetSpriteBeingEdited();
		RenderSpriteComponent->SetSprite(Sprite);

		PreviewScene->AddComponent(RenderSpriteComponent, FTransform::Identity);
	}

	// Create a sprite and render component for the source texture view
	{
		UPaperSprite* DummySprite = NewObject<UPaperSprite>();
		DummySprite->SpriteCollisionDomain = ESpriteCollisionMode::None;
		DummySprite->PivotMode = ESpritePivotMode::Bottom_Left;

		SourceTextureViewComponent = NewObject<UPaperSpriteComponent>();
		SourceTextureViewComponent->SetSprite(DummySprite);
		UpdateSourceTextureSpriteFromSprite(GetSpriteBeingEdited());

		// Nudge the source texture view back a bit so it doesn't occlude sprites
		const FTransform Transform(-1.0f * PaperAxisZ);
		SourceTextureViewComponent->bVisible = false;
		PreviewScene->AddComponent(SourceTextureViewComponent, Transform);
	}
}

void FSpriteEditorViewportClient::UpdateSourceTextureSpriteFromSprite(UPaperSprite* SourceSprite)
{
	UPaperSprite* TargetSprite = SourceTextureViewComponent->GetSprite();
	check(TargetSprite);

	if (SourceSprite != nullptr)
	{
		if ((SourceSprite->GetSourceTexture() != TargetSprite->GetSourceTexture()) || (TargetSprite->PixelsPerUnrealUnit != SourceSprite->PixelsPerUnrealUnit))
		{
			FComponentReregisterContext ReregisterSprite(SourceTextureViewComponent);

			FSpriteAssetInitParameters SpriteReinitParams;
			SpriteReinitParams.SetTextureAndFill(SourceSprite->SourceTexture);
			TargetSprite->PixelsPerUnrealUnit = SourceSprite->PixelsPerUnrealUnit;
			TargetSprite->InitializeSprite(SpriteReinitParams);

			bDeferZoomToSprite = true;
		}


		// Position the sprite for the mode its meant to be in
		FVector2D CurrentPivotPosition;
		ESpritePivotMode::Type CurrentPivotMode = TargetSprite->GetPivotMode(/*out*/CurrentPivotPosition);

		FVector Translation(1.0f * PaperAxisZ);
		if (IsInSourceRegionEditMode())
		{
			if (CurrentPivotMode != ESpritePivotMode::Bottom_Left)
			{
				TargetSprite->SetPivotMode(ESpritePivotMode::Bottom_Left, FVector2D::ZeroVector);
				TargetSprite->PostEditChange();
			}
			SourceTextureViewComponent->SetSpriteColor(FLinearColor::White);
			SourceTextureViewComponent->SetWorldTransform(FTransform(Translation));
		}
		else
		{
			// Tint the source texture darker to help distinguish the two
			FLinearColor DarkTintColor(0.05f, 0.05f, 0.05f, 1.0f);

			FVector2D PivotPosition = SourceSprite->GetPivotPosition();
			if (CurrentPivotMode != ESpritePivotMode::Custom || CurrentPivotPosition != PivotPosition)
			{
				TargetSprite->SetPivotMode(ESpritePivotMode::Custom, PivotPosition);
				TargetSprite->PostEditChange();
			}
			SourceTextureViewComponent->SetSpriteColor(DarkTintColor);

			bool bRotated = SourceSprite->IsRotatedInSourceImage();
			if (bRotated)
			{
				FQuat Rotation(PaperAxisZ, FMath::DegreesToRadians(90.0f));
				SourceTextureViewComponent->SetWorldTransform(FTransform(Rotation, Translation));
			}
			else
			{
				SourceTextureViewComponent->SetWorldTransform(FTransform(Translation));
			}
		}
	}
	else
	{
		// No source sprite, so don't draw the target either
		TargetSprite->SourceTexture = nullptr;
	}
}

FVector2D FSpriteEditorViewportClient::TextureSpaceToScreenSpace(const FSceneView& View, const FVector2D& SourcePoint) const
{
	const FVector WorldSpacePoint = TextureSpaceToWorldSpace(SourcePoint);

	FVector2D PixelLocation;
	View.WorldToPixel(WorldSpacePoint, /*out*/ PixelLocation);
	return PixelLocation;
}

FVector FSpriteEditorViewportClient::TextureSpaceToWorldSpace(const FVector2D& SourcePoint) const
{
	UPaperSprite* Sprite = RenderSpriteComponent->GetSprite();
	return Sprite->ConvertTextureSpaceToWorldSpace(SourcePoint);
}

FVector2D FSpriteEditorViewportClient::SourceTextureSpaceToScreenSpace(const FSceneView& View, const FVector2D& SourcePoint) const
{
	const FVector WorldSpacePoint = SourceTextureSpaceToWorldSpace(SourcePoint);

	FVector2D PixelLocation;
	View.WorldToPixel(WorldSpacePoint, /*out*/ PixelLocation);
	return PixelLocation;
}

FVector FSpriteEditorViewportClient::SourceTextureSpaceToWorldSpace(const FVector2D& SourcePoint) const
{
	UPaperSprite* Sprite = SourceTextureViewComponent->GetSprite();
	return Sprite->ConvertTextureSpaceToWorldSpace(SourcePoint);
}

void FSpriteEditorViewportClient::DrawTriangleList(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const TArray<FVector2D>& Triangles)
{
	// Draw the collision data (non-interactive)
	const FLinearColor BakedCollisionRenderColor(1.0f, 1.0f, 0.0f, 0.5f);
	float BakedCollisionVertexSize = 3.0f;

	// Draw the vertices
	for (int32 VertexIndex = 0; VertexIndex < Triangles.Num(); ++VertexIndex)
	{
		const FVector2D VertexPos(TextureSpaceToScreenSpace(View, Triangles[VertexIndex]));
		Canvas.DrawTile(VertexPos.X - BakedCollisionVertexSize*0.5f, VertexPos.Y - BakedCollisionVertexSize*0.5f, BakedCollisionVertexSize, BakedCollisionVertexSize, 0.f, 0.f, 1.f, 1.f, BakedCollisionRenderColor, GWhiteTexture);
	}

	// Draw the lines
	const FLinearColor BakedCollisionLineRenderColor(1.0f, 1.0f, 0.0f, 0.25f);
	for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num() / 3; ++TriangleIndex)
	{
		for (int32 Offset = 0; Offset < 3; ++Offset)
		{
			const int32 VertexIndex = (TriangleIndex * 3) + Offset;
			const int32 NextVertexIndex = (TriangleIndex * 3) + ((Offset + 1) % 3);

			const FVector2D VertexPos(TextureSpaceToScreenSpace(View, Triangles[VertexIndex]));
			const FVector2D NextVertexPos(TextureSpaceToScreenSpace(View, Triangles[NextVertexIndex]));

			FCanvasLineItem LineItem(VertexPos, NextVertexPos);
			LineItem.SetColor(BakedCollisionLineRenderColor);
			Canvas.DrawItem(LineItem);
		}
	}
}

void FSpriteEditorViewportClient::DrawRelatedSprites(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const FLinearColor& LineColor)
{
	for (int SpriteIndex = 0; SpriteIndex < RelatedSprites.Num(); ++SpriteIndex)
	{
		FRelatedSprite& RelatedSprite = RelatedSprites[SpriteIndex];
		const FVector2D& SourceUV = RelatedSprite.SourceUV;
		const FVector2D& SourceDimension = RelatedSprite.SourceDimension;
		FVector2D BoundsVertices[4];
		BoundsVertices[0] = SourceTextureSpaceToScreenSpace(View, SourceUV);
		BoundsVertices[1] = SourceTextureSpaceToScreenSpace(View, SourceUV + FVector2D(SourceDimension.X, 0));
		BoundsVertices[2] = SourceTextureSpaceToScreenSpace(View, SourceUV + FVector2D(SourceDimension.X, SourceDimension.Y));
		BoundsVertices[3] = SourceTextureSpaceToScreenSpace(View, SourceUV + FVector2D(0, SourceDimension.Y));
		for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
		{
			const int32 NextVertexIndex = (VertexIndex + 1) % 4;

			FCanvasLineItem LineItem(BoundsVertices[VertexIndex], BoundsVertices[NextVertexIndex]);
			LineItem.SetColor(LineColor);
			Canvas.DrawItem(LineItem);
		}
	}
}

void FSpriteEditorViewportClient::DrawSourceRegion(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const FLinearColor& GeometryVertexColor)
{
	const bool bIsHitTesting = Canvas.IsHitTesting();
	UPaperSprite* Sprite = GetSpriteBeingEdited();

    const float CornerCollisionVertexSize = 8.0f;
	const float EdgeCollisionVertexSize = 6.0f;

	const float NormalLength = 15.0f;
	const FLinearColor GeometryLineColor(GeometryVertexColor.R, GeometryVertexColor.G, GeometryVertexColor.B, 0.5f * GeometryVertexColor.A);
	const FLinearColor GeometryNormalColor(0.0f, 1.0f, 0.0f, 0.5f);
    
    const bool bDrawEdgeHitProxies = true;
    const bool bDrawCornerHitProxies = true;

	FVector2D BoundsVertices[4];
	BoundsVertices[0] = SourceTextureSpaceToScreenSpace(View, Sprite->SourceUV);
	BoundsVertices[1] = SourceTextureSpaceToScreenSpace(View, Sprite->SourceUV + FVector2D(Sprite->SourceDimension.X, 0));
	BoundsVertices[2] = SourceTextureSpaceToScreenSpace(View, Sprite->SourceUV + FVector2D(Sprite->SourceDimension.X, Sprite->SourceDimension.Y));
	BoundsVertices[3] = SourceTextureSpaceToScreenSpace(View, Sprite->SourceUV + FVector2D(0, Sprite->SourceDimension.Y));
	for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
	{
		const int32 NextVertexIndex = (VertexIndex + 1) % 4;

		FCanvasLineItem LineItem(BoundsVertices[VertexIndex], BoundsVertices[NextVertexIndex]);
		LineItem.SetColor(GeometryVertexColor);
		Canvas.DrawItem(LineItem);

		FVector2D MidPoint = (BoundsVertices[VertexIndex] + BoundsVertices[NextVertexIndex]) * 0.5f;
        FVector2D CornerPoint = BoundsVertices[VertexIndex];

        // Add edge hit proxy
        if (bDrawEdgeHitProxies)
        {
            if (bIsHitTesting)
            {
                TSharedPtr<FSpriteSelectedSourceRegion> Data = MakeShareable(new FSpriteSelectedSourceRegion());
                Data->SpritePtr = Sprite;
                Data->VertexIndex = 4 + VertexIndex;
                Canvas.SetHitProxy(new HSpriteSelectableObjectHitProxy(Data));
            }

            Canvas.DrawTile(MidPoint.X - EdgeCollisionVertexSize*0.5f, MidPoint.Y - EdgeCollisionVertexSize*0.5f, EdgeCollisionVertexSize, EdgeCollisionVertexSize, 0.f, 0.f, 1.f, 1.f, GeometryVertexColor, GWhiteTexture);

            if (bIsHitTesting)
            {
                Canvas.SetHitProxy(nullptr);
            }
        }

        // Add corner hit proxy
        if (bDrawCornerHitProxies)
        {
            if (bIsHitTesting)
            {
                TSharedPtr<FSpriteSelectedSourceRegion> Data = MakeShareable(new FSpriteSelectedSourceRegion());
                Data->SpritePtr = Sprite;
                Data->VertexIndex = VertexIndex;
                Canvas.SetHitProxy(new HSpriteSelectableObjectHitProxy(Data));
            }
            
            Canvas.DrawTile(CornerPoint.X - CornerCollisionVertexSize * 0.5f, CornerPoint.Y - CornerCollisionVertexSize * 0.5f, CornerCollisionVertexSize, CornerCollisionVertexSize, 0.f, 0.f, 1.f, 1.f, GeometryVertexColor, GWhiteTexture);

            if (bIsHitTesting)
            {
                Canvas.SetHitProxy(nullptr);
            }
        }
	}
}

void FSpriteEditorViewportClient::DrawMarquee(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const FLinearColor& MarqueeColor)
{
	FVector2D MarqueeDelta = MarqueeEndPos - MarqueeStartPos;
	FVector2D MarqueeVertices[4];
	MarqueeVertices[0] = FVector2D(MarqueeStartPos.X, MarqueeStartPos.Y);
	MarqueeVertices[1] = MarqueeVertices[0] + FVector2D(MarqueeDelta.X, 0);
	MarqueeVertices[2] = MarqueeVertices[0] + FVector2D(MarqueeDelta.X, MarqueeDelta.Y);
	MarqueeVertices[3] = MarqueeVertices[0] + FVector2D(0, MarqueeDelta.Y);
	for (int32 MarqueeVertexIndex = 0; MarqueeVertexIndex < 4; ++MarqueeVertexIndex)
	{
		const int32 NextVertexIndex = (MarqueeVertexIndex + 1) % 4;
		FCanvasLineItem MarqueeLine(MarqueeVertices[MarqueeVertexIndex], MarqueeVertices[NextVertexIndex]);
		MarqueeLine.SetColor(MarqueeColor);
		Canvas.DrawItem(MarqueeLine);
	}
}

void FSpriteEditorViewportClient::DrawGeometry(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const FSpritePolygonCollection& Geometry, const FLinearColor& GeometryVertexColor, const FLinearColor& NegativeGeometryVertexColor, bool bIsRenderGeometry)
{
	const bool bIsHitTesting = Canvas.IsHitTesting();
	UPaperSprite* Sprite = GetSpriteBeingEdited();

	const float CollisionVertexSize = 8.0f;

	const float NormalLength = 15.0f;
	const FLinearColor GeometryNormalColor(0.0f, 1.0f, 0.0f, 0.5f);
	const FLinearColor SelectedColor(1, 1, 1, 1);

	// Run thru the custom collision vertices and draw hit proxies for them
	for (int32 PolygonIndex = 0; PolygonIndex < Geometry.Polygons.Num(); ++PolygonIndex)
	{
		const FSpritePolygon& Polygon = Geometry.Polygons[PolygonIndex];
		const FLinearColor LineColor = Polygon.bNegativeWinding ? NegativeGeometryVertexColor : GeometryVertexColor;
		const FLinearColor VertexColor = Polygon.bNegativeWinding ? NegativeGeometryVertexColor : GeometryVertexColor;

		// Draw lines connecting the vertices of the polygon
		for (int32 VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
		{
			const int32 NextVertexIndex = (VertexIndex + 1) % Polygon.Vertices.Num();

			const FVector2D ScreenPos = TextureSpaceToScreenSpace(View, Polygon.Vertices[VertexIndex]);
			const FVector2D NextScreenPos = TextureSpaceToScreenSpace(View, Polygon.Vertices[NextVertexIndex]);

			bool IsEdgeSelected = IsPolygonVertexSelected(PolygonIndex, VertexIndex) && IsPolygonVertexSelected(PolygonIndex, NextVertexIndex);


			// Draw the normal tick
			if (bShowNormals)
			{
				const FVector2D Direction = (NextScreenPos - ScreenPos).GetSafeNormal();
				const FVector2D Normal = FVector2D(-Direction.Y, Direction.X);

				const FVector2D Midpoint = (ScreenPos + NextScreenPos) * 0.5f;
				const FVector2D NormalPoint = Midpoint - Normal * NormalLength;
				FCanvasLineItem LineItem(Midpoint, NormalPoint);
				LineItem.SetColor(GeometryNormalColor);
				Canvas.DrawItem(LineItem);
			}

			// Draw the edge
			{
				if (bIsHitTesting)
				{
					TSharedPtr<FSpriteSelectedEdge> Data = MakeShareable(new FSpriteSelectedEdge());
					Data->SpritePtr = Sprite;
					Data->PolygonIndex = PolygonIndex;
					Data->VertexIndex = VertexIndex;
					Data->bRenderData = bIsRenderGeometry;
					Canvas.SetHitProxy(new HSpriteSelectableObjectHitProxy(Data));
				}

				FCanvasLineItem LineItem(ScreenPos, NextScreenPos);
				LineItem.SetColor(IsEdgeSelected ? SelectedColor : LineColor);
				Canvas.DrawItem(LineItem);

				if (bIsHitTesting)
				{
					Canvas.SetHitProxy(nullptr);
				}
			}
		}

		// Draw the vertices
		for (int32 VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
		{
			const FVector2D ScreenPos = TextureSpaceToScreenSpace(View, Polygon.Vertices[VertexIndex]);
			const float X = ScreenPos.X;
			const float Y = ScreenPos.Y;

			bool bIsVertexSelected = IsPolygonVertexSelected(PolygonIndex, VertexIndex);
			bool bIsVertexLastAdded = IsAddingPolygon() && AddingPolygonIndex == PolygonIndex && VertexIndex == Polygon.Vertices.Num() - 1;
			bool bNeedHighlightVertex = bIsVertexSelected || bIsVertexLastAdded;

			if (bIsHitTesting)
			{
				TSharedPtr<FSpriteSelectedVertex> Data = MakeShareable(new FSpriteSelectedVertex());
				Data->SpritePtr = Sprite;
				Data->PolygonIndex = PolygonIndex;
				Data->VertexIndex = VertexIndex;
				Data->bRenderData = bIsRenderGeometry;
				Canvas.SetHitProxy(new HSpriteSelectableObjectHitProxy(Data));
			}

			Canvas.DrawTile(ScreenPos.X - CollisionVertexSize*0.5f, ScreenPos.Y - CollisionVertexSize*0.5f, CollisionVertexSize, CollisionVertexSize, 0.f, 0.f, 1.f, 1.f, bNeedHighlightVertex ? SelectedColor : VertexColor, GWhiteTexture);
			
			if (bIsHitTesting)
			{
				Canvas.SetHitProxy(nullptr);
			}
		}
	}
}

void FSpriteEditorViewportClient::DrawGeometryStats(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, const FSpritePolygonCollection& Geometry, bool bIsRenderGeometry, int32& YPos)
{
	// Draw the type of geometry we're displaying stats for
	const FText GeometryName = bIsRenderGeometry ? LOCTEXT("RenderGeometry", "Render Geometry (source)") : LOCTEXT("CollisionGeometry", "Collision Geometry (source)");

	FCanvasTextItem TextItem(FVector2D(6, YPos), GeometryName, GEngine->GetSmallFont(), FLinearColor::White);
	TextItem.EnableShadow(FLinearColor::Black);

	TextItem.Draw(&Canvas);
	TextItem.Position += FVector2D(6.0f, 18.0f);


	// Draw the number of polygons
	TextItem.Text = FText::Format(LOCTEXT("PolygonCount", "Polys: {0}"), FText::AsNumber(Geometry.Polygons.Num()));
	TextItem.Draw(&Canvas);
	TextItem.Position.Y += 18.0f;

	//@TODO: Should show the agg numbers instead for collision, and the render tris numbers for rendering
	// Draw the number of vertices
	int32 NumVerts = 0;
	for (int32 PolyIndex = 0; PolyIndex < Geometry.Polygons.Num(); ++PolyIndex)
	{
		NumVerts += Geometry.Polygons[PolyIndex].Vertices.Num();
	}

	TextItem.Text = FText::Format(LOCTEXT("VerticesCount", "Verts: {0}"), FText::AsNumber(NumVerts));
	TextItem.Draw(&Canvas);
	TextItem.Position.Y += 18.0f;

	YPos = (int32)TextItem.Position.Y;
}

void AttributeTrianglesByMaterialType(int32 NumTriangles, UMaterialInterface* Material, int32& NumOpaqueTriangles, int32& NumMaskedTriangles, int32& NumTranslucentTriangles)
{
	if (Material != nullptr)
	{
		switch (Material->GetBlendMode())
		{
		case EBlendMode::BLEND_Opaque:
			NumOpaqueTriangles += NumTriangles;
			break;
		case EBlendMode::BLEND_Translucent:
		case EBlendMode::BLEND_Additive:
		case EBlendMode::BLEND_Modulate:
			NumTranslucentTriangles += NumTriangles;
			break;
		case EBlendMode::BLEND_Masked:
			NumMaskedTriangles += NumTriangles;
			break;
		}
	}
}

void FSpriteEditorViewportClient::DrawRenderStats(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, class UPaperSprite* Sprite, int32& YPos)
{
	FCanvasTextItem TextItem(FVector2D(6, YPos), LOCTEXT("RenderGeomBaked", "Render Geometry (baked)"), GEngine->GetSmallFont(), FLinearColor::White);
	TextItem.EnableShadow(FLinearColor::Black);

	TextItem.Draw(&Canvas);
	TextItem.Position += FVector2D(6.0f, 18.0f);

	int32 NumSections = (Sprite->AlternateMaterialSplitIndex != INDEX_NONE) ? 2 : 1;
	if (NumSections > 1)
	{
		TextItem.Text = FText::Format(LOCTEXT("SectionCount", "Sections: {0}"), FText::AsNumber(NumSections));
		TextItem.Draw(&Canvas);
		TextItem.Position.Y += 18.0f;
	}

	int32 NumOpaqueTriangles = 0;
	int32 NumMaskedTriangles = 0;
	int32 NumTranslucentTriangles = 0;

	int32 NumVerts = Sprite->BakedRenderData.Num();
	int32 DefaultTriangles = 0;
	int32 AlternateTriangles = 0;
	if (Sprite->AlternateMaterialSplitIndex != INDEX_NONE)
	{
		DefaultTriangles = Sprite->AlternateMaterialSplitIndex / 3;
		AlternateTriangles = (NumVerts - Sprite->AlternateMaterialSplitIndex) / 3;
	}
	else
	{
		DefaultTriangles = NumVerts / 3;
	}

	AttributeTrianglesByMaterialType(DefaultTriangles, Sprite->GetDefaultMaterial(), /*inout*/ NumOpaqueTriangles, /*inout*/ NumMaskedTriangles, /*inout*/ NumTranslucentTriangles);
	AttributeTrianglesByMaterialType(AlternateTriangles, Sprite->GetAlternateMaterial(), /*inout*/ NumOpaqueTriangles, /*inout*/ NumMaskedTriangles, /*inout*/ NumTranslucentTriangles);

	// Draw the number of polygons
	if (NumOpaqueTriangles > 0)
	{
		TextItem.Text = FText::Format(LOCTEXT("OpaqueTriangleCount", "Triangles: {0} (opaque)"), FText::AsNumber(NumOpaqueTriangles));
		TextItem.Draw(&Canvas);
		TextItem.Position.Y += 18.0f;
	}

	if (NumMaskedTriangles > 0)
	{
		TextItem.Text = FText::Format(LOCTEXT("MaskedTriangleCount", "Triangles: {0} (masked)"), FText::AsNumber(NumMaskedTriangles));
		TextItem.Draw(&Canvas);
		TextItem.Position.Y += 18.0f;
	}

	if (NumTranslucentTriangles > 0)
	{
		TextItem.Text = FText::Format(LOCTEXT("TranslucentTriangleCount", "Triangles: {0} (translucent)"), FText::AsNumber(NumTranslucentTriangles));
		TextItem.Draw(&Canvas);
		TextItem.Position.Y += 18.0f;
	}

	YPos = (int32)TextItem.Position.Y;
}

void FSpriteEditorViewportClient::DrawBoundsAsText(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, int32& YPos)
{
	FNumberFormattingOptions NoDigitGroupingFormat;
	NoDigitGroupingFormat.UseGrouping = false;

	UPaperSprite* Sprite = GetSpriteBeingEdited();
	FBoxSphereBounds Bounds = Sprite->GetRenderBounds();

	const FText DisplaySizeText = FText::Format(LOCTEXT("BoundsSize", "Approx. Size: {0}x{1}x{2}"),
		FText::AsNumber((int32)(Bounds.BoxExtent.X * 2.0f), &NoDigitGroupingFormat),
		FText::AsNumber((int32)(Bounds.BoxExtent.Y * 2.0f), &NoDigitGroupingFormat),
		FText::AsNumber((int32)(Bounds.BoxExtent.Z * 2.0f), &NoDigitGroupingFormat));

	Canvas.DrawShadowedString(
		6,
		YPos,
		*DisplaySizeText.ToString(),
		GEngine->GetSmallFont(),
		FLinearColor::White);
	YPos += 18;
}

void FSpriteEditorViewportClient::DrawSockets(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	UPaperSprite* Sprite = GetSpriteBeingEdited();

	const bool bIsHitTesting = PDI->IsHitTesting();

	const float DiamondSize = 5.0f;
	const FColor DiamondColor(255, 128, 128);

	const FTransform SpritePivotToWorld = Sprite->GetPivotToWorld();
	for (int32 SocketIndex = 0; SocketIndex < Sprite->Sockets.Num(); SocketIndex++)
	{
		const FPaperSpriteSocket& Socket = Sprite->Sockets[SocketIndex];
		const FMatrix SocketTM = (Socket.LocalTransform * SpritePivotToWorld).ToMatrixWithScale();
		
		if (bIsHitTesting)
		{
			TSharedPtr<FSpriteSelectedSocket> Data = MakeShareable(new FSpriteSelectedSocket);
			Data->SpritePtr = Sprite;
			Data->SocketName = Socket.SocketName;
			PDI->SetHitProxy(new HSpriteSelectableObjectHitProxy(Data));
		}
		
		DrawWireDiamond(PDI, SocketTM, DiamondSize, DiamondColor, SDPG_Foreground);
		
		PDI->SetHitProxy(nullptr);
	}
}

void FSpriteEditorViewportClient::DrawSocketNames(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	UPaperSprite* Sprite = GetSpriteBeingEdited();

	const int32 HalfX = Viewport->GetSizeXY().X / 2;
	const int32 HalfY = Viewport->GetSizeXY().Y / 2;

	const FColor SocketNameColor(255,196,196);

	// Draw socket names if desired.
	const FTransform SpritePivotToWorld = Sprite->GetPivotToWorld();
	for (int32 SocketIndex = 0; SocketIndex < Sprite->Sockets.Num(); SocketIndex++)
	{
		const FPaperSpriteSocket& Socket = Sprite->Sockets[SocketIndex];

		const FVector SocketWorldPos = SpritePivotToWorld.TransformPosition(Socket.LocalTransform.GetLocation());
		
		const FPlane Proj = View.Project(SocketWorldPos);
		if (Proj.W > 0.f)
		{
			const int32 XPos = HalfX + (HalfX * Proj.X);
			const int32 YPos = HalfY + (HalfY * (-Proj.Y));

			FCanvasTextItem Msg(FVector2D(XPos, YPos), FText::FromString(Socket.SocketName.ToString()), GEngine->GetMediumFont(), SocketNameColor);
			Canvas.DrawItem(Msg);

			//@TODO: Draws the current value of the rotation (probably want to keep this!)
// 			if (bManipulating && StaticMeshEditorPtr.Pin()->GetSelectedSocket() == Socket)
// 			{
// 				// Figure out the text height
// 				FTextSizingParameters Parameters(GEngine->GetSmallFont(), 1.0f, 1.0f);
// 				UCanvas::CanvasStringSize(Parameters, *Socket->SocketName.ToString());
// 				int32 YL = FMath::TruncToInt(Parameters.DrawYL);
// 
// 				DrawAngles(&Canvas, XPos, YPos + YL, 
// 					Widget->GetCurrentAxis(), 
// 					GetWidgetMode(),
// 					Socket->RelativeRotation,
// 					Socket->RelativeLocation);
// 			}
		}
	}
}

void FSpriteEditorViewportClient::DrawCanvas(FViewport& Viewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(Viewport, View, Canvas);

	const bool bIsHitTesting = Canvas.IsHitTesting();
	if (!bIsHitTesting)
	{
		Canvas.SetHitProxy(nullptr);
	}

	if (!SpriteEditorPtr.IsValid())
	{
		return;
	}

	UPaperSprite* Sprite = GetSpriteBeingEdited();

	int32 YPos = 42;

	static const FText GeomHelpStr = LOCTEXT("GeomEditHelp", "Shift + click to insert a vertex.\nSelect one or more vertices and press Delete to remove them.\nDouble click a vertex to select a polygon\n");
	static const FText GeomAddPolygonHelpStr = LOCTEXT("GeomClickAddPolygon", "Click to start creating a polygon\nCtrl + Click to start creating a subtractive polygon\n");
	static const FText GeomAddCollisionPolygonHelpStr = LOCTEXT("GeomClickAddCollisionPolygon", "Click to start creating a polygon");
	static const FText GeomAddVerticesHelpStr = LOCTEXT("GeomClickAddVertices", "Click to add points to the polygon\nEnter to finish adding a polygon\nEscape to cancel this polygon\n");
	static const FText SourceRegionHelpStr = LOCTEXT("SourceRegionHelp", "Drag handles to adjust source region\nDouble-click on an image region to select all connected pixels\nHold down Ctrl and drag a rectangle to create a new sprite at that position");

	switch (CurrentMode)
	{
	case ESpriteEditorMode::ViewMode:
	default:

		// Display the pivot
		{
			FNumberFormattingOptions NoDigitGroupingFormat;
			NoDigitGroupingFormat.UseGrouping = false;
			const FText PivotStr = FText::Format(LOCTEXT("PivotPosition", "Pivot: ({0}, {1})"), FText::AsNumber(Sprite->CustomPivotPoint.X, &NoDigitGroupingFormat), FText::AsNumber(Sprite->CustomPivotPoint.Y, &NoDigitGroupingFormat));
			FCanvasTextItem TextItem(FVector2D(6, YPos), PivotStr, GEngine->GetSmallFont(), FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Draw(&Canvas);
			YPos += 18;
		}

		// Collision geometry (if present)
		if (Sprite->BodySetup != nullptr)
		{
			DrawGeometryStats(Viewport, View, Canvas, Sprite->CollisionGeometry, false, /*inout*/ YPos);
		}

		// Baked render data
		DrawRenderStats(Viewport, View, Canvas, Sprite, /*inout*/ YPos);

		// And bounds
		DrawBoundsAsText(Viewport, View, Canvas, /*inout*/ YPos);

		break;
	case ESpriteEditorMode::EditCollisionMode:
		{
 			// Display tool help
			{
				const FText* HelpStr;
				if (IsAddingPolygon())
				{
					HelpStr = (AddingPolygonIndex == -1) ? &GeomAddCollisionPolygonHelpStr : &GeomAddVerticesHelpStr;
				}
				else
				{
					HelpStr = &GeomHelpStr;
				}

				FCanvasTextItem TextItem(FVector2D(6, YPos), *HelpStr, GEngine->GetSmallFont(), FLinearColor::White);
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Draw(&Canvas);
				YPos += 54;
			}

			// Draw the custom collision geometry
			const FLinearColor CollisionColor(1.0f, 1.0f, 0.0f, 1.0f);
			const FLinearColor White(1, 1, 1, 1);
			DrawGeometry(Viewport, View, Canvas, Sprite->CollisionGeometry, CollisionColor, White, false);
			if (Sprite->BodySetup != nullptr)
			{
				DrawGeometryStats(Viewport, View, Canvas, Sprite->CollisionGeometry, false, /*inout*/ YPos);
			}
		}
		break;
	case ESpriteEditorMode::EditRenderingGeomMode:
		{
 			// Display tool help
			{
				const FText* HelpStr;
				if (IsAddingPolygon())
				{
					HelpStr = (AddingPolygonIndex == -1) ? &GeomAddPolygonHelpStr : &GeomAddVerticesHelpStr;
				}
				else
				{
					HelpStr = &GeomHelpStr;
				}

				FCanvasTextItem TextItem(FVector2D(6, YPos), *HelpStr, GEngine->GetSmallFont(), FLinearColor::White);
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Draw(&Canvas);
				YPos += 54;
			}

			// Draw the custom render geometry
			const FLinearColor RenderGeomColor(1.0f, 0.2f, 0.0f, 1.0f);
			const FLinearColor NegativeRenderGeomColor(0.0f, 0.2f, 1.0f, 1.0f);
			DrawGeometry(Viewport, View, Canvas, Sprite->RenderGeometry, RenderGeomColor, NegativeRenderGeomColor, true);
			DrawGeometryStats(Viewport, View, Canvas, Sprite->RenderGeometry, true, /*inout*/ YPos);
			DrawRenderStats(Viewport, View, Canvas, Sprite, /*inout*/ YPos);

			// And bounds
			DrawBoundsAsText(Viewport, View, Canvas, /*inout*/ YPos);
		}
		break;
	case ESpriteEditorMode::EditSourceRegionMode:
		{
			// Display tool help
			{
				FCanvasTextItem TextItem(FVector2D(6, YPos), SourceRegionHelpStr, GEngine->GetSmallFont(), FLinearColor::White);
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Draw(&Canvas);
				YPos += 18;
			}

			if (bShowRelatedSprites)
			{
				// Alpha is being ignored in FBatchedElements::AddLine :(
				const FLinearColor RelatedBoundsColor(0.3f, 0.3f, 0.3f, 0.2f);
				DrawRelatedSprites(Viewport, View, Canvas, RelatedBoundsColor);
			}

			const FLinearColor BoundsColor(1.0f, 1.0f, 1.0f, 0.8f);
			DrawSourceRegion(Viewport, View, Canvas, BoundsColor);
		}
		break;
	case ESpriteEditorMode::AddSpriteMode:
		//@TODO:
		break;
	}

	if (bIsMarqueeTracking)
	{
		const FLinearColor MarqueeColor(1.0f, 1.0f, 1.0f, 0.5f);
		DrawMarquee(Viewport, View, Canvas, MarqueeColor);
	}

	if (bShowSockets)
	{
		DrawSocketNames(Viewport, View, Canvas);
	}
}

void FSpriteEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	// We don't draw the pivot when showing the source region
	// The pivot may be outside the actual texture bounds there
	if (bShowPivot && !bShowSourceTexture && !IsInSourceRegionEditMode())
	{
		FUnrealEdUtils::DrawWidget(View, PDI, RenderSpriteComponent->ComponentToWorld.ToMatrixWithScale(), 0, 0, EAxisList::XZ, EWidgetMovementMode::WMM_Translate);
	}

	if (bShowSockets)
	{
		DrawSockets(View, PDI);
	}
}

void FSpriteEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	FEditorViewportClient::Draw(Viewport, Canvas);
}

void FSpriteEditorViewportClient::Tick(float DeltaSeconds)
{
	if (UPaperSprite* Sprite = RenderSpriteComponent->GetSprite())
	{
		//@TODO: Doesn't need to happen every frame, only when properties are updated

		// Update the source texture view sprite (in case the texture has changed)
		UpdateSourceTextureSpriteFromSprite(Sprite);

		// Reposition the sprite (to be at the correct relative location to it's parent, undoing the pivot behavior)
		const FVector2D PivotInTextureSpace = Sprite->ConvertPivotSpaceToTextureSpace(FVector2D::ZeroVector);
		const FVector PivotInWorldSpace = TextureSpaceToWorldSpace(PivotInTextureSpace);
		RenderSpriteComponent->SetRelativeLocation(PivotInWorldSpace);

		bool bSourceTextureViewComponentVisibility = bShowSourceTexture || IsInSourceRegionEditMode();
		if (bSourceTextureViewComponentVisibility != SourceTextureViewComponent->IsVisible())
		{
			bDeferZoomToSprite = true;
			SourceTextureViewComponent->SetVisibility(bSourceTextureViewComponentVisibility);
		}

		bool bRenderTextureViewComponentVisibility = !IsInSourceRegionEditMode();
		if (bRenderTextureViewComponentVisibility != RenderSpriteComponent->IsVisible())
		{
			bDeferZoomToSprite = true;
			RenderSpriteComponent->SetVisibility(bRenderTextureViewComponentVisibility);
		}

		// Zoom in on the sprite
		//@TODO: Fix this properly so it doesn't need to be deferred, or wait for the viewport to initialize
		FIntPoint Size = Viewport->GetSizeXY();
		if (bDeferZoomToSprite && (Size.X > 0) && (Size.Y > 0))
		{
			UPaperSpriteComponent* ComponentToFocusOn = SourceTextureViewComponent->IsVisible() ? SourceTextureViewComponent : RenderSpriteComponent;
			FocusViewportOnBox(ComponentToFocusOn->Bounds.GetBox(), true);
			bDeferZoomToSprite = false;
		}		
	}

	if (bIsMarqueeTracking)
	{
		int32 HitX = Viewport->GetMouseX();
		int32 HitY = Viewport->GetMouseY();
		MarqueeEndPos = FVector2D(HitX, HitY);
	}

	FPaperEditorViewportClient::Tick(DeltaSeconds);

	if (!GIntraFrameDebuggingGameThread)
	{
		OwnedPreviewScene.GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FSpriteEditorViewportClient::ToggleShowSourceTexture()
{
	bShowSourceTexture = !bShowSourceTexture;
	SourceTextureViewComponent->SetVisibility(bShowSourceTexture);
	
	Invalidate();
}

void FSpriteEditorViewportClient::ToggleShowMeshEdges()
{
	EngineShowFlags.MeshEdges = !EngineShowFlags.MeshEdges;
	Invalidate(); 
}

bool FSpriteEditorViewportClient::IsShowMeshEdgesChecked() const
{
	return EngineShowFlags.MeshEdges;
}

// Find all related sprites (not including self)
void FSpriteEditorViewportClient::UpdateRelatedSpritesList()
{
	UPaperSprite* Sprite = GetSpriteBeingEdited();
	UTexture2D* Texture = Sprite->GetSourceTexture();
	if (Texture != nullptr)
	{
		FARFilter Filter;
		Filter.ClassNames.Add(UPaperSprite::StaticClass()->GetFName());
		const FString TextureString = FAssetData(Texture).GetExportTextName();
		const FName SourceTexturePropName(TEXT("SourceTexture"));
		Filter.TagsAndValues.Add(SourceTexturePropName, TextureString);
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> SpriteAssetData;
		AssetRegistryModule.Get().GetAssets(Filter, SpriteAssetData);

		FAssetData CurrentAssetData(Sprite);

		RelatedSprites.Empty();
		for (int i = 0; i < SpriteAssetData.Num(); ++i)
		{
			FAssetData& SpriteAsset = SpriteAssetData[i];
			if (SpriteAsset == Sprite)
			{
				continue;
			}

			const FString* SourceUVString = SpriteAsset.TagsAndValues.Find("SourceUV");
			const FString* SourceDimensionString = SpriteAsset.TagsAndValues.Find("SourceDimension");
			FVector2D SourceUV, SourceDimension;
			if (SourceUVString != nullptr && SourceDimensionString != nullptr)
			{
				if (SourceUV.InitFromString(*SourceUVString) && SourceDimension.InitFromString(*SourceDimensionString))
				{
					FRelatedSprite RelatedSprite;
					RelatedSprite.AssetData = SpriteAsset;
					RelatedSprite.SourceUV = SourceUV;
					RelatedSprite.SourceDimension = SourceDimension;
					
					RelatedSprites.Add(RelatedSprite);
				}
			}
		}
	}
}

void FSpriteEditorViewportClient::UpdateMouseDelta()
{
	FPaperEditorViewportClient::UpdateMouseDelta();
}

void FSpriteEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	const FViewportClick Click(&View, this, Key, Event, HitX, HitY);
	const bool bIsCtrlKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	const bool bIsShiftKeyDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);
	const bool bIsAltKeyDown = Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt);
	bool bHandled = false;

	const bool bAllowSelectVertex = !(IsEditingGeometry() && IsAddingPolygon()) && !bIsShiftKeyDown;

	const bool bClearSelectionModifier = bIsCtrlKeyDown;
	const bool bDeleteClickedVertex = bIsAltKeyDown;
	const bool bInsertVertexModifier = bIsShiftKeyDown;
	HSpriteSelectableObjectHitProxy* SelectedItemProxy = HitProxyCast<HSpriteSelectableObjectHitProxy>(HitProxy);

	if (bAllowSelectVertex && SelectedItemProxy)
	{
		if (!bClearSelectionModifier)
		{
			ClearSelectionSet();
		}

		if (bDeleteClickedVertex)
		{
			// Delete selection
			if (const FSpriteSelectedVertex* SelectedVertex = SelectedItemProxy->Data->CastSelectedVertex())
			{
				ClearSelectionSet();
				SelectionSet.Add(SelectedItemProxy->Data);
				SelectedVertexSet.Add(GetPolygonVertexHash(SelectedVertex->PolygonIndex, SelectedVertex->VertexIndex));
				DeleteSelection();
			}
		}
		else if (Event == EInputEvent::IE_DoubleClick)
		{
			// Double click to select a polygon
			if (const FSpriteSelectedVertex* SelectedVertex = SelectedItemProxy->Data->CastSelectedVertex())
			{
				if (SelectedVertex->IsValidInEditor(GetSpriteBeingEdited(), IsInRenderingEditMode()))
				{
					SelectPolygon(SelectedVertex->PolygonIndex);
				}
			}
		}
		else
		{
			if (const FSpriteSelectedVertex* SelectedVertex = SelectedItemProxy->Data->CastSelectedVertex())
			{
				if (SelectedVertex->IsA(FSelectionTypes::Edge))
				{
					// Add the next vertex defined by this edge
					AddPolygonEdgeToSelection(SelectedVertex->PolygonIndex, SelectedVertex->VertexIndex);
				}
				else
				{
					AddPolygonVertexToSelection(SelectedVertex->PolygonIndex, SelectedVertex->VertexIndex);
				}
			}
			else
			{
				SelectionSet.Add(SelectedItemProxy->Data);
			}
		}

		bHandled = true;
	}
// 	else if (HWidgetUtilProxy* PivotProxy = HitProxyCast<HWidgetUtilProxy>(HitProxy))
// 	{
// 		//const bool bUserWantsPaint = bIsLeftButtonDown && ( !GetDefault<ULevelEditorViewportSettings>()->bLeftMouseDragMovesCamera ||  bIsCtrlDown );
// 		//findme
// 		WidgetAxis = WidgetProxy->Axis;
// 
// 			// Calculate the screen-space directions for this drag.
// 			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( Viewport, GetScene(), EngineShowFlags ));
// 			FSceneView* View = CalcSceneView(&ViewFamily);
// 			WidgetProxy->CalcVectors(View, FViewportClick(View, this, Key, Event, HitX, HitY), LocalManipulateDir, WorldManipulateDir, DragX, DragY);
// 			bHandled = true;
// 	}
	else
	{
		if (IsInSourceRegionEditMode())
		{
			ClearSelectionSet();

			if (Event == EInputEvent::IE_DoubleClick)
			{
				FVector4 WorldPoint = View.PixelToWorld(HitX, HitY, 0);
				UPaperSprite* Sprite = GetSpriteBeingEdited();
				FVector2D TexturePoint = SourceTextureViewComponent->GetSprite()->ConvertWorldSpaceToTextureSpace(WorldPoint);
				if (bIsCtrlKeyDown)
				{
					UPaperSprite* NewSprite = CreateNewSprite(Sprite->GetSourceUV(), Sprite->GetSourceSize());
					if (NewSprite != nullptr)
					{
						NewSprite->ExtractSourceRegionFromTexturePoint(TexturePoint);
						bHandled = true;
					}
				}
				else
				{
					Sprite->ExtractSourceRegionFromTexturePoint(TexturePoint);
					bHandled = true;
				}
			}
			else
			{
				FVector4 WorldPoint = View.PixelToWorld(HitX, HitY, 0);
				FVector2D TexturePoint = SourceTextureViewComponent->GetSprite()->ConvertWorldSpaceToTextureSpace(WorldPoint);
				for (int RelatedSpriteIndex = 0; RelatedSpriteIndex < RelatedSprites.Num(); ++RelatedSpriteIndex)
				{
					FRelatedSprite& RelatedSprite = RelatedSprites[RelatedSpriteIndex];
					if (TexturePoint.X >= RelatedSprite.SourceUV.X && TexturePoint.Y >= RelatedSprite.SourceUV.Y &&
						TexturePoint.X < (RelatedSprite.SourceUV.X + RelatedSprite.SourceDimension.X) &&
						TexturePoint.Y < (RelatedSprite.SourceUV.Y + RelatedSprite.SourceDimension.Y))
					{
						// Select this sprite
						if (UPaperSprite* LoadedSprite = Cast<UPaperSprite>(RelatedSprite.AssetData.GetAsset()))
						{
							if (SpriteEditorPtr.IsValid())
							{
								SpriteEditorPtr.Pin()->SetSpriteBeingEdited(LoadedSprite);
								break;
							}
						}
						bHandled = true;
					}
				}
			}
		}
		else if (IsEditingGeometry() && !IsAddingPolygon())
		{
			if (bInsertVertexModifier)
			{
				FVector4 WorldPoint = View.PixelToWorld(HitX, HitY, 0);
				UPaperSprite* Sprite = RenderSpriteComponent->GetSprite();
				if (Sprite != nullptr)
				{
					FVector2D SpriteSpaceClickPoint = Sprite->ConvertWorldSpaceToTextureSpace(WorldPoint);

					// find a polygon to add vert to
					if (SelectionSet.Num() > 0)
					{
						TSharedPtr<FSelectedItem> SelectedItemPtr = *SelectionSet.CreateIterator();
						if (const FSpriteSelectedVertex* SelectedVertex = SelectedItemPtr->CastSelectedVertex())
						{
							if (SelectedVertex->IsValidInEditor(Sprite, IsInRenderingEditMode()))
							{
								AddPointToGeometry(SpriteSpaceClickPoint, SelectedVertex->PolygonIndex);
							}
						}
					}
					else
					{
						AddPointToGeometry(SpriteSpaceClickPoint);
					}
					bHandled = true;
				}
			}
		}

		if (!bHandled)
		{
			FPaperEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
		}
	}
}

// Create a new sprite and return this sprite. The sprite editor will now be editing this new sprite
// Returns nullptr if failed
UPaperSprite* FSpriteEditorViewportClient::CreateNewSprite(FVector2D TopLeft, FVector2D Dimensions)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	UPaperSprite* CurrentSprite = GetSpriteBeingEdited();
	UPaperSprite* CreatedSprite = nullptr;

	// Create the factory used to generate the sprite
	UPaperSpriteFactory* SpriteFactory = ConstructObject<UPaperSpriteFactory>(UPaperSpriteFactory::StaticClass());
	SpriteFactory->InitialTexture = CurrentSprite->SourceTexture;
	SpriteFactory->bUseSourceRegion = true;
	SpriteFactory->InitialSourceUV = TopLeft;
	SpriteFactory->InitialSourceDimension = Dimensions;


	// Get a unique name for the sprite
	FString Name, PackageName;
	AssetToolsModule.Get().CreateUniqueAssetName(CurrentSprite->GetOutermost()->GetName(), TEXT(""), /*out*/ PackageName, /*out*/ Name);
	const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
	if (UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, UPaperSprite::StaticClass(), SpriteFactory))
	{
		TArray<UObject*> Objects;
		Objects.Add(NewAsset);
		ContentBrowserModule.Get().SyncBrowserToAssets(Objects);

		UPaperSprite* NewSprite = Cast<UPaperSprite>(NewAsset);
		if (SpriteEditorPtr.IsValid() && NewSprite != nullptr)
		{
			SpriteEditorPtr.Pin()->SetSpriteBeingEdited(NewSprite);
		}

		CreatedSprite = NewSprite;
	}

	return CreatedSprite;
}

bool FSpriteEditorViewportClient::ProcessMarquee(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, bool bMarqueeStartModifierPressed)
{
	bool bMarqueeReady = false;

	if (Key == EKeys::LeftMouseButton)
	{
		int32 HitX = Viewport->GetMouseX();
		int32 HitY = Viewport->GetMouseY();
		if (Event == IE_Pressed && bMarqueeStartModifierPressed)
		{
			HHitProxy*	HitResult = Viewport->GetHitProxy(HitX, HitY);
			if (!HitResult)
			{
				bIsMarqueeTracking = true;
				MarqueeStartPos = FVector2D(HitX, HitY);
				MarqueeEndPos = MarqueeStartPos;
			}
		}
		else if (bIsMarqueeTracking && Event == IE_Released)
		{
			MarqueeEndPos = FVector2D(HitX, HitY);
			bIsMarqueeTracking = false;
			bMarqueeReady = true;
		}
	}
	else if (bIsMarqueeTracking && Key == EKeys::Escape)
	{
		// Cancel marquee selection
		bIsMarqueeTracking = false;
	}

	return bMarqueeReady;
}

bool FSpriteEditorViewportClient::InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	bool bHandled = false;
	FInputEventState InputState(Viewport, Key, Event);

	// Handle marquee tracking in source region edit mode
	if (IsInSourceRegionEditMode())
	{
		bool bMarqueeStartModifier = InputState.IsCtrlButtonPressed();
		if (ProcessMarquee(Viewport, ControllerId, Key, Event, bMarqueeStartModifier))
		{
			FVector2D TextureSpaceStartPos, TextureSpaceDimensions;
			if (ConvertMarqueeToSourceTextureSpace(/*out*/TextureSpaceStartPos, /*out*/TextureSpaceDimensions))
			{
				//@TODO: Warn if overlapping with another sprite
				CreateNewSprite(TextureSpaceStartPos, TextureSpaceDimensions);
			}
		}
	}
	else if (IsEditingGeometry())
	{
		if (IsAddingPolygon())
		{
			if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
			{
				int32 HitX = Viewport->GetMouseX();
				int32 HitY = Viewport->GetMouseY();

				// Calculate world space positions
				FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, GetScene(), EngineShowFlags));
				FSceneView* View = CalcSceneView(&ViewFamily);
				FVector WorldPoint = View->PixelToWorld(HitX, HitY, 0);

				UPaperSprite* Sprite = GetSpriteBeingEdited();
				FVector2D TexturePoint = Sprite->ConvertWorldSpaceToTextureSpace(WorldPoint);

				BeginTransaction(LOCTEXT("AddPolygonVertexTransaction", "Add Vertex to Polygon"));

				FSpritePolygonCollection* Geometry = GetGeometryBeingEdited();
				if (AddingPolygonIndex == -1)
				{
					FSpritePolygon NewPolygon;
					
					const bool bIsCtrlKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
					NewPolygon.bNegativeWinding = CanAddSubtractivePolygon() && bIsCtrlKeyDown;

					Geometry->Polygons.Add(NewPolygon);
					AddingPolygonIndex = Geometry->Polygons.Num() - 1;
				}

				if (Geometry->Polygons.IsValidIndex(AddingPolygonIndex))
				{
					Geometry->Polygons[AddingPolygonIndex].Vertices.Add(TexturePoint);
				}
				else
				{
					ResetAddPolyonMode();
				}

				Geometry->GeometryType = ESpritePolygonMode::FullyCustom;
				bManipulationDirtiedSomething = true;
				EndTransaction();
			}
			else if (Key == EKeys::Enter)
			{
				ResetAddPolyonMode();
			}
			else if (Key == EKeys::Escape && AddingPolygonIndex != -1)
			{
				FSpritePolygonCollection* Polygons = GetGeometryBeingEdited();
				if (Polygons->Polygons.IsValidIndex(AddingPolygonIndex))
				{
					BeginTransaction(LOCTEXT("DeletePolygon", "Delete Polygon"));
					Polygons->Polygons.RemoveAt(AddingPolygonIndex);
					bManipulationDirtiedSomething = true;
					EndTransaction();
				}
				ResetAddPolyonMode();
			}
		}
		else
		{
			if (ProcessMarquee(Viewport, ControllerId, Key, Event, true))
			{
				bool bAddingToSelection = InputState.IsShiftButtonPressed(); //@TODO: control button moves widget? Hopefully make this more consistent when that is changed
				SelectVerticesInMarquee(bAddingToSelection);
			}
		}
	}

	// Start the drag
	//@TODO: EKeys::LeftMouseButton
	//@TODO: Event.IE_Pressed
	// Implement InputAxis
	// StartTracking

	// Pass keys to standard controls, if we didn't consume input
	return (bHandled) ? true : FEditorViewportClient::InputKey(Viewport,  ControllerId, Key, Event, AmountDepressed, bGamepad);
}

bool FSpriteEditorViewportClient::InputWidgetDelta(FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	bool bHandled = false;
	if (bManipulating && (CurrentAxis != EAxisList::None))
	{
		bHandled = true;

		// Negate Y because vertices are in source texture space, not world space
		const FVector2D Drag2D(FVector::DotProduct(Drag, PaperAxisX), -FVector::DotProduct(Drag, PaperAxisY));

		// Apply the delta to all of the selected objects
		for (auto SelectionIt = SelectionSet.CreateConstIterator(); SelectionIt; ++SelectionIt)
		{
			TSharedPtr<FSelectedItem> SelectedItem = *SelectionIt;
			SelectedItem->ApplyDelta(Drag2D);
		}

		if (SelectionSet.Num() > 0)
		{
			if (!IsInSourceRegionEditMode())
			{
				UPaperSprite* Sprite = GetSpriteBeingEdited();
				// Sprite->RebuildCollisionData();
				// Sprite->RebuildRenderData();
				Sprite->PostEditChange();
				Invalidate();
			}

			bManipulationDirtiedSomething = true;
		}
	}

	return bHandled;
}

void FSpriteEditorViewportClient::TrackingStarted(const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge)
{
	if (!bManipulating && bIsDragging)
	{
		BeginTransaction(LOCTEXT("ModificationInViewport", "Modification in Viewport"));
		bManipulating = true;
		bManipulationDirtiedSomething = false;
	}
}

void FSpriteEditorViewportClient::TrackingStopped() 
{
	if (bManipulating)
	{
		EndTransaction();
		bManipulating = false;
	}
}

FWidget::EWidgetMode FSpriteEditorViewportClient::GetWidgetMode() const
{
	return (SelectionSet.Num() > 0) ? FWidget::WM_Translate : FWidget::WM_None;
}

FVector FSpriteEditorViewportClient::GetWidgetLocation() const
{
	if (SelectionSet.Num() > 0)
	{
		UPaperSprite* Sprite = GetSpriteBeingEdited();

		// Find the center of the selection set
		FVector SummedPos(ForceInitToZero);
		for (auto SelectionIt = SelectionSet.CreateConstIterator(); SelectionIt; ++SelectionIt)
		{
			TSharedPtr<FSelectedItem> SelectedItem = *SelectionIt;
			SummedPos += SelectedItem->GetWorldPos();
		}
		return (SummedPos / SelectionSet.Num());
	}

	return FVector::ZeroVector;
}

FMatrix FSpriteEditorViewportClient::GetWidgetCoordSystem() const
{
	return FMatrix::Identity;
}

ECoordSystem FSpriteEditorViewportClient::GetWidgetCoordSystemSpace() const
{
	return COORD_World;
}

void FSpriteEditorViewportClient::BeginTransaction(const FText& SessionName)
{
	if (ScopedTransaction == nullptr)
	{
		ScopedTransaction = new FScopedTransaction(SessionName);

		UPaperSprite* Sprite = GetSpriteBeingEdited();
		Sprite->Modify();
	}
}

void FSpriteEditorViewportClient::EndTransaction()
{
	if (bManipulationDirtiedSomething)
	{
		UPaperSprite* Sprite = RenderSpriteComponent->GetSprite();

		if (IsInSourceRegionEditMode())
		{
			// Snap to pixel grid at the end of the drag
			Sprite->SourceUV.X = FMath::Max(FMath::RoundToFloat(Sprite->SourceUV.X), 0.0f);
			Sprite->SourceUV.Y = FMath::Max(FMath::RoundToFloat(Sprite->SourceUV.Y), 0.0f);
			Sprite->SourceDimension.X = FMath::Max(FMath::RoundToFloat(Sprite->SourceDimension.X), 0.0f);
			Sprite->SourceDimension.Y = FMath::Max(FMath::RoundToFloat(Sprite->SourceDimension.Y), 0.0f);
		}

		Sprite->PostEditChange();
	}
	
	bManipulationDirtiedSomething = false;

	if (ScopedTransaction != nullptr)
	{
		delete ScopedTransaction;
		ScopedTransaction = nullptr;
	}
}

void FSpriteEditorViewportClient::NotifySpriteBeingEditedHasChanged()
{
	//@TODO: Ideally we do this before switching
	EndTransaction();
	ClearSelectionSet();
	ResetAddPolyonMode();

	// Update components to know about the new sprite being edited
	UPaperSprite* Sprite = GetSpriteBeingEdited();

	RenderSpriteComponent->SetSprite(Sprite);
	UpdateSourceTextureSpriteFromSprite(Sprite);

	if (IsInSourceRegionEditMode()) 
	{
		UpdateRelatedSpritesList();
	}

	//
	bDeferZoomToSprite = true;
}

void FSpriteEditorViewportClient::FocusOnSprite()
{
	FocusViewportOnBox(RenderSpriteComponent->Bounds.GetBox());
}

// Be sure to call this with polygonIndex and VertexIndex in descending order
static void DeleteVertexInPolygonInternal(UPaperSprite* Sprite, FSpritePolygonCollection* Geometry, const int PolygonIndex, const int VertexIndex)
{
	if (Geometry->Polygons.IsValidIndex(PolygonIndex))
	{
		FSpritePolygon& Polygon = Geometry->Polygons[PolygonIndex];
		if (Polygon.Vertices.IsValidIndex(VertexIndex))
		{
			Polygon.Vertices.RemoveAt(VertexIndex);
			
			// Delete polygon
			if (Polygon.Vertices.Num() == 0)
			{
				Geometry->Polygons.RemoveAt(PolygonIndex);
			}
		}
	}
}

void FSpriteEditorViewportClient::DeleteSelection()
{
	// Make a set of polygon-vertices "pairs" to delete
	TSet<int32> CompositeIndicesSet;

	UPaperSprite* Sprite = GetSpriteBeingEdited();
	FSpritePolygonCollection* Geometry = GetGeometryBeingEdited();
	for (TSharedPtr<FSelectedItem> SelectionIt : SelectionSet)
	{
		if (const FSpriteSelectedVertex* SelectedVertex = SelectionIt->CastSelectedVertex())
		{
			bool bValidSelectedVertex = SelectedVertex->IsValidInEditor(Sprite, IsInRenderingEditMode());
			if (bValidSelectedVertex)
			{
				CompositeIndicesSet.Add(GetPolygonVertexHash(SelectedVertex->PolygonIndex, SelectedVertex->VertexIndex));
	
				if (SelectedVertex->IsA(FSelectionTypes::Edge)) // add the "next" point for the edge
				{
					int32 NextIndex = (SelectedVertex->VertexIndex + 1) % Geometry->Polygons[SelectedVertex->PolygonIndex].Vertices.Num();
					CompositeIndicesSet.Add(GetPolygonVertexHash(SelectedVertex->PolygonIndex, NextIndex));
				}
			}
		}
	}

	if (CompositeIndicesSet.Num() > 0)
	{
		BeginTransaction(LOCTEXT("DeleteSelectionTransaction", "Delete Selection"));

		// Must delete vertices and polygons in descending order to preserve index validity
		TArray<int32> CompositeIndices = CompositeIndicesSet.Array();
		CompositeIndices.Sort();
		for (int32 SelectedCompositeIndex = CompositeIndices.Num() - 1; SelectedCompositeIndex >= 0; --SelectedCompositeIndex)
		{
			int32 CompositeIndex = CompositeIndices[SelectedCompositeIndex];
			int32 PolygonIndex = GetPolygonIndexFromHash(CompositeIndex);
			int32 VertexIndex = GetVertexIndexFromHash(CompositeIndex);
			DeleteVertexInPolygonInternal(Sprite, Geometry, PolygonIndex, VertexIndex);
		}

		Geometry->GeometryType = ESpritePolygonMode::FullyCustom;
		bManipulationDirtiedSomething = true;

		ClearSelectionSet();
		EndTransaction();
		ResetAddPolyonMode();
	}
}

void FSpriteEditorViewportClient::SplitEdge()
{
	BeginTransaction(LOCTEXT("SplitEdgeTransaction", "Split Edge"));

	//@TODO: Really need a proper data structure!!!
	if (SelectionSet.Num() == 1)
	{
		// Split this item
		TSharedPtr<FSelectedItem> SelectedItem = *SelectionSet.CreateIterator();
		SelectedItem->SplitEdge();
		ClearSelectionSet();
		bManipulationDirtiedSomething = true;
	}
	else
	{
		// Can't handle this without a better DS, as the indices will get screwed up
	}

	EndTransaction();
}

void FSpriteEditorViewportClient::ToggleAddPolygonMode()
{
	if (IsAddingPolygon())
	{
		ResetAddPolyonMode();
	}
	else
	{
		ClearSelectionSet();
		
		bIsAddingPolygon = true; 
		AddingPolygonIndex = -1; 
		Invalidate();
	}
}

void FSpriteEditorViewportClient::SnapAllVerticesToPixelGrid()
{
	BeginTransaction(LOCTEXT("SnapAllVertsToPixelGridTransaction", "Snap All Verts to Pixel Grid"));

	//@TODO: Doesn't handle snapping rectangle bounding boxes!
	//@TODO: Only OK because we don't allow manually editing them yet, and they're always on pixel grid when created automatically
	if (FSpritePolygonCollection* Geometry = GetGeometryBeingEdited())
	{
		for (int32 PolyIndex = 0; PolyIndex < Geometry->Polygons.Num(); ++PolyIndex)
		{
			FSpritePolygon& Poly = Geometry->Polygons[PolyIndex];
			for (int32 VertexIndex = 0; VertexIndex < Poly.Vertices.Num(); ++VertexIndex)
			{
				FVector2D& Vertex = Poly.Vertices[VertexIndex];

				Vertex.X = FMath::RoundToInt(Vertex.X);
				Vertex.Y = FMath::RoundToInt(Vertex.Y);
				bManipulationDirtiedSomething = true;
			}
		}
	}

	EndTransaction();
}

FSpritePolygonCollection* FSpriteEditorViewportClient::GetGeometryBeingEdited() const
{
	UPaperSprite* Sprite = GetSpriteBeingEdited();
	switch (CurrentMode)
	{
	case ESpriteEditorMode::EditCollisionMode:
		return &(Sprite->CollisionGeometry);
	case ESpriteEditorMode::EditRenderingGeomMode:
		return &(Sprite->RenderGeometry);
	default:
		return nullptr;
	}
}

// Selects a polygon in the currently selected mode
void FSpriteEditorViewportClient::SelectPolygon(const int PolygonIndex)
{
	UPaperSprite* Sprite = GetSpriteBeingEdited();
	FSpritePolygonCollection* Geometry = GetGeometryBeingEdited();
	if (ensure(Geometry != nullptr && Geometry->Polygons.IsValidIndex(PolygonIndex)))
	{
		FSpritePolygon& Polygon = Geometry->Polygons[PolygonIndex];
		for (int VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
		{
			TSharedPtr<FSpriteSelectedVertex> Vertex = MakeShareable(new FSpriteSelectedVertex());
			Vertex->SpritePtr = Sprite;
			Vertex->PolygonIndex = PolygonIndex;
			Vertex->VertexIndex = VertexIndex;
			Vertex->bRenderData = IsInRenderingEditMode();

			AddPolygonVertexToSelection(PolygonIndex, VertexIndex);
		}
	}
}

bool FSpriteEditorViewportClient::IsPolygonVertexSelected(const int32 PolygonIndex, const int32 VertexIndex) const
{
	return SelectedVertexSet.Contains(GetPolygonVertexHash(PolygonIndex, VertexIndex));
}

void FSpriteEditorViewportClient::AddPolygonVertexToSelection(const int32 PolygonIndex, const int32 VertexIndex)
{
	UPaperSprite* Sprite = GetSpriteBeingEdited();
	FSpritePolygonCollection* Geometry = GetGeometryBeingEdited();
	if (ensure(Geometry != nullptr && Geometry->Polygons.IsValidIndex(PolygonIndex)))
	{
		FSpritePolygon& Polygon = Geometry->Polygons[PolygonIndex];
		if (Polygon.Vertices.IsValidIndex(VertexIndex))
		{
			if (!IsPolygonVertexSelected(PolygonIndex, VertexIndex))
			{
				TSharedPtr<FSpriteSelectedVertex> Vertex = MakeShareable(new FSpriteSelectedVertex());
				Vertex->SpritePtr = Sprite;
				Vertex->PolygonIndex = PolygonIndex;
				Vertex->VertexIndex = VertexIndex;
				Vertex->bRenderData = IsInRenderingEditMode();

				SelectionSet.Add(Vertex);
				SelectedVertexSet.Add(GetPolygonVertexHash(PolygonIndex, VertexIndex));
			}
		}
	}
}

void FSpriteEditorViewportClient::AddPolygonEdgeToSelection(const int32 PolygonIndex, const int32 FirstVertexIndex)
{
	FSpritePolygonCollection* Geometry = GetGeometryBeingEdited();
	if (ensure(Geometry != nullptr && Geometry->Polygons.IsValidIndex(PolygonIndex)))
	{
		FSpritePolygon& Polygon = Geometry->Polygons[PolygonIndex];
		int NextVertexIndex = (FirstVertexIndex + 1) % Polygon.Vertices.Num();

		AddPolygonVertexToSelection(PolygonIndex, FirstVertexIndex);
		AddPolygonVertexToSelection(PolygonIndex, NextVertexIndex);
	}
}


void FSpriteEditorViewportClient::ClearSelectionSet()
{
	SelectionSet.Empty();
	SelectedVertexSet.Empty();
}

static bool ClosestPointOnLine(const FVector2D& Point, const FVector2D& LineStart, const FVector2D& LineEnd, FVector2D& OutClosestPoint)
{
	FVector2D DL = LineEnd - LineStart;
	FVector2D DP = Point - LineStart;
	float DotL = FVector2D::DotProduct(DP, DL);
	float DotP = FVector2D::DotProduct(DL, DL);
	if (DotP > 0.0001f)
	{
		float T = DotL / DotP;
		if (T >= -0.0001f && T <= 1.0001f)
		{
			T = FMath::Clamp(T, 0.0f, 1.0f);
			OutClosestPoint = LineStart + T * DL;
			return true;
		}
	}

	return false;
}

// Adds a point to the currently edited index
// If SelectedPolygonIndex is set, only that polygon is considered
void FSpriteEditorViewportClient::AddPointToGeometry(const FVector2D& TextureSpacePoint, const int32 SelectedPolygonIndex)
{
	FSpritePolygonCollection* Geometry = GetGeometryBeingEdited();

	int ClosestPolygonIndex = -1;
	int ClosestVertexInsertIndex = -1;
	float ClosestDistanceSquared = MAX_FLT;

	int StartPolygonIndex = 0;
	int EndPolygonIndex = Geometry->Polygons.Num();
	if (SelectedPolygonIndex >= 0 && SelectedPolygonIndex < Geometry->Polygons.Num())
	{
		StartPolygonIndex = SelectedPolygonIndex;
		EndPolygonIndex = SelectedPolygonIndex + 1;
	}

	for (int PolygonIndex = StartPolygonIndex; PolygonIndex < EndPolygonIndex; ++PolygonIndex)
	{
		FSpritePolygon& Polygon = Geometry->Polygons[PolygonIndex];
		if (Polygon.Vertices.Num() >= 3)
		{
			for (int VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
			{
				FVector2D ClosestPoint;
				FVector2D LineStart = Polygon.Vertices[VertexIndex];
				int NextVertexIndex = (VertexIndex + 1) % Polygon.Vertices.Num();
				FVector2D LineEnd = Polygon.Vertices[NextVertexIndex];
				if (ClosestPointOnLine(TextureSpacePoint, LineStart, LineEnd, /*out*/ClosestPoint))
				{
					float CurrentDistanceSquared = FVector2D::DistSquared(ClosestPoint, TextureSpacePoint);
					if (CurrentDistanceSquared < ClosestDistanceSquared)
					{
						ClosestPolygonIndex = PolygonIndex;
						ClosestDistanceSquared = CurrentDistanceSquared;
						ClosestVertexInsertIndex = NextVertexIndex;
					}
				}
			}
		}
		else
		{
			// Simply insert the vertex
			for (int VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
			{
				float CurrentDistanceSquared = FVector2D::DistSquared(Polygon.Vertices[VertexIndex], TextureSpacePoint);
				if (CurrentDistanceSquared < ClosestDistanceSquared)
				{
					ClosestPolygonIndex = PolygonIndex;
					ClosestDistanceSquared = CurrentDistanceSquared;
					ClosestVertexInsertIndex = VertexIndex + 1;
				}
			}
		}
	}

	if (ClosestVertexInsertIndex != -1 && ClosestPolygonIndex != -1)
	{
		BeginTransaction(LOCTEXT("AddPolygonVertexTransaction", "Add Vertex to Polygon"));
		FSpritePolygon& Polygon = Geometry->Polygons[ClosestPolygonIndex];
		Polygon.Vertices.Insert(TextureSpacePoint, ClosestVertexInsertIndex);
		Geometry->GeometryType = ESpritePolygonMode::FullyCustom;
		bManipulationDirtiedSomething = true;
		EndTransaction();

		// Select this vertex
		ClearSelectionSet();
		AddPolygonVertexToSelection(ClosestPolygonIndex, ClosestVertexInsertIndex);
	}
}

void FSpriteEditorViewportClient::ResetMarqueeTracking()
{
	bIsMarqueeTracking = false;
}

bool FSpriteEditorViewportClient::ConvertMarqueeToSourceTextureSpace(/*out*/FVector2D& OutStartPos, /*out*/FVector2D& OutDimension)
{
	bool bSuccessful = false;
	UPaperSprite* Sprite = SourceTextureViewComponent->GetSprite();
	UTexture2D* SpriteSourceTexture = Sprite->GetSourceTexture();
	if (SpriteSourceTexture != nullptr)
	{
		// Calculate world space positions
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, GetScene(), EngineShowFlags));
		FSceneView* View = CalcSceneView(&ViewFamily);
		FVector StartPos = View->PixelToWorld(MarqueeStartPos.X, MarqueeStartPos.Y, 0);
		FVector EndPos = View->PixelToWorld(MarqueeEndPos.X, MarqueeEndPos.Y, 0);

		// Convert to source texture space to work out the pixels dragged
		FVector2D TextureSpaceStartPos = Sprite->ConvertWorldSpaceToTextureSpace(StartPos);
		FVector2D TextureSpaceEndPos = Sprite->ConvertWorldSpaceToTextureSpace(EndPos);

		if (TextureSpaceStartPos.X > TextureSpaceEndPos.X)
		{
			Swap(TextureSpaceStartPos.X, TextureSpaceEndPos.X);
		}
		if (TextureSpaceStartPos.Y > TextureSpaceEndPos.Y)
		{
			Swap(TextureSpaceStartPos.Y, TextureSpaceEndPos.Y);
		}

		int SourceTextureWidth = SpriteSourceTexture->GetSizeX();
		int SourceTextureHeight = SpriteSourceTexture->GetSizeY();
		TextureSpaceStartPos.X = FMath::Clamp((int)TextureSpaceStartPos.X, 0, SourceTextureWidth - 1);
		TextureSpaceStartPos.Y = FMath::Clamp((int)TextureSpaceStartPos.Y, 0, SourceTextureHeight - 1);
		TextureSpaceEndPos.X = FMath::Clamp((int)TextureSpaceEndPos.X, 0, SourceTextureWidth - 1);
		TextureSpaceEndPos.Y = FMath::Clamp((int)TextureSpaceEndPos.Y, 0, SourceTextureHeight - 1);

		FVector2D TextureSpaceDimensions = TextureSpaceEndPos - TextureSpaceStartPos;
		if (TextureSpaceDimensions.X > 0 || TextureSpaceDimensions.Y > 0)
		{
			OutStartPos = TextureSpaceStartPos;
			OutDimension = TextureSpaceDimensions;
			bSuccessful = true;
		}
	}

	return bSuccessful;
}

void FSpriteEditorViewportClient::SelectVerticesInMarquee(bool bAddToSelection)
{
	if (!bAddToSelection)
	{
		ClearSelectionSet();
	}

	if (UPaperSprite* Sprite = GetSpriteBeingEdited())
	{
		// Calculate world space positions
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, GetScene(), EngineShowFlags));
		FSceneView* View = CalcSceneView(&ViewFamily);
		FVector StartPos = View->PixelToWorld(MarqueeStartPos.X, MarqueeStartPos.Y, 0);
		FVector EndPos = View->PixelToWorld(MarqueeEndPos.X, MarqueeEndPos.Y, 0);

		// Convert to source texture space to work out the pixels dragged
		FVector2D TextureSpaceStartPos = Sprite->ConvertWorldSpaceToTextureSpace(StartPos);
		FVector2D TextureSpaceEndPos = Sprite->ConvertWorldSpaceToTextureSpace(EndPos);

		if (TextureSpaceStartPos.X > TextureSpaceEndPos.X)
		{
			Swap(TextureSpaceStartPos.X, TextureSpaceEndPos.X);
		}
		if (TextureSpaceStartPos.Y > TextureSpaceEndPos.Y)
		{
			Swap(TextureSpaceStartPos.Y, TextureSpaceEndPos.Y);
		}

		if (FSpritePolygonCollection* Geometry = GetGeometryBeingEdited())
		{
			for (int32 PolygonIndex = 0; PolygonIndex < Geometry->Polygons.Num(); ++PolygonIndex)
			{
				auto Polygon = Geometry->Polygons[PolygonIndex];
				for (int32 VertexIndex = 0; VertexIndex < Polygon.Vertices.Num(); ++VertexIndex)
				{
					FVector2D& Vertex = Polygon.Vertices[VertexIndex];
					if (Vertex.X >= TextureSpaceStartPos.X && Vertex.X <= TextureSpaceEndPos.X &&
						Vertex.Y >= TextureSpaceStartPos.Y && Vertex.Y <= TextureSpaceEndPos.Y)
					{
						AddPolygonVertexToSelection(PolygonIndex, VertexIndex);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE