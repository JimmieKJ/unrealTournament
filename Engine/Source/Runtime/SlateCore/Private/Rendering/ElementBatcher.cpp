// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "ElementBatcher.h"
#include "SlateRenderTransform.h"
#include "Internationalization/Text.h"
#include "SlateStats.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Layers"), STAT_SlateNumLayers, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Batches"), STAT_SlateNumBatches, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Vertices"), STAT_SlateVertexCount, STATGROUP_Slate);
DECLARE_MEMORY_STAT(TEXT("Batch Vertex Memory"), STAT_SlateVertexBatchMemory, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Batch Index Memory"), STAT_SlateIndexBatchMemory, STATGROUP_SlateMemory);

SLATE_DECLARE_CYCLE_COUNTER(GSlateAddElements, "Add Elements");
SLATE_DECLARE_CYCLE_COUNTER(GSlateFindBatchTime, "FindElementForBatch");
SLATE_DECLARE_CYCLE_COUNTER(GSlateFillBatchBuffers, "FillBatchBuffers");

// Super-hacky way of storing the scissor rect so we don't have to change all the FSlateDrawElement APIs for this hacky support.
SLATECORE_API TOptional<FShortRect> GSlateScissorRect;

FVector2D RoundToInt(const FVector2D& Vec)
{
	return FVector2D(FMath::RoundToInt(Vec.X), FMath::RoundToInt(Vec.Y));
}

/**
 * Used to construct a rotated rect from an aligned clip rect and a set of layout and render transforms from the geometry, snapped to pixel boundaries. Returns a float or float16 version of the rect based on the typedef.
 */
FSlateRotatedClipRectType ToSnappedRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FSlateLayoutTransform& InverseLayoutTransform, const FSlateRenderTransform& RenderTransform)
{
	FSlateRotatedRect RotatedRect = TransformRect(Concatenate(InverseLayoutTransform, RenderTransform), FSlateRotatedRect(ClipRectInLayoutWindowSpace));
	// Pixel snapping is done here by rounding the resulting floats to ints.
	return FSlateRotatedClipRectType(
		RoundToInt(RotatedRect.TopLeft), 
		RoundToInt(RotatedRect.ExtentX), 
		RoundToInt(RotatedRect.ExtentY));
}

/**
 * Computes the element tint color based in the user specified color and brush being used.
 * Note: The color could be in RGB or HSV
 * @return The final color to be passed per vertex
 */
static FColor GetElementColor( const FLinearColor& InColor, const FSlateBrush* InBrush )
{
	// Pass the color through
	return InColor.ToFColor(false);
}

FSlateElementBatcher::FSlateElementBatcher( TSharedRef<FSlateRenderingPolicy> InRenderingPolicy )
	: ResourceManager( *InRenderingPolicy->GetResourceManager() )
	, FontCache( *InRenderingPolicy->GetFontCache() )
	, PixelCenterOffset( InRenderingPolicy->GetPixelCenterOffset() )
{

}


FSlateElementBatcher::~FSlateElementBatcher()
{
}


void FSlateElementBatcher::AddElements( const TArray<FSlateDrawElement>& DrawElements )
{
	SLATE_CYCLE_COUNTER_SCOPE(GSlateAddElements);
	// This stuff is just for the counters. Could be scoped by an #ifdef if necessary.
	static_assert(
		FSlateDrawElement::EElementType::ET_Box == 0 &&
		FSlateDrawElement::EElementType::ET_DebugQuad == 1 &&
		FSlateDrawElement::EElementType::ET_Text == 2 &&
		FSlateDrawElement::EElementType::ET_Spline == 3 &&
		FSlateDrawElement::EElementType::ET_Line == 4 &&
		FSlateDrawElement::EElementType::ET_Gradient == 5 &&
		FSlateDrawElement::EElementType::ET_Viewport == 6 &&
		FSlateDrawElement::EElementType::ET_Border == 7 &&
		FSlateDrawElement::EElementType::ET_Custom == 8 &&
		FSlateDrawElement::EElementType::ET_Count == 9, 
		"If FSlateDrawElement::EElementType is modified, this array must be made to match.");

	static FName ElementFNames[] = 
	{
		FName(TEXT("Box")),
		FName(TEXT("DebugQuad")),
		FName(TEXT("Text")),
		FName(TEXT("Spline")),
		FName(TEXT("Line")),
		FName(TEXT("Gradient")),
		FName(TEXT("Viewport")),
		FName(TEXT("Border")),
		FName(TEXT("Custom")),
	};


	for( int32 DrawElementIndex = 0; DrawElementIndex < DrawElements.Num(); ++DrawElementIndex )
	{
		const FSlateDrawElement& DrawElement = DrawElements[DrawElementIndex];
		const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	
		// A zero or negatively sized clipping rect means the geometry will not be displayed
		const bool bIsFullyClipped = (!InClippingRect.IsValid() || (InClippingRect.Top == 0.0f && InClippingRect.Left == 0.0f && InClippingRect.Bottom == 0.0f && InClippingRect.Right == 0.0f));
		
		if ( !bIsFullyClipped )
		{
			// do this check in here do we can short circuit above more quickly, but still name this variable so its meaning is clear.
			const bool bIsScissored = DrawElement.GetScissorRect().IsSet() && !DrawElement.GetScissorRect().GetValue().DoesIntersect(FShortRect(InClippingRect));
			// scissor rects are sort of a low level hack, so no one konws to clip against them. Instead we do it here to make sure the element is not actually rendered.
			if (!bIsScissored)
			{
				// time just the adding of the element. The clipping stuff will be counted in exclusive time for the non-typed timer.
				SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlateAddElements, ElementFNames[DrawElement.GetElementType()]);
				// Determine what type of element to add
				switch( DrawElement.GetElementType() )
				{
				case FSlateDrawElement::ET_Box:
					AddBoxElement( DrawElement );
					break;
				case FSlateDrawElement::ET_DebugQuad:
					AddQuadElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Text:
					AddTextElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Spline:
					AddSplineElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Line:
					AddLineElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Gradient:
					AddGradientElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Viewport:
					AddViewportElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Border:
					AddBorderElement( DrawElement );
					break;
				case FSlateDrawElement::ET_Custom:
					AddCustomElement( DrawElement );
					break;
				default:
					checkf(0, TEXT("Invalid element type"));
					break;
				}
			}
		}
	}
}

void FSlateElementBatcher::AddQuadElement( const FSlateDrawElement& DrawElement, FColor Color )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	//const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	//ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::Default, ESlateDrawEffect::None, ESlateBatchDrawFlag::Wireframe|ESlateBatchDrawFlag::NoBlending, DrawElement.GetScissorRect() );
	TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
	TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];

	// Determine the four corners of the quad
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D(LocalSize.X, 0);
	FVector2D BotLeft = FVector2D(0, LocalSize.Y);
	FVector2D BotRight = FVector2D(LocalSize.X, LocalSize.Y);

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();

	// Add four vertices to the list of verts to be added to the vertex buffer
	BatchVertices.Add( FSlateVertex( RenderTransform, TopLeft, FVector2D(0.0f,0.0f), Color, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, TopRight, FVector2D(1.0f,0.0f), Color, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, BotLeft, FVector2D(0.0f,1.0f), Color, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, BotRight, FVector2D(1.0f,1.0f),Color, RenderClipRect ) );

	// The offset into the index buffer where this quads indices start
	uint32 IndexOffsetStart = BatchIndices.Num();
	// Add 6 indices to the vertex buffer.  (2 tri's per quad, 3 indices per tri)
	BatchIndices.Add( IndexStart + 0 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 2 );

	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 3 );
}


FSlateRenderTransform GetBoxRenderTransform(const FSlateDrawElement& DrawElement)
{
	const FSlateRenderTransform& ElementRenderTransform = DrawElement.GetRenderTransform();
	const float RotationAngle = DrawElement.GetDataPayload().Angle;
	if (RotationAngle == 0.0f)
	{
		return ElementRenderTransform;
	}
	const FVector2D RotationPoint = DrawElement.GetDataPayload().RotationPoint;
	const FSlateRenderTransform RotationTransform = Concatenate(Inverse(RotationPoint), FQuat2D(RotationAngle), RotationPoint);
	return Concatenate(RotationTransform, ElementRenderTransform);
}


