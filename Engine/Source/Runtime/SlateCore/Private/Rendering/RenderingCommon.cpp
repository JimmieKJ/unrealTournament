// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "RenderingCommon.h"

FSlateRotatedRect::FSlateRotatedRect() {}

FSlateRotatedRect::FSlateRotatedRect(const FSlateRect& AlignedRect)
	: TopLeft(AlignedRect.GetTopLeft())
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

FSlateRotatedRectHalf::FSlateRotatedRectHalf() {}

FSlateRotatedRectHalf::FSlateRotatedRectHalf(const FSlateRotatedRect& RotatedRect)
	: TopLeft(RotatedRect.TopLeft)
	, ExtentX(RotatedRect.ExtentX)
	, ExtentY(RotatedRect.ExtentY)
{
}

FSlateRotatedRectHalf::FSlateRotatedRectHalf(const FVector2D& InTopLeft, const FVector2D& InExtentX, const FVector2D& InExtentY)
	: TopLeft(InTopLeft)
	, ExtentX(InExtentX)
	, ExtentY(InExtentY)
{
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

