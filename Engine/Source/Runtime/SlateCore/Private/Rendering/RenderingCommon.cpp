// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Rendering/RenderingCommon.h"

FSlateRotatedRect::FSlateRotatedRect() {}

FSlateRotatedRect::FSlateRotatedRect(const FSlateRect& AlignedRect)
	: TopLeft(AlignedRect.Left, AlignedRect.Top)
	, ExtentX(AlignedRect.Right - AlignedRect.Left, 0.0f)
	, ExtentY(0.0f, AlignedRect.Bottom - AlignedRect.Top)
{
}

FSlateRotatedRect::FSlateRotatedRect(const FVector2D& InTopLeft, const FVector2D& InExtentX, const FVector2D& InExtentY)
	: TopLeft(InTopLeft)
	, ExtentX(InExtentX)
	, ExtentY(InExtentY)
{
}

// !!! WRH 2014/08/25 - this is a brute-force, not efficient implementation, uses a bunch of extra conditionals.
FSlateRect FSlateRotatedRect::ToBoundingRect() const
{
	FVector2D Points[4] = 
	{
		TopLeft,
		TopLeft + ExtentX,
		TopLeft + ExtentY,
		TopLeft + ExtentX + ExtentY
	};
	return FSlateRect(
		FMath::Min(Points[0].X, FMath::Min3(Points[1].X, Points[2].X, Points[3].X)),
		FMath::Min(Points[0].Y, FMath::Min3(Points[1].Y, Points[2].Y, Points[3].Y)),
		FMath::Max(Points[0].X, FMath::Max3(Points[1].X, Points[2].X, Points[3].X)),
		FMath::Max(Points[0].Y, FMath::Max3(Points[1].Y, Points[2].Y, Points[3].Y))
		);
}

bool FSlateRotatedRect::IsUnderLocation(const FVector2D& Location) const
{
	const FVector2D Offset = Location - TopLeft;
	const float Det = FVector2D::CrossProduct(ExtentX, ExtentY);

	// Not exhaustively efficient. Could optimize the checks for [0..1] to short circuit faster.
	const float S = -FVector2D::CrossProduct(Offset, ExtentX) / Det;
	if (FMath::IsWithinInclusive(S, 0.0f, 1.0f))
	{
		const float T = FVector2D::CrossProduct(Offset, ExtentY) / Det;
		return FMath::IsWithinInclusive(T, 0.0f, 1.0f);
	}
	return false;
}

static FVector2D RoundToInt(const FVector2D& Vec)
{
	return FVector2D(FMath::RoundToInt(Vec.X), FMath::RoundToInt(Vec.Y));
}

FSlateRotatedRect FSlateRotatedRect::MakeSnappedRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FSlateLayoutTransform& InverseLayoutTransform, const FSlateRenderTransform& RenderTransform)
{
	FSlateRotatedRect RotatedRect = TransformRect(
		Concatenate(InverseLayoutTransform, RenderTransform),
		FSlateRotatedRect(ClipRectInLayoutWindowSpace));

	// Pixel snapping is done here by rounding the resulting floats to ints, we do this before
	// calculating the final extents of the clip box otherwise we'll get a smaller clip rect than a visual
	// rect where each point is individually snapped.
	FVector2D SnappedTopLeft = RoundToInt(RotatedRect.TopLeft);
	FVector2D SnappedTopRight = RoundToInt(RotatedRect.TopLeft + RotatedRect.ExtentX);
	FVector2D SnappedBottomLeft = RoundToInt(RotatedRect.TopLeft + RotatedRect.ExtentY);

	//NOTE: We explicitly do not re-snap the extent x/y, it wouldn't be correct to snap again in distance space
	// even if two points are snapped, their distance wont necessarily be a whole number if those points are not
	// axis aligned.
	return FSlateRotatedClipRectType(
		SnappedTopLeft,
		SnappedTopRight - SnappedTopLeft,
		SnappedBottomLeft - SnappedTopLeft);
}

FSlateVertex::FSlateVertex() 
{
}

FSlateVertex::FSlateVertex(const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InTexCoord, const FVector2D& InTexCoord2, const FColor& InColor, const FSlateRotatedClipRectType& InClipRect )
	: ClipRect( InClipRect )
	, Color( InColor )
{
	TexCoords[0] = InTexCoord.X;
	TexCoords[1] = InTexCoord.Y;
	TexCoords[2] = InTexCoord2.X;
	TexCoords[3] = InTexCoord2.Y;

	const FVector2D WindowPosition = TransformPoint(RenderTransform, InLocalPosition);

	// Pixel snapping here.
	Position[0] = FMath::RoundToInt(WindowPosition.X);
	Position[1] = FMath::RoundToInt(WindowPosition.Y);
}

FSlateVertex::FSlateVertex( const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InTexCoord, const FColor& InColor, const FSlateRotatedClipRectType& InClipRect )
	: ClipRect( InClipRect )
	, Color( InColor )
{
	TexCoords[0] = InTexCoord.X;
	TexCoords[1] = InTexCoord.Y;
	TexCoords[2] = 1.0f;
	TexCoords[3] = 1.0f;

	const FVector2D WindowPosition = TransformPoint(RenderTransform, InLocalPosition);

	// Pixel snapping here.
	Position[0] = FMath::RoundToInt(WindowPosition.X);
	Position[1] = FMath::RoundToInt(WindowPosition.Y);
}


FSlateVertex::FSlateVertex( const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector4& InTexCoord, const FVector2D& InMaterialTexCoords, const FColor& InColor, const FSlateRotatedClipRectType& InClipRect )
	: ClipRect( InClipRect )
	, Color( InColor )
{

	TexCoords[0] = InTexCoord.X;
	TexCoords[1] = InTexCoord.Y;
	TexCoords[2] = InTexCoord.Z;
	TexCoords[3] = InTexCoord.W;

	MaterialTexCoords[0] = InMaterialTexCoords.X;
	MaterialTexCoords[1] = InMaterialTexCoords.Y;

	const FVector2D WindowPosition = TransformPoint(RenderTransform, InLocalPosition);

	// Pixel snapping here.
	Position[0] = FMath::RoundToInt(WindowPosition.X);
	Position[1] = FMath::RoundToInt(WindowPosition.Y);
}