void FSlateElementBatcher::AddBoxElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& ElementRenderTransform = DrawElement.GetRenderTransform();
	const FSlateRenderTransform RenderTransform = GetBoxRenderTransform(DrawElement);
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	// The clip rect is NOT subject to the rotations specified by MakeRotatedBox.
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, ElementRenderTransform);

	check(InPayload.BrushResource);
	const FSlateBrush* BrushResource = InPayload.BrushResource;

	if ( BrushResource->DrawAs != ESlateBrushDrawType::NoDrawType )
	{
		// Do pixel snapping
		FVector2D TopLeft(0,0);
		FVector2D BotRight(LocalSize);

		uint32 TextureWidth = 1;
		uint32 TextureHeight = 1;

		// Get the default start and end UV.  If the texture is atlased this value will be a subset of this
		FVector2D StartUV = FVector2D(0.0f,0.0f);
		FVector2D EndUV = FVector2D(1.0f,1.0f);
		FVector2D SizeUV;

		FVector2D HalfTexel;

		FSlateShaderResourceProxy* ResourceProxy = ResourceManager.GetShaderResource(*BrushResource);
		FSlateShaderResource* Resource = nullptr;
		if( ResourceProxy )
		{
			// The actual texture for rendering.  If the texture is atlased this is the atlas
			Resource = ResourceProxy->Resource;
			// The width and height of the texture (non-atlased size)
			TextureWidth = ResourceProxy->ActualSize.X != 0 ? ResourceProxy->ActualSize.X : 1;
			TextureHeight = ResourceProxy->ActualSize.Y != 0 ? ResourceProxy->ActualSize.Y : 1;

			// Texel offset
			HalfTexel = FVector2D( PixelCenterOffset/TextureWidth, PixelCenterOffset/TextureHeight );

			SizeUV = ResourceProxy->SizeUV;
			StartUV = ResourceProxy->StartUV + HalfTexel;
			EndUV = StartUV + ResourceProxy->SizeUV;
		}
		else
		{
			// no texture
			SizeUV = FVector2D(1.0f,1.0f);
			HalfTexel = FVector2D( PixelCenterOffset, PixelCenterOffset );
		}


		FColor Tint = GetElementColor(InPayload.Tint, BrushResource);

		const ESlateBrushTileType::Type TilingRule = BrushResource->Tiling;
		const bool bTileHorizontal = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Horizontal);
		const bool bTileVertical = (TilingRule == ESlateBrushTileType::Both || TilingRule == ESlateBrushTileType::Vertical);

		const ESlateBrushMirrorType::Type MirroringRule = BrushResource->Mirroring;
		const bool bMirrorHorizontal = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Horizontal);
		const bool bMirrorVertical = (MirroringRule == ESlateBrushMirrorType::Both || MirroringRule == ESlateBrushMirrorType::Vertical);

		// Pass the tiling information as a flag so we can pick the correct texture addressing mode
		ESlateBatchDrawFlag::Type DrawFlags = ( ( bTileHorizontal ? ESlateBatchDrawFlag::TileU : 0 ) | ( bTileVertical ? ESlateBatchDrawFlag::TileV : 0 ) );

		FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), Resource, ESlateDrawPrimitive::TriangleList, ESlateShader::Default, InDrawEffects, DrawFlags, DrawElement.GetScissorRect() );
		TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
		TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];

		float HorizontalTiling = bTileHorizontal ? LocalSize.X/TextureWidth : 1.0f;
		float VerticalTiling = bTileVertical ? LocalSize.Y/TextureHeight : 1.0f;

		const FVector2D Tiling( HorizontalTiling,VerticalTiling );

		// The start index of these vertices in the index buffer
		uint32 IndexStart = BatchVertices.Num();
		// The offset into the index buffer where this elements indices start
		uint32 IndexOffsetStart = BatchIndices.Num();

		const FMargin& Margin = BrushResource->Margin;

		if ( BrushResource->DrawAs != ESlateBrushDrawType::Image &&
			( Margin.Left != 0.0f || Margin.Top != 0.0f || Margin.Right != 0.0f || Margin.Bottom != 0.0f ) )
		{
			// Create 9 quads for the box element based on the following diagram
			//     ___LeftMargin    ___RightMargin
			//    /                /
			//  +--+-------------+--+
			//  |  |c1           |c2| ___TopMargin
			//  +--o-------------o--+
			//  |  |             |  |
			//  |  |c3           |c4|
			//  +--o-------------o--+
			//  |  |             |  | ___BottomMargin
			//  +--+-------------+--+


			// Determine the texture coordinates for each quad
			// These are not scaled.
			float LeftMarginU = (Margin.Left > 0.0f)
				? StartUV.X + Margin.Left * SizeUV.X + HalfTexel.X
				: StartUV.X;
			float TopMarginV = (Margin.Top > 0.0f)
				? StartUV.Y + Margin.Top * SizeUV.Y + HalfTexel.Y
				: StartUV.Y;
			float RightMarginU = (Margin.Right > 0.0f)
				? EndUV.X - Margin.Right * SizeUV.X + HalfTexel.X
				: EndUV.X;
			float BottomMarginV = (Margin.Bottom > 0.0f)
				? EndUV.Y - Margin.Bottom * SizeUV.Y + HalfTexel.Y
				: EndUV.Y;

			if( bMirrorHorizontal || bMirrorVertical )
			{
				const FVector2D UVMin = StartUV;
				const FVector2D UVMax = EndUV;

				if( bMirrorHorizontal )
				{
					StartUV.X = UVMax.X - ( StartUV.X - UVMin.X );
					EndUV.X = UVMax.X - ( EndUV.X - UVMin.X );
					LeftMarginU = UVMax.X - ( LeftMarginU - UVMin.X );
					RightMarginU = UVMax.X - ( RightMarginU - UVMin.X );
				}
				if( bMirrorVertical )
				{
					StartUV.Y = UVMax.Y - ( StartUV.Y - UVMin.Y );
					EndUV.Y = UVMax.Y - ( EndUV.Y - UVMin.Y );
					TopMarginV = UVMax.Y - ( TopMarginV - UVMin.Y );
					BottomMarginV = UVMax.Y - ( BottomMarginV - UVMin.Y );
				}
			}

			// Determine the margins for each quad

			float LeftMarginX = TextureWidth * Margin.Left;
			float TopMarginY = TextureHeight * Margin.Top;
			float RightMarginX = LocalSize.X - TextureWidth * Margin.Right;
			float BottomMarginY = LocalSize.Y - TextureHeight * Margin.Bottom;

			// If the margins are overlapping the margins are too big or the button is too small
			// so clamp margins to half of the box size
			if( RightMarginX < LeftMarginX )
			{
				LeftMarginX = LocalSize.X / 2;
				RightMarginX = LeftMarginX;
			}

			if( BottomMarginY < TopMarginY )
			{
				TopMarginY = LocalSize.Y / 2;
				BottomMarginY = TopMarginY;
			}

			FVector2D Position = TopLeft;
			FVector2D EndPos = BotRight;

			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, Position.Y ),		StartUV,									Tiling,	Tint, RenderClipRect ) ); //0
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, TopMarginY ),		FVector2D( StartUV.X, TopMarginV ),			Tiling,	Tint, RenderClipRect ) ); //1
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, Position.Y ),		FVector2D( LeftMarginU, StartUV.Y ),		Tiling,	Tint, RenderClipRect ) ); //2
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		FVector2D( LeftMarginU, TopMarginV ),		Tiling,	Tint, RenderClipRect ) ); //3
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, Position.Y ),	FVector2D( RightMarginU, StartUV.Y ),		Tiling,	Tint, RenderClipRect ) ); //4
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	FVector2D( RightMarginU,TopMarginV),		Tiling,	Tint, RenderClipRect ) ); //5
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, Position.Y ),		FVector2D( EndUV.X, StartUV.Y ),			Tiling,	Tint, RenderClipRect ) ); //6
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, TopMarginY ),		FVector2D( EndUV.X, TopMarginV),			Tiling,	Tint, RenderClipRect ) ); //7

			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, BottomMarginY ),	FVector2D( StartUV.X, BottomMarginV ),		Tiling,	Tint, RenderClipRect ) ); //8
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	FVector2D( LeftMarginU, BottomMarginV ),	Tiling,	Tint, RenderClipRect ) ); //9
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, BottomMarginY ),	FVector2D( RightMarginU, BottomMarginV ),	Tiling,	Tint, RenderClipRect ) ); //10
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, BottomMarginY ),		FVector2D( EndUV.X, BottomMarginV ),		Tiling,	Tint, RenderClipRect ) ); //11
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, EndPos.Y ),		FVector2D( StartUV.X, EndUV.Y ),			Tiling,	Tint, RenderClipRect ) ); //12
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, EndPos.Y ),		FVector2D( LeftMarginU, EndUV.Y ),			Tiling,	Tint, RenderClipRect ) ); //13
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, EndPos.Y ),		FVector2D( RightMarginU, EndUV.Y ),			Tiling,	Tint, RenderClipRect ) ); //14
			BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, EndPos.Y ),			EndUV,										Tiling,	Tint, RenderClipRect ) ); //15

			// Top
			BatchIndices.Add( IndexStart + 0 );
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart + 2 );
			BatchIndices.Add( IndexStart + 2 );
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart + 3 );

			BatchIndices.Add( IndexStart + 2 );
			BatchIndices.Add( IndexStart + 3 );
			BatchIndices.Add( IndexStart + 4 );
			BatchIndices.Add( IndexStart + 4 );
			BatchIndices.Add( IndexStart + 3 );
			BatchIndices.Add( IndexStart + 5 );

			BatchIndices.Add( IndexStart + 4 );
			BatchIndices.Add( IndexStart + 5 );
			BatchIndices.Add( IndexStart + 6 );
			BatchIndices.Add( IndexStart + 6 );
			BatchIndices.Add( IndexStart + 5 );
			BatchIndices.Add( IndexStart + 7 );

			// Middle
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart + 8 );
			BatchIndices.Add( IndexStart + 3 );
			BatchIndices.Add( IndexStart + 3 );
			BatchIndices.Add( IndexStart + 8 );
			BatchIndices.Add( IndexStart + 9 );

			BatchIndices.Add( IndexStart + 3 );
			BatchIndices.Add( IndexStart + 9 );
			BatchIndices.Add( IndexStart + 5 );
			BatchIndices.Add( IndexStart + 5 );
			BatchIndices.Add( IndexStart + 9 );
			BatchIndices.Add( IndexStart + 10 );

			BatchIndices.Add( IndexStart + 5 );
			BatchIndices.Add( IndexStart + 10 );
			BatchIndices.Add( IndexStart + 7 );
			BatchIndices.Add( IndexStart + 7 );
			BatchIndices.Add( IndexStart + 10 );
			BatchIndices.Add( IndexStart + 11 );

			// Bottom
			BatchIndices.Add( IndexStart + 8 );
			BatchIndices.Add( IndexStart + 12 );
			BatchIndices.Add( IndexStart + 9 );
			BatchIndices.Add( IndexStart + 9 );
			BatchIndices.Add( IndexStart + 12 );
			BatchIndices.Add( IndexStart + 13 );

			BatchIndices.Add( IndexStart + 9 );
			BatchIndices.Add( IndexStart + 13 );
			BatchIndices.Add( IndexStart + 10 );
			BatchIndices.Add( IndexStart + 10 );
			BatchIndices.Add( IndexStart + 13 );
			BatchIndices.Add( IndexStart + 14 );

			BatchIndices.Add( IndexStart + 10 );
			BatchIndices.Add( IndexStart + 14 );
			BatchIndices.Add( IndexStart + 11 );
			BatchIndices.Add( IndexStart + 11 );
			BatchIndices.Add( IndexStart + 14 );
			BatchIndices.Add( IndexStart + 15 );
		}
		else
		{
			FVector2D TopRight = FVector2D( BotRight.X, TopLeft.Y);
			FVector2D BotLeft =	 FVector2D( TopLeft.X, BotRight.Y);

			if( bMirrorHorizontal || bMirrorVertical )
			{
				const FVector2D UVMin = StartUV;
				const FVector2D UVMax = EndUV;

				if( bMirrorHorizontal )
				{
					StartUV.X = UVMax.X - ( StartUV.X - UVMin.X );
					EndUV.X = UVMax.X - ( EndUV.X - UVMin.X );
				}
				if( bMirrorVertical )
				{
					StartUV.Y = UVMax.Y - ( StartUV.Y - UVMin.Y );
					EndUV.Y = UVMax.Y - ( EndUV.Y - UVMin.Y );
				}
			}

			// Add four vertices to the list of verts to be added to the vertex buffer
			BatchVertices.Add( FSlateVertex( RenderTransform, TopLeft,	StartUV,						Tiling,	Tint, RenderClipRect ) );
			BatchVertices.Add( FSlateVertex( RenderTransform, TopRight,	FVector2D(EndUV.X,StartUV.Y),	Tiling,	Tint, RenderClipRect ) );
			BatchVertices.Add( FSlateVertex( RenderTransform, BotLeft,	FVector2D(StartUV.X,EndUV.Y),	Tiling,	Tint, RenderClipRect ) );
			BatchVertices.Add( FSlateVertex( RenderTransform, BotRight,	EndUV,							Tiling,	Tint, RenderClipRect ) );

			BatchIndices.Add( IndexStart + 0 );
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart + 2 );

			BatchIndices.Add( IndexStart + 2 );
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart + 3 );
		}
	}
}

void FSlateElementBatcher::AddTextElement(const FSlateDrawElement& DrawElement)
{
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	//const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform LayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition());

	// We don't just scale up fonts, we draw them in local space pre-scaled so we don't get scaling artifcats.
	// So we need to pull the layout scale out of the layout and render transform so we can apply them
	// in local space with pre-scaled fonts.
	const float FontScale = LayoutTransform.GetScale();
	FSlateLayoutTransform InverseLayoutTransform = Inverse(Concatenate(Inverse(FontScale), LayoutTransform));
	const FSlateRenderTransform RenderTransform = Concatenate(Inverse(FontScale), DrawElement.GetRenderTransform());

	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);
	// Used to clip individual characters as we generate them.
	FSlateRect LocalClipRect = TransformRect(InverseLayoutTransform, InClippingRect);

	const FString& Text = InPayload.Text;
	// Nothing to do if no text
	if( Text.Len() == 0 )
	{
		return;
	}


	FCharacterList& CharacterList = FontCache.GetCharacterList( InPayload.FontInfo, FontScale );

	float MaxHeight = CharacterList.GetMaxHeight();

	uint32 FontTextureIndex = 0;
	FSlateShaderResource* FontTexture = nullptr;

	FSlateElementBatch* ElementBatch = nullptr;
	TArray<FSlateVertex>* BatchVertices = nullptr;
	TArray<SlateIndex>* BatchIndices = nullptr;

	uint32 VertexOffset = 0;
	uint32 IndexOffset = 0;

	float InvTextureSizeX = 0;
	float InvTextureSizeY = 0;

	float LineX = 0;

	FCharacterEntry PreviousCharEntry;

	int32 Kerning = 0;

	FVector2D TopLeft(0,0);

	const float PosX = TopLeft.X;
	float PosY = TopLeft.Y;

	LineX = PosX;
	
	FColor FinalColor = GetElementColor( InPayload.Tint, nullptr );

	for( int32 CharIndex = 0; CharIndex < Text.Len(); ++CharIndex )
	{
		const TCHAR CurrentChar = Text[ CharIndex ];

		const bool IsNewline = (CurrentChar == '\n');

		if (IsNewline)
		{
			// Move down: we are drawing the next line.
			PosY += MaxHeight;
			// Carriage return 
			LineX = PosX;
		}
		else
		{
			const FCharacterEntry& Entry = CharacterList[ CurrentChar ];

			if( FontTexture == nullptr || Entry.TextureIndex != FontTextureIndex )
			{
				// Font has a new texture for this glyph. Refresh the batch we use and the index we are currently using
				FontTextureIndex = Entry.TextureIndex;

				FontTexture = FontCache.GetSlateTextureResource( FontTextureIndex );
				ElementBatch = &FindBatchForElement( Layer, FShaderParams(), FontTexture, ESlateDrawPrimitive::TriangleList, ESlateShader::Font, InDrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetScissorRect() );

				BatchVertices = &BatchVertexArrays[ElementBatch->VertexArrayIndex];
				BatchIndices = &BatchIndexArrays[ElementBatch->IndexArrayIndex];

				VertexOffset = BatchVertices->Num();
				IndexOffset = BatchIndices->Num();
				
				InvTextureSizeX = 1.0f/FontTexture->GetWidth();
				InvTextureSizeY = 1.0f/FontTexture->GetHeight();
			}

			const bool bIsWhitespace = FText::IsWhitespace(CurrentChar);

			if( !bIsWhitespace && PreviousCharEntry.IsValidEntry() )
			{
				Kerning = CharacterList.GetKerning( PreviousCharEntry, Entry );
			}
			else
			{
				Kerning = 0;
			}

			LineX += Kerning;
			PreviousCharEntry = Entry;

			if( !bIsWhitespace )
			{
				const float X = LineX + Entry.HorizontalOffset;
				// Note PosX,PosY is the upper left corner of the bounding box representing the string.  This computes the Y position of the baseline where text will sit

				const float Y = PosY - Entry.VerticalOffset+MaxHeight+Entry.GlobalDescender;
				const float U = Entry.StartU * InvTextureSizeX;
				const float V = Entry.StartV * InvTextureSizeY;
				const float SizeX = Entry.USize;
				const float SizeY = Entry.VSize;
				const float SizeU = Entry.USize * InvTextureSizeX;
				const float SizeV = Entry.VSize * InvTextureSizeY;

				FSlateRect CharRect( X, Y, X+SizeX, Y+SizeY );

				if( FSlateRect::DoRectanglesIntersect( LocalClipRect, CharRect ) )
				{
					TArray<FSlateVertex>& BatchVerticesRef = *BatchVertices;
					TArray<SlateIndex>& BatchIndicesRef = *BatchIndices;

					FVector2D UpperLeft( X, Y );
					FVector2D UpperRight( X+SizeX, Y );
					FVector2D LowerLeft( X, Y+SizeY );
					FVector2D LowerRight( X+SizeX, Y+SizeY );

					// Add four vertices for this quad
					BatchVerticesRef.AddUninitialized( 4 );
					// Add six indices for this quad
					BatchIndicesRef.AddUninitialized( 6 );

					// The start index of these vertices in the index buffer
					uint32 IndexStart = VertexOffset;

					// Add four vertices to the list of verts to be added to the vertex buffer
					BatchVerticesRef[ VertexOffset++ ] = FSlateVertex( RenderTransform, UpperLeft,								FVector2D(U,V),				FinalColor, RenderClipRect );
					BatchVerticesRef[ VertexOffset++ ] = FSlateVertex( RenderTransform, FVector2D(LowerRight.X,UpperLeft.Y),	FVector2D(U+SizeU, V),		FinalColor, RenderClipRect );
					BatchVerticesRef[ VertexOffset++ ] = FSlateVertex( RenderTransform, FVector2D(UpperLeft.X,LowerRight.Y),	FVector2D(U, V+SizeV),		FinalColor, RenderClipRect );
					BatchVerticesRef[ VertexOffset++ ] = FSlateVertex( RenderTransform, LowerRight,								FVector2D(U+SizeU, V+SizeV),FinalColor, RenderClipRect );

					BatchIndicesRef[IndexOffset++] = IndexStart + 0;
					BatchIndicesRef[IndexOffset++] = IndexStart + 1;
					BatchIndicesRef[IndexOffset++] = IndexStart + 2;
					BatchIndicesRef[IndexOffset++] = IndexStart + 1;
					BatchIndicesRef[IndexOffset++] = IndexStart + 3;
					BatchIndicesRef[IndexOffset++] = IndexStart + 2;
				}
			}

			LineX += Entry.XAdvance;
		}
	}
}

void FSlateElementBatcher::AddGradientElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);

	// There must be at least one gradient stop
	check( InPayload.GradientStops.Num() > 0 );

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::Default, InDrawEffects, InPayload.bGammaCorrect == false ? ESlateBatchDrawFlag::NoGamma : ESlateBatchDrawFlag::None, DrawElement.GetScissorRect() );
	TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
	TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];

	// Determine the four corners of the quad containing the gradient
	FVector2D TopLeft = FVector2D::ZeroVector;
	FVector2D TopRight = FVector2D(LocalSize.X, 0);
	FVector2D BotLeft = FVector2D(0, LocalSize.Y);
	FVector2D BotRight = FVector2D(LocalSize.X, LocalSize.Y);

	// Copy the gradient stops.. We may need to add more
	TArray<FSlateGradientStop> GradientStops = InPayload.GradientStops;

	const FSlateGradientStop& FirstStop = InPayload.GradientStops[0];
	const FSlateGradientStop& LastStop = InPayload.GradientStops[ InPayload.GradientStops.Num() - 1 ];
		
	// Determine if the first and last stops are not at the start and end of the quad
	// If they are not add a gradient stop with the same color as the first and/or last stop
	if( InPayload.GradientType == Orient_Vertical )
	{
		if( 0.0f < FirstStop.Position.X )
		{
			// The first stop is after the left side of the quad.  Add a stop at the left side of the quad using the same color as the first stop
			GradientStops.Insert( FSlateGradientStop( FVector2D(0.0, 0.0), FirstStop.Color ), 0 );
		}

		if( LocalSize.X > LastStop.Position.X )
		{
			// The last stop is before the right side of the quad.  Add a stop at the right side of the quad using the same color as the last stop
			GradientStops.Add( FSlateGradientStop( LocalSize, LastStop.Color ) ); 
		}
	}
	else
	{

		if( 0.0f < FirstStop.Position.Y )
		{
			// The first stop is after the top side of the quad.  Add a stop at the top side of the quad using the same color as the first stop
			GradientStops.Insert( FSlateGradientStop( FVector2D(0.0, 0.0), FirstStop.Color ), 0 );
		}

		if( LocalSize.Y > LastStop.Position.Y )
		{
			// The last stop is before the bottom side of the quad.  Add a stop at the bottom side of the quad using the same color as the last stop
			GradientStops.Add( FSlateGradientStop( LocalSize, LastStop.Color ) ); 
		}
	}

	uint32 IndexOffsetStart = BatchIndices.Num();

	// Add a pair of vertices for each gradient stop. Connecting them to the previous stop if necessary
	// Assumes gradient stops are sorted by position left to right or top to bottom
	for( int32 StopIndex = 0; StopIndex < GradientStops.Num(); ++StopIndex )
	{
		uint32 IndexStart = BatchVertices.Num();

		const FSlateGradientStop& CurStop = GradientStops[StopIndex];

		// The start vertex at this stop
		FVector2D StartPt;
		// The end vertex at this stop
		FVector2D EndPt;

		if( InPayload.GradientType == Orient_Vertical )
		{
			// Gradient stop is vertical so gradients to left to right
			StartPt = TopLeft;
			EndPt = BotLeft;
			// Gradient stops are interpreted in local space.
			StartPt.X += CurStop.Position.X;
			EndPt.X += CurStop.Position.X;
		}
		else
		{
			// Gradient stop is horizontal so gradients to top to bottom
			StartPt = TopLeft;
			EndPt = TopRight;
			// Gradient stops are interpreted in local space.
			StartPt.Y += CurStop.Position.Y;
			EndPt.Y += CurStop.Position.Y;
		}

		if( StopIndex == 0 )
		{
			// First stop does not have a full quad yet so do not create indices
			BatchVertices.Add( FSlateVertex(RenderTransform, StartPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false), RenderClipRect ) );
			BatchVertices.Add( FSlateVertex(RenderTransform, EndPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false), RenderClipRect ) );
		}
		else
		{
			// All stops after the first have indices and generate quads
			BatchVertices.Add( FSlateVertex(RenderTransform, StartPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false), RenderClipRect ) );
			BatchVertices.Add( FSlateVertex(RenderTransform, EndPt, FVector2D::ZeroVector, FVector2D::ZeroVector, CurStop.Color.ToFColor(false), RenderClipRect ) );

			// Connect the indices to the previous vertices
			BatchIndices.Add( IndexStart - 2 );
			BatchIndices.Add( IndexStart - 1 );
			BatchIndices.Add( IndexStart + 0 );

			BatchIndices.Add( IndexStart + 0 );
			BatchIndices.Add( IndexStart - 1 );
			BatchIndices.Add( IndexStart + 1 );
		}
	}
}

void FSlateElementBatcher::AddSplineElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	//const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);

	//@todo SLATE: Merge with AddLineElement?

	//@todo SLATE: This should probably be done in window space so there are no scaling artifacts?
	const float DirectLength = (InPayload.EndPt - InPayload.StartPt).Size();
	const float HandleLength = ((InPayload.EndPt - InPayload.EndDir) - (InPayload.StartPt + InPayload.StartDir)).Size();
	float NumSteps = FMath::Clamp<float>(FMath::CeilToInt(FMath::Max(DirectLength,HandleLength)/15.0f), 1, 256);

	// 1 is the minimum thickness we support
	// Thickness is given in screenspace, so convert it to local space before proceeding.
	float InThickness = FMath::Max( 1.0f, InverseLayoutTransform.GetScale() * InPayload.Thickness );

	// The radius to use when checking the distance of pixels to the actual line.  Arbitrary value based on what looks the best
	const float Radius = 1.5f;

	// Compute the actual size of the line we need based on thickness.  Need to ensure pixels that are at least Thickness/2 + Sample radius are generated so that we have enough pixels to blend.
	// The idea for the spline anti-alising technique is based on the fast prefiltered lines technique published in GPU Gems 2 
	const float LineThickness = FMath::CeilToInt( (2.0f * Radius + InThickness ) * FMath::Sqrt(2.0f) );

	// The amount we increase each side of the line to generate enough pixels
	const float HalfThickness = LineThickness * .5f + Radius;

	// Find a batch for the element
	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams::MakePixelShaderParams( FVector4( InPayload.Thickness,Radius,0,0) ), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::LineSegment, InDrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetScissorRect() );
	TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
	TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];


	const FVector2D StartPt = InPayload.StartPt;
	const FVector2D StartDir = InPayload.StartDir;
	const FVector2D EndPt = InPayload.EndPt;
	const FVector2D EndDir = InPayload.EndDir;
	
	// Compute the normal to the line
	FVector2D Normal = FVector2D( StartPt.Y - EndPt.Y, EndPt.X - StartPt.X ).GetSafeNormal();

	FVector2D Up = Normal * HalfThickness;

	// Generate the first segment
	const float Alpha = 1.0f/NumSteps;
	FVector2D StartPos = StartPt;
	FVector2D EndPos = FVector2D( FMath::CubicInterp( StartPt, StartDir, EndPt, EndDir, Alpha ) );

	FColor FinalColor = GetElementColor( InPayload.Tint, nullptr );

	BatchVertices.Add( FSlateVertex( RenderTransform, StartPos + Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), FinalColor, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, StartPos - Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), FinalColor, RenderClipRect ) );

	// Generate the rest of the segments
	for( int32 Step = 0; Step < NumSteps; ++Step )
	{
		// Skip the first step as it was already generated
		if( Step > 0 )
		{
			const float StepAlpha = (Step+1.0f)/NumSteps;
			EndPos = FVector2D( FMath::CubicInterp( StartPt, StartDir, EndPt, EndDir, StepAlpha ) );
		}

		int32 IndexStart = BatchVertices.Num();

		// Compute the normal to the line
		FVector2D SegmentNormal = FVector2D( StartPos.Y - EndPos.Y, EndPos.X - StartPos.X ).GetSafeNormal();

		// Create the new vertices for the thick line segment
		Up = SegmentNormal * HalfThickness;

		BatchVertices.Add( FSlateVertex( RenderTransform, EndPos + Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), FinalColor, RenderClipRect ) );
		BatchVertices.Add( FSlateVertex( RenderTransform, EndPos - Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), FinalColor, RenderClipRect ) );

		BatchIndices.Add( IndexStart - 2 );
		BatchIndices.Add( IndexStart - 1 );
		BatchIndices.Add( IndexStart + 0 );

		BatchIndices.Add( IndexStart + 0 );
		BatchIndices.Add( IndexStart + 1 );
		BatchIndices.Add( IndexStart - 1 );

		StartPos = EndPos;
	}
}


/**
 * Calculates the intersection of two line segments P1->P2, P3->P4
 * The tolerance setting is used when the lines arent currently intersecting but will intersect in the future  
 * The higher the tolerance the greater the distance that the intersection point can be.
 *
 * @return true if the line intersects.  Populates Intersection
 */
static bool LineIntersect( const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, const FVector2D& P4, FVector2D& Intersect, float Tolerance = .1f)
{
	float NumA = ( (P4.X-P3.X)*(P1.Y-P3.Y) - (P4.Y-P3.Y)*(P1.X-P3.X) );
	float NumB =  ( (P2.X-P1.X)*(P1.Y-P3.Y) - (P2.Y-P1.Y)*(P1.X-P3.X) );

	float Denom = (P4.Y-P3.Y)*(P2.X-P1.X) - (P4.X-P3.X)*(P2.Y-P1.Y);

	if( FMath::IsNearlyZero( NumA ) && FMath::IsNearlyZero( NumB )  )
	{
		// Lines are the same
		Intersect = (P1 + P2) / 2;
		return true;
	}

	if( FMath::IsNearlyZero(Denom) )
	{
		// Lines are parallel
		return false;
	}
	 
	float B = NumB / Denom;
	float A = NumA / Denom;

	// Note that this is a "tweaked" intersection test for the purpose of joining line segments.  We don't just want to know if the line segements
	// Intersect, but where they would if they don't currently. Except that we dont care in the case that where the segments 
	// intersection is so far away that its infeasible to use the intersection point later.
	if( A >= -Tolerance && A <= (1.0f + Tolerance ) && B >= -Tolerance && B <= (1.0f + Tolerance) )
	{
		Intersect = P1+A*(P2-P1);
		return true;
	}

	return false;
}
	

void FSlateElementBatcher::AddLineElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	//const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type DrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);

	if( InPayload.Points.Num() < 2 )
	{
		return;
	}

	FColor FinalColor = GetElementColor( InPayload.Tint, nullptr );

	if( InPayload.bAntialias )
	{
		// The radius to use when checking the distance of pixels to the actual line.  Arbitrary value based on what looks the best
		const float Radius = 1.5f;

		// Thickness is given in screenspace, so convert it to local space before proceeding.
		float RequestedThickness = FMath::Max( 1.0f, InverseLayoutTransform.GetScale() * InPayload.Thickness );

		// Compute the actual size of the line we need based on thickness.  Need to ensure pixels that are at least Thickness/2 + Sample radius are generated so that we have enough pixels to blend.
		// The idea for the anti-aliasing technique is based on the fast prefiltered lines technique published in GPU Gems 2 
		const float LineThickness = FMath::CeilToInt( (2.0f * Radius + RequestedThickness ) * FMath::Sqrt(2.0f) );

		// The amount we increase each side of the line to generate enough pixels
		const float HalfThickness = LineThickness * .5f + Radius;

		// Find a batch for the element
		FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams::MakePixelShaderParams( FVector4(RequestedThickness,Radius,0,0) ), nullptr, ESlateDrawPrimitive::TriangleList, ESlateShader::LineSegment, DrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetScissorRect() );
		TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
		TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];

		const TArray<FVector2D>& Points = InPayload.Points;

		FVector2D StartPos = Points[0];
		FVector2D EndPos = Points[1];

		FVector2D Normal = FVector2D( StartPos.Y - EndPos.Y, EndPos.X - StartPos.X ).GetSafeNormal();

		FVector2D Up = Normal * HalfThickness;

		BatchVertices.Add( FSlateVertex( RenderTransform, StartPos + Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), FinalColor, RenderClipRect ) );
		BatchVertices.Add( FSlateVertex( RenderTransform, StartPos - Up, TransformPoint(RenderTransform, StartPos), TransformPoint(RenderTransform, EndPos), FinalColor, RenderClipRect ) );

		// Generate the rest of the segments
		for( int32 Point = 1; Point < Points.Num(); ++Point )
		{
			EndPos = Points[Point];
			// Determine if we should check the intersection point with the next line segment.
			// We will adjust were this line ends to the intersection
			bool bCheckIntersection = Points.IsValidIndex( Point+1 ) ;
			uint32 IndexStart = BatchVertices.Num();

			// Compute the normal to the line
			Normal = FVector2D( StartPos.Y - EndPos.Y, EndPos.X - StartPos.X ).GetSafeNormal();

			// Create the new vertices for the thick line segment
			Up = Normal * HalfThickness;

			FVector2D IntersectUpper = EndPos + Up;
			FVector2D IntersectLower = EndPos - Up;
			FVector2D IntersectCenter = EndPos;

			if( bCheckIntersection )
			{
				// The end point of the next segment
				const FVector2D NextEndPos = Points[Point+1];

				// The normal of the next segment
				const FVector2D NextNormal = FVector2D( EndPos.Y - NextEndPos.Y, NextEndPos.X - EndPos.X ).GetSafeNormal();

				// The next amount to adjust the vertices by 
				FVector2D NextUp = NextNormal * HalfThickness;

				FVector2D IntersectionPoint;
				if( LineIntersect( StartPos + Up, EndPos + Up, EndPos + NextUp, NextEndPos + NextUp, IntersectionPoint ) )
				{
					// If the lines intersect adjust where the line starts
					IntersectUpper = IntersectionPoint;

					// visualizes the intersection
					//AddQuadElement( IntersectUpper-FVector2D(1,1), FVector2D(2,2), 1, InClippingRect, Layer+1, FColor::Red);

				}

				if( LineIntersect( StartPos - Up, EndPos - Up, EndPos - NextUp, NextEndPos - NextUp, IntersectionPoint ) )
				{
					// If the lines intersect adjust where the line starts
					IntersectLower = IntersectionPoint;

					// visualizes the intersection
					//AddQuadElement( IntersectLower-FVector2D(1,1), FVector2D(2,2), 1, InClippingRect, Layer+1, FColor::Yellow);
				}
				// the midpoint of the intersection.  Used as the new end to the line segment (not adjusted for anti-aliasing)
				IntersectCenter = (IntersectUpper+IntersectLower) * .5f;
			}

			// We use these points when making the copy of the vert below, so go ahead and cache it.
			FVector2D StartPosRenderSpace = TransformPoint(RenderTransform, StartPos);
			FVector2D IntersectCenterRenderSpace = TransformPoint(RenderTransform, IntersectCenter);

			if( Point > 1 )
			{
				// Make a copy of the last two vertices and update their start and end position to reflect the new line segment
				FSlateVertex StartV1 = BatchVertices[IndexStart-1];
				FSlateVertex StartV2 = BatchVertices[IndexStart-2];

				StartV1.TexCoords[0] = StartPosRenderSpace.X;
				StartV1.TexCoords[1] = StartPosRenderSpace.Y;
				StartV1.TexCoords[2] = IntersectCenterRenderSpace.X;
				StartV1.TexCoords[3] = IntersectCenterRenderSpace.Y;

				StartV2.TexCoords[0] = StartPosRenderSpace.X;
				StartV2.TexCoords[1] = StartPosRenderSpace.Y;
				StartV2.TexCoords[2] = IntersectCenterRenderSpace.X;
				StartV2.TexCoords[3] = IntersectCenterRenderSpace.Y;

				IndexStart += 2;
				BatchVertices.Add( StartV2 );
				BatchVertices.Add( StartV1 );
			}

			BatchVertices.Add( FSlateVertex( RenderTransform, IntersectUpper, StartPosRenderSpace, IntersectCenterRenderSpace, FinalColor, RenderClipRect ) );
			BatchVertices.Add( FSlateVertex( RenderTransform, IntersectLower, StartPosRenderSpace, IntersectCenterRenderSpace, FinalColor, RenderClipRect ) );

			BatchIndices.Add( IndexStart - 1 );
			BatchIndices.Add( IndexStart - 2 );
			BatchIndices.Add( IndexStart + 0 );

			BatchIndices.Add( IndexStart + 0 );
			BatchIndices.Add( IndexStart + 1 );
			BatchIndices.Add( IndexStart - 1 );

			StartPos = EndPos;
		}
	}
	else
	{
		// Find a batch for the element
		FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), nullptr, ESlateDrawPrimitive::LineList, ESlateShader::Default, DrawEffects, ESlateBatchDrawFlag::None, DrawElement.GetScissorRect() );
		TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
		TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];

		// Generate the rest of the segments
		for( int32 Point = 0; Point < InPayload.Points.Num()-1; ++Point )
		{
			uint32 IndexStart = BatchVertices.Num();
			FVector2D StartPos = InPayload.Points[Point];
			FVector2D EndPos = InPayload.Points[Point+1];


			BatchVertices.Add( FSlateVertex( RenderTransform, StartPos, FVector2D::ZeroVector, FinalColor, RenderClipRect ) );
			BatchVertices.Add( FSlateVertex( RenderTransform, EndPos, FVector2D::ZeroVector, FinalColor, RenderClipRect ) );

			BatchIndices.Add( IndexStart );
			BatchIndices.Add( IndexStart + 1 );
		}
	}
}


void FSlateElementBatcher::AddViewportElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);

	const FColor FinalColor = GetElementColor( InPayload.Tint, nullptr );

	ESlateBatchDrawFlag::Type DrawFlags = ESlateBatchDrawFlag::None;
	
	if( !InPayload.bAllowBlending )
	{
		DrawFlags |= ESlateBatchDrawFlag::NoBlending;
	}

	if( !InPayload.bGammaCorrect )
	{
		// Don't apply gamma correction
		DrawFlags |= ESlateBatchDrawFlag::NoGamma;
	}

	TSharedPtr<const ISlateViewport> ViewportPin = InPayload.Viewport.Pin();

	FSlateShaderResource* ViewportResource =  nullptr;
	ESlateShader::Type ShaderType = ESlateShader::Default;

	if( ViewportPin.IsValid() )
	{
		ViewportResource = ViewportPin->GetViewportRenderTargetTexture();

		if( ViewportPin->IsViewportTextureAlphaOnly() )
		{
			// This is a slight hack, but the font shader is the same as the general shader except it reads alpha only textures
			ShaderType = ESlateShader::Font;
		}
	}

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, FShaderParams(), ViewportResource, ESlateDrawPrimitive::TriangleList, ShaderType, InDrawEffects, DrawFlags, DrawElement.GetScissorRect() );
	TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ElementBatch.VertexArrayIndex];
	TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ElementBatch.IndexArrayIndex];

	// Tag this batch as requiring vsync if the viewport requires it.
	if( ViewportPin.IsValid() )
	{
		bRequiresVsync |= ViewportPin->RequiresVsync();
	}

	// Do pixel snapping
	FVector2D TopLeft(0,0);
	FVector2D BotRight(LocalSize);

	FVector2D TopRight = FVector2D( BotRight.X, TopLeft.Y);
	FVector2D BotLeft =	 FVector2D( TopLeft.X, BotRight.Y);

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();

	// Add four vertices to the list of verts to be added to the vertex buffer
	BatchVertices.Add( FSlateVertex( RenderTransform, TopLeft,	FVector2D(0.0f,0.0f),	FinalColor, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, TopRight,	FVector2D(1.0f,0.0f),	FinalColor, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, BotLeft,	FVector2D(0.0f,1.0f),	FinalColor, RenderClipRect ) );
	BatchVertices.Add( FSlateVertex( RenderTransform, BotRight,	FVector2D(1.0f,1.0f),	FinalColor, RenderClipRect ) );

	// The offset into the index buffer where this quads indices start
	uint32 IndexOffsetStart = BatchIndices.Num();

	// Add 6 indices to the vertex buffer.  (2 tri's per quad, 3 indices per tri)
	BatchIndices.Add( IndexStart + 0 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 2 );

	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 3 );

}


void FSlateElementBatcher::AddBorderElement( const FSlateDrawElement& DrawElement )
{
	const FSlateRenderTransform& RenderTransform = DrawElement.GetRenderTransform();
	//const FVector2D& InPosition = DrawElement.GetPosition();
	//const FVector2D& Size = DrawElement.GetSize();
	const FVector2D& LocalSize = DrawElement.GetLocalSize();
	//float Scale = DrawElement.GetScale();
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	const FSlateRect& InClippingRect = DrawElement.GetClippingRect();
	ESlateDrawEffect::Type InDrawEffects = DrawElement.GetDrawEffects();
	uint32 Layer = DrawElement.GetLayer();

	// extract the layout transform from the draw element
	FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(DrawElement.GetScale(), DrawElement.GetPosition())));
	FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(InClippingRect, InverseLayoutTransform, RenderTransform);

	check( InPayload.BrushResource );

	uint32 TextureWidth = 1;
	uint32 TextureHeight = 1;

	// Currently borders are not atlased because they are tiled.  So we just assume the texture proxy holds the actual texture
	FSlateShaderResourceProxy* ResourceProxy = ResourceManager.GetShaderResource( *InPayload.BrushResource );
	FSlateShaderResource* Resource = ResourceProxy ? ResourceProxy->Resource : nullptr;
	if( Resource )
	{
		TextureWidth = Resource->GetWidth();
		TextureHeight = Resource->GetHeight();
	}
	FVector2D TextureSizeLocalSpace = TransformVector(InverseLayoutTransform, FVector2D(TextureWidth, TextureHeight));
 
	// Texel offset
	const FVector2D HalfTexel( PixelCenterOffset/TextureWidth, PixelCenterOffset/TextureHeight );

	const FVector2D StartUV = HalfTexel;
	const FVector2D EndUV = FVector2D( 1.0f, 1.0f ) + HalfTexel;

	const FMargin& Margin = InPayload.BrushResource->Margin;

	// Do pixel snapping
	FVector2D TopLeft(0,0);
	FVector2D BotRight(LocalSize);
	// Determine the margins for each quad
	FVector2D TopLeftMargin(TextureSizeLocalSpace * FVector2D(Margin.Left, Margin.Top));
	FVector2D BotRightMargin(LocalSize - TextureSizeLocalSpace * FVector2D(Margin.Right, Margin.Bottom));

	float LeftMarginX = TopLeftMargin.X;
	float TopMarginY = TopLeftMargin.Y;
	float RightMarginX = BotRightMargin.X;
	float BottomMarginY = BotRightMargin.Y;

	// If the margins are overlapping the margins are too big or the button is too small
	// so clamp margins to half of the box size
	if( RightMarginX < LeftMarginX )
	{
		LeftMarginX = LocalSize.X / 2;
		RightMarginX = LeftMarginX;
	}

	if( BottomMarginY < TopMarginY )
	{
		TopMarginY = LocalSize.Y / 2;
		BottomMarginY = TopMarginY;
	}

	// Determine the texture coordinates for each quad
	float LeftMarginU = (Margin.Left > 0.0f) ? Margin.Left : 0.0f;
	float TopMarginV = (Margin.Top > 0.0f) ? Margin.Top : 0.0f;
	float RightMarginU = (Margin.Right > 0.0f) ? 1.0f - Margin.Right : 1.0f;
	float BottomMarginV = (Margin.Bottom > 0.0f) ? 1.0f - Margin.Bottom : 1.0f;

	LeftMarginU += HalfTexel.X;
	TopMarginV += HalfTexel.Y;
	BottomMarginV += HalfTexel.Y;
	RightMarginU += HalfTexel.X;

	// Determine the amount of tiling needed for the texture in this element.  The formula is number of pixels covered by the tiling portion of the texture / the number number of texels corresponding to the tiled portion of the texture.
	float TopTiling = (RightMarginX-LeftMarginX)/(TextureSizeLocalSpace.X * ( 1 - Margin.GetTotalSpaceAlong<Orient_Horizontal>() ));
	float LeftTiling = (BottomMarginY-TopMarginY)/(TextureSizeLocalSpace.Y * ( 1 - Margin.GetTotalSpaceAlong<Orient_Vertical>() ));
	
	FShaderParams ShaderParams = FShaderParams::MakePixelShaderParams( FVector4(LeftMarginU,RightMarginU,TopMarginV,BottomMarginV) );

	// The tint color applies to all brushes and is passed per vertex
	FColor Tint = GetElementColor( InPayload.Tint, InPayload.BrushResource );

	// Pass the tiling information as a flag so we can pick the correct texture addressing mode
	ESlateBatchDrawFlag::Type DrawFlags = (ESlateBatchDrawFlag::TileU|ESlateBatchDrawFlag::TileV);

	FSlateElementBatch& ElementBatch = FindBatchForElement( Layer, ShaderParams, Resource, ESlateDrawPrimitive::TriangleList, ESlateShader::Border, InDrawEffects, DrawFlags, DrawElement.GetScissorRect() );
	TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ ElementBatch.VertexArrayIndex ];
	TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ ElementBatch.IndexArrayIndex ];

	// Ensure tiling of at least 1.  
	TopTiling = TopTiling >= 1.0f ? TopTiling : 1.0f;
	LeftTiling = LeftTiling >= 1.0f ? LeftTiling : 1.0f;
	float RightTiling = LeftTiling;
	float BottomTiling = TopTiling;

	FVector2D Position = TopLeft;
	FVector2D EndPos = BotRight;

	// The start index of these vertices in the index buffer
	uint32 IndexStart = BatchVertices.Num();

	// Zero for second UV indicates no tiling and to just pass the UV though (for the corner sections)
	FVector2D Zero(0,0);

	// Add all the vertices needed for this element.  Vertices are duplicated so that we can have some sections with no tiling and some with tiling.
	BatchVertices.Add( FSlateVertex( RenderTransform, Position,									FVector2D( StartUV.X, StartUV.Y ),			Zero,							Tint, RenderClipRect ) ); //0
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, TopMarginY ),		FVector2D( StartUV.X, TopMarginV ),			Zero,							Tint, RenderClipRect ) ); //1
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, Position.Y ),		FVector2D( LeftMarginU, StartUV.Y ),		Zero,							Tint, RenderClipRect ) ); //2
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		FVector2D( LeftMarginU, TopMarginV ),		Zero,							Tint, RenderClipRect ) ); //3
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, Position.Y ),		FVector2D( StartUV.X, StartUV.Y ),			FVector2D(TopTiling, 0.0f),		Tint, RenderClipRect ) ); //4
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		FVector2D( StartUV.X, TopMarginV ),			FVector2D(TopTiling, 0.0f),		Tint, RenderClipRect ) ); //5
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, Position.Y ),	FVector2D( EndUV.X, StartUV.Y ),			FVector2D(TopTiling, 0.0f),		Tint, RenderClipRect ) ); //6
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	FVector2D( EndUV.X, TopMarginV),			FVector2D(TopTiling, 0.0f),		Tint, RenderClipRect ) ); //7
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, Position.Y ),	FVector2D( RightMarginU, StartUV.Y ),		Zero,							Tint, RenderClipRect ) ); //8
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	FVector2D( RightMarginU, TopMarginV),		Zero,							Tint, RenderClipRect ) ); //9
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, Position.Y ),		FVector2D( EndUV.X, StartUV.Y ),			Zero,							Tint, RenderClipRect ) ); //10
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, TopMarginY ),		FVector2D( EndUV.X, TopMarginV),			Zero,							Tint, RenderClipRect ) ); //11
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, TopMarginY ),		FVector2D( StartUV.X, StartUV.Y ),			FVector2D(0.0f, LeftTiling),	Tint, RenderClipRect ) ); //12
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, BottomMarginY ),	FVector2D( StartUV.X, EndUV.Y ),			FVector2D(0.0f, LeftTiling),	Tint, RenderClipRect ) ); //13
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, TopMarginY ),		FVector2D( LeftMarginU, StartUV.Y ),		FVector2D(0.0f, LeftTiling),	Tint, RenderClipRect ) ); //14
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	FVector2D( LeftMarginU, EndUV.Y ),			FVector2D(0.0f, LeftTiling),	Tint, RenderClipRect ) ); //15
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, TopMarginY ),	FVector2D( RightMarginU, StartUV.Y ),		FVector2D(0.0f, RightTiling),	Tint, RenderClipRect ) ); //16
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, BottomMarginY ),	FVector2D( RightMarginU, EndUV.Y ),			FVector2D(0.0f, RightTiling),	Tint, RenderClipRect ) ); //17
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, TopMarginY ),		FVector2D( EndUV.X, StartUV.Y ),			FVector2D(0.0f, RightTiling),	Tint, RenderClipRect ) ); //18
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, BottomMarginY ),		FVector2D( EndUV.X, EndUV.Y ),				FVector2D(0.0f, RightTiling),	Tint, RenderClipRect ) ); //19
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, BottomMarginY ),	FVector2D( StartUV.X, BottomMarginV ),		Zero,							Tint, RenderClipRect ) ); //20
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( Position.X, EndPos.Y ),		FVector2D( StartUV.X, EndUV.Y ),			Zero,							Tint, RenderClipRect ) ); //21
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	FVector2D( LeftMarginU, BottomMarginV ),	Zero,							Tint, RenderClipRect ) ); //22
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, EndPos.Y ),		FVector2D( LeftMarginU, EndUV.Y ),			Zero,							Tint, RenderClipRect ) ); //23
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, BottomMarginY ),	FVector2D( StartUV.X, BottomMarginV ),		FVector2D(BottomTiling, 0.0f),	Tint, RenderClipRect ) ); //24
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( LeftMarginX, EndPos.Y ),		FVector2D( StartUV.X, EndUV.Y ),			FVector2D(BottomTiling, 0.0f),	Tint, RenderClipRect ) ); //25
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX,BottomMarginY ),	FVector2D( EndUV.X, BottomMarginV ),		FVector2D(BottomTiling, 0.0f),	Tint, RenderClipRect ) ); //26
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, EndPos.Y ),		FVector2D( EndUV.X, EndUV.Y ),				FVector2D(BottomTiling, 0.0f),	Tint, RenderClipRect ) ); //27
	
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, BottomMarginY ),	FVector2D( RightMarginU, BottomMarginV ),	Zero,							Tint, RenderClipRect ) ); //29
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( RightMarginX, EndPos.Y ),		FVector2D( RightMarginU, EndUV.Y ),			Zero,							Tint, RenderClipRect ) ); //30
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, BottomMarginY ),		FVector2D( EndUV.X, BottomMarginV ),		Zero,							Tint, RenderClipRect ) ); //31
	BatchVertices.Add( FSlateVertex( RenderTransform, FVector2D( EndPos.X, EndPos.Y ),			FVector2D( EndUV.X, EndUV.Y ),				Zero,							Tint, RenderClipRect ) ); //32


	// The offset into the index buffer where this elements indices start
	uint32 IndexOffsetStart = BatchIndices.Num();

	// Top
	BatchIndices.Add( IndexStart + 0 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 2 );
	BatchIndices.Add( IndexStart + 1 );
	BatchIndices.Add( IndexStart + 3 );

	BatchIndices.Add( IndexStart + 4 );
	BatchIndices.Add( IndexStart + 5 );
	BatchIndices.Add( IndexStart + 6 );
	BatchIndices.Add( IndexStart + 6 );
	BatchIndices.Add( IndexStart + 5 );
	BatchIndices.Add( IndexStart + 7 );

	BatchIndices.Add( IndexStart + 8 );
	BatchIndices.Add( IndexStart + 9 );
	BatchIndices.Add( IndexStart + 10 );
	BatchIndices.Add( IndexStart + 10 );
	BatchIndices.Add( IndexStart + 9 );
	BatchIndices.Add( IndexStart + 11 );

	// Middle
	BatchIndices.Add( IndexStart + 12 );
	BatchIndices.Add( IndexStart + 13 );
	BatchIndices.Add( IndexStart + 14 );
	BatchIndices.Add( IndexStart + 14 );
	BatchIndices.Add( IndexStart + 13 );
	BatchIndices.Add( IndexStart + 15 );

	BatchIndices.Add( IndexStart + 16 );
	BatchIndices.Add( IndexStart + 17 );
	BatchIndices.Add( IndexStart + 18 );
	BatchIndices.Add( IndexStart + 18 );
	BatchIndices.Add( IndexStart + 17 );
	BatchIndices.Add( IndexStart + 19 );

	// Bottom
	BatchIndices.Add( IndexStart + 20 );
	BatchIndices.Add( IndexStart + 21 );
	BatchIndices.Add( IndexStart + 22 );
	BatchIndices.Add( IndexStart + 22 );
	BatchIndices.Add( IndexStart + 21 );
	BatchIndices.Add( IndexStart + 23 );

	BatchIndices.Add( IndexStart + 24 );
	BatchIndices.Add( IndexStart + 25 );
	BatchIndices.Add( IndexStart + 26 );
	BatchIndices.Add( IndexStart + 26 );
	BatchIndices.Add( IndexStart + 25 );
	BatchIndices.Add( IndexStart + 27 );

	BatchIndices.Add( IndexStart + 28 );
	BatchIndices.Add( IndexStart + 29 );
	BatchIndices.Add( IndexStart + 30 );
	BatchIndices.Add( IndexStart + 30 );
	BatchIndices.Add( IndexStart + 29 );
	BatchIndices.Add( IndexStart + 31 );
}


void FSlateElementBatcher::AddCustomElement( const FSlateDrawElement& DrawElement )
{
	const FSlateDataPayload& InPayload = DrawElement.GetDataPayload();
	uint32 Layer = DrawElement.GetLayer();

	if( InPayload.CustomDrawer.IsValid() )
	{
		// See if the layer already exists.
		TSet<FSlateElementBatch>* ElementBatches = LayerToElementBatches.Find( Layer );
		if( !ElementBatches )
		{
			// The layer doesn't exist so make it now
			ElementBatches = &LayerToElementBatches.Add( Layer, TSet<FSlateElementBatch>() );
		}
		check( ElementBatches );


		// Custom elements are not batched together 
		ElementBatches->Add( FSlateElementBatch( InPayload.CustomDrawer, DrawElement.GetScissorRect() ) );
	}
}


FSlateElementBatch& FSlateElementBatcher::FindBatchForElement( 
	uint32 Layer, 
	const FShaderParams& ShaderParams, 
	const FSlateShaderResource* InTexture, 
	ESlateDrawPrimitive::Type PrimitiveType,
	ESlateShader::Type ShaderType, 
	ESlateDrawEffect::Type DrawEffects, 
	ESlateBatchDrawFlag::Type DrawFlags,
	const TOptional<FShortRect>& ScissorRect)
{
	SLATE_CYCLE_COUNTER_SCOPE_DETAILED(SLATE_STATS_DETAIL_LEVEL_HI, GSlateFindBatchTime);

	// See if the layer already exists.
	TSet<FSlateElementBatch>* ElementBatches = LayerToElementBatches.Find( Layer );
	if( !ElementBatches )
	{
		// The layer doesn't exist so make it now
		ElementBatches = &LayerToElementBatches.Add( Layer, TSet<FSlateElementBatch>() );
	}
	check( ElementBatches );

	// Create a temp batch so we can use it as our key to find if the same batch already exists
	FSlateElementBatch TempBatch( InTexture, ShaderParams, ShaderType, PrimitiveType, DrawEffects, DrawFlags, ScissorRect );

	FSlateElementBatch* ElementBatch = ElementBatches->Find( TempBatch );
	if( !ElementBatch )
	{
		// No batch with the specified parameter exists.  Create it from the temp batch.
		FSetElementId ID = ElementBatches->Add( TempBatch );
		ElementBatch = &(*ElementBatches)[ID];

		// Get a free vertex array
		if( VertexArrayFreeList.Num() > 0 )
		{
			ElementBatch->VertexArrayIndex = VertexArrayFreeList.Pop(/*bAllowShrinking=*/ false);
			BatchVertexArrays[ElementBatch->VertexArrayIndex].Reserve(200);		
		}
		else
		{
			// There are no free vertex arrays so we must add one		
			uint32 NewIndex = BatchVertexArrays.Add( TArray<FSlateVertex>() );
			BatchVertexArrays[NewIndex].Reserve(200);		

			ElementBatch->VertexArrayIndex = NewIndex;
		}

		// Get a free index array
		if( IndexArrayFreeList.Num() > 0 )
		{
			ElementBatch->IndexArrayIndex = IndexArrayFreeList.Pop(/*bAllowShrinking=*/ false);
			BatchIndexArrays[ElementBatch->IndexArrayIndex].Reserve(200);		
		}
		else
		{
			// There are no free index arrays so we must add one
			uint32 NewIndex = BatchIndexArrays.Add( TArray<SlateIndex>() );
			BatchIndexArrays[NewIndex].Reserve(500);	

			ElementBatch->IndexArrayIndex = NewIndex;

			check( BatchIndexArrays.IsValidIndex( ElementBatch->IndexArrayIndex ) );
		}
		
	}
	check( ElementBatch );

	// Increment the number of elements in the batch.
	++ElementBatch->NumElementsInBatch;
	return *ElementBatch;
}


void FSlateElementBatcher::AddVertices( TArray<FSlateVertex>& OutVertices, FSlateElementBatch& ElementBatch, const TArray<FSlateVertex>& VertexBatch )
{
	uint32 FirstIndex = OutVertices.Num();
	OutVertices.AddUninitialized( VertexBatch.Num() );

	uint32 RequiredSize =  VertexBatch.Num() * VertexBatch.GetTypeSize();

	FMemory::Memcpy( &OutVertices[FirstIndex], VertexBatch.GetData(), RequiredSize );

	RequiredVertexMemory += RequiredSize;
	TotalVertexMemory += VertexBatch.GetAllocatedSize();
	NumVertices += VertexBatch.Num();

	ElementBatch.VertexOffset = FirstIndex;
	ElementBatch.NumVertices = VertexBatch.Num();
}


void FSlateElementBatcher::AddIndices( TArray<SlateIndex>& OutIndices, FSlateElementBatch& ElementBatch, const TArray<SlateIndex>& IndexBatch )
{
	uint32 FirstIndex = OutIndices.Num();
	OutIndices.AddUninitialized( IndexBatch.Num() );

	uint32 RequiredSize =  IndexBatch.Num() * IndexBatch.GetTypeSize();

	FMemory::Memcpy( &OutIndices[FirstIndex], IndexBatch.GetData(), RequiredSize );

	RequiredIndexMemory += RequiredSize;
	TotalIndexMemory += IndexBatch.GetAllocatedSize();

	ElementBatch.IndexOffset = FirstIndex;
	ElementBatch.NumIndices = IndexBatch.Num();
}


void FSlateElementBatcher::FillBatchBuffers( FSlateWindowElementList& WindowElementList, bool& bRequiresStencilTest )
{
	SLATE_CYCLE_COUNTER_SCOPE(GSlateFillBatchBuffers);

	TArray<FSlateRenderBatch>& OutRenderBatches = WindowElementList.RenderBatches;
	TArray<FSlateVertex>& OutBatchedVertices = WindowElementList.BatchedVertices;
	TArray<SlateIndex>& OutBatchedIndices = WindowElementList.BatchedIndices;

	check( OutRenderBatches.Num() == 0 && OutBatchedVertices.Num() == 0 && OutBatchedIndices.Num() == 0 );

	// Sort by layer
	LayerToElementBatches.KeySort( TLess<uint32>() );


	STAT(NumLayers = FMath::Max<uint32>(NumLayers, LayerToElementBatches.Num() ));

	bRequiresStencilTest = false;
	// For each element batch add its vertices and indices to the bulk lists.
	for( TMap< uint32, TSet<FSlateElementBatch> >::TIterator It( LayerToElementBatches ); It; ++It )
	{
		TSet<FSlateElementBatch>& ElementBatches = It.Value();

		STAT(NumBatches += ElementBatches.Num());
		for( TSet<FSlateElementBatch>::TIterator BatchIt(ElementBatches); BatchIt; ++BatchIt )
		{
			FSlateElementBatch& ElementBatch = *BatchIt;

			bool bIsMaterial = ElementBatch.GetShaderResource() && ElementBatch.GetShaderResource()->GetType() == ESlateShaderResource::Material;

			if( !ElementBatch.GetCustomDrawer().IsValid() )
			{
				if( ElementBatch.GetShaderType() == ESlateShader::LineSegment )
				{
					bRequiresStencilTest = true;
				}

				TArray<FSlateVertex>& BatchVertices = BatchVertexArrays[ ElementBatch.VertexArrayIndex ];
				// after this loop, we'll be done with the array, so put it back on the free list.
				VertexArrayFreeList.Add( ElementBatch.VertexArrayIndex );

				TArray<SlateIndex>& BatchIndices = BatchIndexArrays[ ElementBatch.IndexArrayIndex ];
				// after this loop, we'll be done with the array, so put it back on the free list.
				IndexArrayFreeList.Add( ElementBatch.IndexArrayIndex );

				// We should have at least some vertices and indices in the batch or none at all
				check( BatchVertices.Num() > 0 && BatchIndices.Num() > 0  || BatchVertices.Num() == 0 && BatchIndices.Num() == 0 );

				if( BatchVertices.Num() > 0 && BatchIndices.Num() > 0  )
				{
					AddVertices( OutBatchedVertices, ElementBatch, BatchVertices );
					AddIndices( OutBatchedIndices, ElementBatch, BatchIndices );

					OutRenderBatches.Add( FSlateRenderBatch( ElementBatch ) );

					// Done with the batch
					BatchVertices.Empty(BatchVertices.Num());
					BatchIndices.Empty(BatchIndices.Num());
				}

			}
			else
			{
				OutRenderBatches.Add( FSlateRenderBatch( ElementBatch ) );
			}
		}
	}

}


void FSlateElementBatcher::ResetBatches()
{
	LayerToElementBatches.Reset();
	bRequiresVsync = false;
}


void FSlateElementBatcher::ResetStats()
{
	SET_DWORD_STAT( STAT_SlateNumLayers, NumLayers );
	SET_DWORD_STAT( STAT_SlateNumBatches, NumBatches );
	SET_DWORD_STAT( STAT_SlateVertexCount, NumVertices );
	SET_MEMORY_STAT( STAT_SlateVertexBatchMemory, TotalVertexMemory );
	SET_MEMORY_STAT( STAT_SlateIndexBatchMemory, TotalIndexMemory );

	NumLayers = 0;
	NumBatches = 0;
	NumVertices = 0;
	RequiredIndexMemory = 0;
	RequiredVertexMemory = 0;
	TotalVertexMemory = 0;
	TotalIndexMemory = 0;
}
